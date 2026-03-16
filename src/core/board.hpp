// =============================================================================
// board.hpp — Board<Spec>: runtime chip owner, buffer manager, auto-wiring
// =============================================================================
//
// Owns the flat memory and chip lifetimes.  Provides pointer access
// into the buffer for each chip, handles dynamic chip add/remove for hot-swap,
// and delegates address-decode wiring to its BusMap<Spec> member.
//
// See bus_map.hpp for the address-decode logic (page tables, MMIO, sub-tables)
// and chip_manifest.hpp for the declarative chip-memory layout.
//
// =============================================================================
#pragma once

#include "core/board_base.hpp"
#include "core/bus_map.hpp"
#include "core/storage/rom_loader.hpp"
#include <cstdio>

// §4  Board<Spec> — runtime chip owner, buffer manager
// =============================================================================
//
// Owns the flat memory and chip lifetimes.  Delegates address-decode
// logic (page table wiring, MMIO handlers, sub-tables) to BusMap<Spec>.
//
// Inherits BoardBase for:
//   - Non-owning component index (chips + ports)
//   - Port ownership (vector<unique_ptr<Port>>)
//
// Template parameter Spec must satisfy BusSpecConcept.
//
// Thread safety: none.  External synchronisation required if add/remove_chip
// or apply() is called concurrently with bus accesses.
//

template<BusSpecConcept Spec>
class Board : public BoardBase {
public:
    using Map         = BusMap<Spec>;
    using Bus         = MemoryBus<Spec>;
    using ChipId      = typename Bus::ChipId;
    using WriteChipId = typename Bus::WriteChipId;
    using SlotRecord  = typename Map::SlotRecord;

    static constexpr size_t kPageSize = Bus::kPageSize;
    static constexpr size_t kPageBits = Spec::PageBits;

    // =====================================================================
    // §4.1  Construction from a ChipManifest
    // =====================================================================
    //
    // Allocates the flat mem to exactly fit all static chips plus the
    // dynamic pool declared in the manifest.  Slot records are built by
    // bus_map_.
    //

    template<size_t N>
    explicit Board(const ChipManifest<N>& manifest)
        : bus_map_(manifest) {
        flat_mem_.assign(manifest.buffer_bytes(kPageBits), uint8_t(0xFF));

        // Precompute per-chip-id byte offsets for chip_buffer().
        const size_t total_ids = manifest.total_ids(kPageBits);
        chip_byte_offsets_.resize(total_ids);
        size_t byte_off = 0;
        ChipId chip_id  = ChipId(0);
        for (size_t i = 0; i < N; ++i) {
            const auto& chip = manifest.chips[i];
            if (chip.size_bytes == 0) continue;
            const size_t eff_bs    = chip.bank_size > 0 ? chip.bank_size : kPageSize;
            const size_t num_banks = chip.size_bytes / eff_bs;
            for (size_t b = 0; b < num_banks; ++b)
                chip_byte_offsets_[chip_id++] = byte_off + b * eff_bs;
            byte_off += chip.size_bytes;
        }
        // Dynamic pool: one bank per page
        for (size_t d = 0; d < manifest.num_dynamic_pages; ++d)
            chip_byte_offsets_[chip_id++] = byte_off + d * kPageSize;

        // Initialise dynamic allocator
        if (manifest.num_dynamic_pages > 0) {
            dynamic_base_ = manifest.dynamic_base_id(kPageBits);
            free_list_.push_back({dynamic_base_, manifest.num_dynamic_pages});
        }
    }

    // =====================================================================
    // §4.2  Chip binding
    // =====================================================================
    //
    // Associates a runtime ChipBase* with a manifest slot.  Delegates slot
    // record updates to bus_map_ and returns a pointer to the chip's region
    // in the flat mem (for MemoryChipBase::bind(), etc.), or nullptr
    // for MMIO-only slots.
    //

    uint8_t* bind_chip(size_t slot_index, ChipBase* chip) {
        bus_map_.bind_chip(slot_index, chip);
        auto& s = bus_map_.slot(slot_index);
        return s.num_pages > 0 ? chip_buffer(s.base_id) : nullptr;
    }

    // =====================================================================
    // §4.3  Auto-wiring: apply()
    // =====================================================================
    //
    // Delegates to BusMap::apply() — programs a MemoryBus viewer from the
    // manifest slots + bound chips.  Safe to call multiple times.
    //

    void apply(Bus& bus, size_t viewer_id = 0) {
        bus_map_.apply(bus, flat_mem_.data(), viewer_id);
    }

    // Delegates to BusMap::build_overlay_snapshots() — generates banking
    // mode snapshots for all viewer × overlay-group combinations.
    // Prerequisites: apply() must have been called on viewer 0 first.

    template<size_t MaxViewers, size_t MaxModes>
    void build_overlay_snapshots(
        Bus& bus,
        size_t num_viewers,
        std::array<std::array<typename Bus::Snapshot, MaxModes>, MaxViewers>& out) const
    {
        bus_map_.build_overlay_snapshots(bus, num_viewers, out);
    }

    // =====================================================================
    // §4.4  Variadic initialization: bind + buffer-bind + apply
    // =====================================================================
    //
    // Binds each chip pointer to its positional manifest slot, auto-calls
    // bind(buffer_ptr) on chips that support it (detected via a C++20
    // requires-expression), then calls apply() to wire page tables,
    // sub-tables, and MMIO handlers.
    //
    // Pass nullptr for unbound slots (e.g. optional ROM not loaded).
    //
    //   board_.initialize(bus_, ram_, monitor_rom_, basic_rom_, &pia_);
    //

    template<typename... Chips>
    void initialize(Bus& bus, Chips*... chips) {
        assert(sizeof...(Chips) <= bus_map_.slot_count());
        size_t idx = 0;
        (bind_one_(idx++, chips), ...);
        apply(bus);
    }

    // =====================================================================
    // §4.4a  Factory-driven chip creation
    // =====================================================================
    //
    // Iterates all manifest slots and calls each slot's factory function to
    // create the chip, bind it to the flat mem, and take ownership.
    // Slots that are already bound (via bind_chip() or initialize()) are
    // skipped — this allows systems to pre-bind MMIO chips that need
    // custom initialization before calling create_chips().
    //
    // Conditional chips:  Slots with condition != 0 are evaluated against
    // the user-supplied ConditionFn callback.  Slots whose condition is not
    // met are skipped (chip remains nullptr).  This enables configuration-
    // dependent chip sets (PAL/NTSC variants, optional RAM expansions, etc.).
    //
    // After create_chips(), call apply(bus) to wire page tables + MMIO.
    //
    // Usage:
    //   board_.bind_chip(kPiaSlot, &pia_);   // pre-bind custom chip
    //   board_.create_chips(&pins_);          // auto-create the rest
    //   board_.apply(bus_);
    //
    // With conditions:
    //   board_.create_chips(&pins_, my_condition_fn, &config_);
    //

    /// Condition evaluation callback — returns true if the chip should be
    /// created.  The condition tag is a system-defined uint16_t; the context
    /// pointer is opaque and typically points to a SystemConfiguration.
    using ConditionFn = bool (*)(uint16_t condition, const void* context);

    void create_chips(const bus_state_t* system_bus,
                      ConditionFn condition_fn = nullptr,
                      const void* condition_ctx = nullptr) {
        for (size_t i = 0; i < bus_map_.slot_count(); ++i) {
            auto& rec = bus_map_.slot(i);
            if (rec.chip) continue;        // Already manually bound
            if (!rec.factory) continue;    // No factory (must be bound manually)

            // Evaluate condition — skip if condition tag is set and not met
            if (rec.condition != 0) {
                if (!condition_fn || !condition_fn(rec.condition, condition_ctx))
                    continue;
            }

            uint8_t* buf = rec.num_pages > 0 ? chip_buffer(rec.base_id) : nullptr;

            // Reconstruct a ChipSlot from the SlotRecord for the factory call.
            ChipSlot slot{rec.base_addr, rec.byte_size,
                          rec.addr_mask, rec.bank_size, 0 /*effective_size*/,
                          rec.overlay_group,
                          rec.factory, rec.label, rec.condition};
            ChipBase* chip = rec.factory(slot, system_bus, buf);

            // Apply placement metadata from the manifest slot.
            // Category is NOT overridden — it comes from the chip's base-class
            // constructor (CpuChipBase → "CPU", VideoChipBase → "Video", etc.).
            if (rec.label) {
                chip->set_display_name(rec.label);
                chip->set_short_name(rec.label);
            }
            chip->set_base_address(static_cast<uint16_t>(rec.base_addr));

            bind_chip(i, chip);
            owned_chips_.emplace_back(chip);

            // Cache the first CPU chip on this board.
            if (auto* as_cpu = dynamic_cast<CpuChipBase*>(chip);
                !cpu_chip_ && as_cpu)
                cpu_chip_ = as_cpu;
        }
    }

    // =====================================================================
    // §4.4b  Typed chip access
    // =====================================================================
    //
    // Returns the chip bound to a manifest slot, cast to the requested type.
    // The caller is responsible for ensuring the slot index and type match.
    //

    template<typename T>
    [[nodiscard]] T* chip_as(size_t slot_index) noexcept {
        return static_cast<T*>(bus_map_.slot(slot_index).chip);
    }

    template<typename T>
    [[nodiscard]] const T* chip_as(size_t slot_index) const noexcept {
        return static_cast<const T*>(bus_map_.slot(slot_index).chip);
    }

    // =====================================================================
    // §4.4b′  Variant chip lookup
    // =====================================================================
    //
    // Returns the first non-null chip among the given slot indices, cast to
    // the requested type.  Useful for configuration-dependent chips where
    // exactly one of several conditional slots was created (e.g. PAL/NTSC
    // video chip variants).
    //
    //   vic_ = board_.first_chip<vic_base_t>({kVicPal, kVicNtsc});
    //

    template<typename T>
    [[nodiscard]] T* first_chip(std::initializer_list<size_t> slot_indices) noexcept {
        for (size_t idx : slot_indices) {
            if (idx < bus_map_.slot_count() && bus_map_.slot(idx).chip)
                return static_cast<T*>(bus_map_.slot(idx).chip);
        }
        return nullptr;
    }

    template<typename T>
    [[nodiscard]] const T* first_chip(std::initializer_list<size_t> slot_indices) const noexcept {
        for (size_t idx : slot_indices) {
            if (idx < bus_map_.slot_count() && bus_map_.slot(idx).chip)
                return static_cast<const T*>(bus_map_.slot(idx).chip);
        }
        return nullptr;
    }

    // =====================================================================
    // §4.4c  Owned chip access (for registration / lifetime)
    // =====================================================================

    [[nodiscard]] std::span<const std::unique_ptr<ChipBase>> owned_chips() const noexcept {
        return owned_chips_;
    }

    // =====================================================================
    // §4.4c′  Component registration
    // =====================================================================
    //
    // Rebuilds the BoardBase component index from owned chips and ports.
    // Call after create_chips() / add_port() to make all components
    // discoverable via find_component().
    //

    void register_board_components() {
        clear_components();
        for (auto& chip : owned_chips_)
            register_component(chip.get());
        for (auto& port : ports_)
            register_component(port.get());
    }

    // =====================================================================
    // §4.4d  Chip lifecycle — bulk reset
    // =====================================================================
    //
    // Calls ChipBase::reset() on every non-null chip bound to a manifest
    // slot.  Safe for all chip types:
    //   - RAM/ROM chips: default no-op (no internal state to clear)
    //   - CPU chips: no-op (CPUs use a separate pin-based reset protocol)
    //   - I/O chips: clears registers, timers, interrupt state
    //
    // Systems with chips that need post-reset callback re-wiring should
    // call reset_chips() first, then apply their chip-specific fixups.
    //

    void reset_chips() noexcept {
        for (const auto& rec : bus_map_.slots()) {
            if (rec.chip)
                rec.chip->reset();
        }
    }

    // =====================================================================
    // §4.5  Buffer and chip access
    // =====================================================================

    // Pointer to the start of the chip's region in the flat mem.
    [[nodiscard]] uint8_t* chip_buffer(ChipId base_id) noexcept {
        return flat_mem_.data() + chip_byte_offsets_[size_t(base_id)];
    }

    [[nodiscard]] const uint8_t* chip_buffer(ChipId base_id) const noexcept {
        return flat_mem_.data() + chip_byte_offsets_[size_t(base_id)];
    }

    // Total size of a chip's buffer region in bytes.
    [[nodiscard]] static constexpr size_t chip_bytes(size_t num_pages) noexcept {
        return num_pages * kPageSize;
    }

    // =====================================================================
    // §4.5  Load ROM/RAM data
    // =====================================================================
    //
    // Copies data into the chip's region.  Returns false if data is larger than
    // the chip's allocated region.  Partial loads (data.size() < chip_bytes)
    // fill only the beginning of the region; the rest retains its value.
    //

    bool load(ChipId base_id, std::span<const uint8_t> data) noexcept {
        const SlotRecord* info = bus_map_.find(base_id);
        if (!info) return false;
        if (data.size() > info->byte_size) return false;
        std::memcpy(chip_buffer(base_id), data.data(), data.size());
        return true;
    }

    // Fill a chip's region with a constant byte (e.g. 0xFF for unpopulated ROM).
    void fill(ChipId base_id, uint8_t value = 0xFF) noexcept {
        const SlotRecord* info = bus_map_.find(base_id);
        if (!info) return;
        std::memset(chip_buffer(base_id), value, info->byte_size);
    }

    // =====================================================================
    // §4.5a  Generic ROM loading from manifest metadata
    // =====================================================================
    //
    // Iterates all manifest slots with non-null RomFileInfo metadata and
    // loads ROM files from the given root directory into their chip buffers.
    // Returns true if all required (non-optional) ROMs loaded successfully.
    //
    // The system is responsible for discovering rom_root via
    // system_config_discover_rom_root() and passing it here.  Slots
    // without RomFileInfo are untouched — systems can load those manually
    // before or after calling this method.
    //

    bool load_roms(const char* rom_root, const char* system_name = nullptr) {
        bool all_ok = true;
        for (size_t i = 0; i < bus_map_.slot_count(); ++i) {
            const auto& rec = bus_map_.slot(i);
            if (!rec.rom.has_rom()) continue; // No ROM metadata on this slot
            if (!rec.chip) continue;          // Chip not created (conditional, skipped)
            if (rec.byte_size == 0) continue; // MMIO-only slot

            uint8_t* buf = chip_buffer(rec.base_id);
            bool ok = rom_loader_load_from_root(
                rom_root, rec.rom.filenames,
                rec.byte_size, buf, rec.byte_size
            );

            const char* label = rec.label ? rec.label : "(unnamed)";
            if (ok) {
                if (system_name)
                    printf("%s: %s loaded (%zu bytes)\n", system_name, label, rec.byte_size);
            } else if (rec.rom.optional) {
                if (system_name)
                    printf("%s: %s not found (optional)\n", system_name, label);
            } else {
                if (system_name)
                    printf("%s: Failed to load %s\n", system_name, label);
                all_ok = false;
            }
        }
        return all_ok;
    }

    // =====================================================================
    // §4.6  Dynamic chip management (hot-swap)
    // =====================================================================
    //
    // add_chip allocates num_pages contiguous pages from the dynamic pool and
    // returns the base chip id.  Returns kInvalidChipId on failure (pool full
    // or fragmented).
    //
    // remove_chip returns the pages to the free list and merges adjacent runs.
    // The caller must remap or unmap the chip in all MemoryBus viewers before
    // or after calling remove_chip.
    //

    static constexpr ChipId kInvalidChipId = ChipId(~ChipId(0));

    [[nodiscard]] ChipId add_chip(std::string_view name,
                                  size_t           num_pages,
                                  bool             read_only = false) noexcept
    {
        if (num_pages == 0) return kInvalidChipId;

        // First-fit search in the free list
        for (auto it = free_list_.begin(); it != free_list_.end(); ++it) {
            if (it->pages >= num_pages) {
                const ChipId base = ChipId(it->base);
                bus_map_.add_slot({
                    name, base, num_pages, 0, 0,
                    read_only, /*dynamic=*/true, nullptr, -1, -1
                });

                // Consume from the free run
                it->base  += num_pages;
                it->pages -= num_pages;
                if (it->pages == 0)
                    free_list_.erase(it);

                return base;
            }
        }
        return kInvalidChipId;  // pool exhausted or too fragmented
    }

    void remove_chip(ChipId base_id) noexcept {
        const SlotRecord* info = bus_map_.find(base_id);
        if (!info || !info->dynamic) return;

        const size_t base   = size_t(info->base_id);
        const size_t npages = info->num_pages;
        bus_map_.remove_slot(base_id);

        // Return pages to free list and merge adjacent runs
        FreeRun run{base, npages};
        free_list_.push_back(run);
        std::sort(free_list_.begin(), free_list_.end(),
            [](const FreeRun& a, const FreeRun& b){ return a.base < b.base; });
        merge_free_list();
    }

    // =====================================================================
    // §4.7  Connect to a MemoryBus (standalone, without full apply)
    // =====================================================================
    //
    // Passes the flat mem pointer to the bus.  Called automatically by
    // apply(), but can also be used standalone for manual page-table setup.
    //

    void connect(Bus& bus) noexcept {
        bus.set_flat_mem(flat_mem_.data());
    }

    // =====================================================================
    // §4.8  Slot lookup
    // =====================================================================

    [[nodiscard]] const SlotRecord* find(ChipId base_id) const noexcept {
        return bus_map_.find(base_id);
    }

    [[nodiscard]] const SlotRecord* find(std::string_view name) const noexcept {
        return bus_map_.find(name);
    }

    [[nodiscard]] SlotRecord& slot(size_t index) noexcept {
        return bus_map_.slot(index);
    }

    [[nodiscard]] const SlotRecord& slot(size_t index) const noexcept {
        return bus_map_.slot(index);
    }

    [[nodiscard]] std::span<const SlotRecord> slots() const noexcept {
        return bus_map_.slots();
    }

    // =====================================================================
    // §4.9  Flat memory introspection
    // =====================================================================

    [[nodiscard]] uint8_t*       flat_mem()       noexcept { return flat_mem_.data(); }
    [[nodiscard]] const uint8_t* flat_mem() const noexcept { return flat_mem_.data(); }
    [[nodiscard]] size_t flat_mem_size() const noexcept { return flat_mem_.size(); }

    // =====================================================================
    // §4.10  Effective size (pre-apply RAM trimming)
    // =====================================================================
    //
    // Limits Phase 1 page mapping for a slot to fewer bytes than the full
    // allocation.  Set before apply() for configuration-dependent sizes
    // (e.g. RAM that can be 2 KB–64 KB).  Phase 0 chip_info still covers
    // the full byte_size so bank switching beyond the visible window works.
    //

    void set_effective_size(size_t slot_index, size_t bytes) noexcept {
        bus_map_.set_effective_size(slot_index, bytes);
    }

    // =====================================================================
    // §4.11  Bank selection (runtime bank switching)
    // =====================================================================
    //
    // Maps one bank from a multi-bank slot to a target page in the address
    // space.  Requires bank_size > 0 on the slot.  Uses fill_read_constant/
    // fill_write_constant for optimal performance (single chip_id per bank).
    //

    void select_bank_at(Bus& bus, size_t viewer_id,
                        size_t slot_idx, size_t bank_idx,
                        size_t target_page) const noexcept {
        bus_map_.select_bank_at(bus, viewer_id, slot_idx, bank_idx, target_page);
    }

    // =====================================================================
    // §4.12  Convenience: manual page mapping
    // =====================================================================
    //
    // Helpers for systems that need manual page-table manipulation beyond
    // what apply() provides (e.g. cartridge bank switching, PLA modes).
    //

    void map_chip_read(Bus& bus, size_t viewer_id,
                       size_t first_page, ChipId base_id) const noexcept {
        bus_map_.map_chip_read(bus, viewer_id, first_page, base_id);
    }

    void map_chip_write(Bus& bus, size_t viewer_id,
                        size_t first_page, ChipId base_id) const noexcept {
        bus_map_.map_chip_write(bus, viewer_id, first_page, base_id);
    }

    void map_chip(Bus& bus, size_t viewer_id,
                  size_t first_page, ChipId base_id) const noexcept {
        bus_map_.map_chip(bus, viewer_id, first_page, base_id);
    }

private:
    // ── Variadic bind helper ─────────────────────────────────────────────
    // Binds one chip to its slot; auto-calls bind(buffer) on types that have it.

    template<typename T>
    void bind_one_(size_t slot_index, T* chip) {
        if (!chip) return;
        uint8_t* buf = bind_chip(slot_index, chip);
        if constexpr (requires(T& t, uint8_t* p) { t.bind(p); }) {
            if (buf) chip->bind(buf);
        }
    }

    // ── Dynamic allocator internal state ─────────────────────────────────

    struct FreeRun {
        size_t base;
        size_t pages;
    };

    void merge_free_list() noexcept {
        // Assumes free_list_ is sorted by base; merges adjacent runs.
        for (size_t i = 0; i + 1 < free_list_.size(); ) {
            FreeRun& cur  = free_list_[i];
            FreeRun& next = free_list_[i + 1];
            if (cur.base + cur.pages == next.base) {
                cur.pages += next.pages;
                free_list_.erase(free_list_.begin() + static_cast<ptrdiff_t>(i + 1));
            } else {
                ++i;
            }
        }
    }

    // ── State ─────────────────────────────────────────────────────────────

    Map                      bus_map_;
    std::vector<uint8_t>     flat_mem_;
    std::vector<size_t>      chip_byte_offsets_; // ChipId → byte offset in flat_mem

    // Chips created by create_chips() — owned here for lifetime management.
    std::vector<std::unique_ptr<ChipBase>> owned_chips_;

    size_t               dynamic_base_ = 0;
    std::vector<FreeRun> free_list_;
};


// =============================================================================
// §5  Usage examples
// =============================================================================
//
// ── Apple 1 (minimal system) ──────────────────────────────────────────────────
//
//  inline constexpr auto kApple1Chips = make_chip_manifest(
//      Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
//      Slot<ROMChip>{0xFF00,   256, 0, "Monitor ROM"},
//      Slot<ROMChip>{0xE000,  4096, 0, "BASIC ROM"},
//      Slot<pia6820_t>{0xD010, 0, 0xFFFC, "PIA"} // MMIO-only, 4-byte window
//  );
//
//  // BusSpec auto-derived: MaxChipId=272, 1 MMIO handler, 1 MaskedSubTable
//  using Apple1Spec = ManifestBusSpec<kApple1Chips, 16, 8>;
//
//  // At runtime:
//  Board<Apple1Spec> mem{kApple1Chips};
//  MemoryBus<Apple1Spec> bus;
//
//  // Single call: bind all chips, auto-call MemoryChipBase::bind(), wire bus
//  mem.initialize(bus, ram_, monitor_rom_, basic_rom_, &pia_);
//
// ── C64 (complex system with indexed sub-table) ──────────────────────────────
//
//  inline constexpr auto kC64Chips = make_chip_manifest(
//      Slot<ROMChip>{0x8000,  4096, 0, "ROML"},
//      Slot<ROMChip>{0xA000,  4096, 0, "ROMH"},
//      Slot<ROMChip>{0xE000,  8192, 0, "KERNAL"},
//      Slot<ROMChip>{0xA000,  8192, 0, "BASIC"},
//      Slot<ROMChip>{0xD000,  8192, 0, "CHARROM"}, // VIC-II only
//      Slot<RAMChip>{0x0000, 65536, 0, "RAM"}
//  );
//
//  // C64 uses a hand-written BusSpec due to indexed sub-tables for the I/O
//  // page at $D000 and dual viewers (CPU + VIC-II).  ManifestBusSpec handles
//  // the common case; complex systems extend it or write their own spec.
//
// ── NES cartridge hot-swap ────────────────────────────────────────────────────
//
//  inline constexpr auto kNesChips = make_chip_manifest(
//      Slot<RAMChip>{0x0000, 2048, 0, "WRAM"},"
//      Slot<RAMChip>{0x2000, 2048, 0, "CIRAM"},  // nametable RAM
//      Slot<RAMChip>{0x6000, 8192, 0, "SRAM"}    // battery-backed RAM
//  ).with_dynamic_pool(256);
//
//  // Dynamic cartridge insertion at runtime:
//  auto cart_id = mem.add_chip("PRG-ROM", 256, /*read_only=*/true);
//  mem.load(cart_id, prg_data);
//
// ── VIC-20 (non-bus chips + conditional variants) ─────────────────────────────
//
//  namespace vic20_cond {
//      inline constexpr uint16_t kPAL  = 1;
//      inline constexpr uint16_t kNTSC = 2;
//  }
//
//  inline constexpr auto kVIC20Chips = make_chip_manifest(
//      // Bus-mapped memory chips
//      Slot<RAMChip>  {0x0000, 65536, 0, "RAM"},
//      Slot<ROMChip>  {0x8000,  4096, 0, "CHARROM"},
//      Slot<ROMChip>  {0xC000,  8192, 0, "BASIC ROM"},
//      Slot<ROMChip>  {0xE000,  8192, 0, "KERNAL ROM"},
//      // Non-bus chips: size_bytes=0, no addr_mask → factory-created but not mapped
//      // Category is auto-derived from each chip's base class at construction time.
//      Slot<MOS6502>  {0, 0, 0, "MOS 6502"},
//      Slot<mos6561_t>{0, 0, 0, "MOS 6561 (PAL)",  vic20_cond::kPAL},
//      Slot<mos6560_t>{0, 0, 0, "MOS 6560 (NTSC)", vic20_cond::kNTSC},
//      Slot<mos6522_t>{0, 0, 0, "VIA 1"},
//      Slot<mos6522_t>{0, 0, 0, "VIA 2"}
//  );
//
//  // Condition callback — evaluates condition tags against system config:
//  static bool vic20_condition(uint16_t tag, const void* ctx) {
//      auto* config = static_cast<const SystemConfiguration*>(ctx);
//      switch (tag) {
//          case vic20_cond::kPAL:  return config->region_option_index <= 0;
//          case vic20_cond::kNTSC: return config->region_option_index > 0;
//      }
//      return true;
//  }
//
//  // At runtime — all chips created from manifest:
//  board_.create_chips(&bus_.state, vic20_condition, &config_);
//  board_.apply(mem_bus_);
//
//  // Typed access — exact slot or variant lookup:
//  cpu_ = board_.chip_as<MOS6502>(kCpu);
//  vic_ = board_.first_chip<vic_base_t>({kVicPal, kVicNtsc});
//
// =============================================================================
