// =============================================================================
// board.hpp — Board<Spec>: runtime chip owner, buffer manager, auto-wiring
// =============================================================================
//
// Owns the flat unified buffer.  Provides pointer access into the buffer for
// each chip, handles dynamic chip add/remove for hot-swap, and can auto-wire
// a MemoryBus from the manifest + bound chips.
//
// See chip_manifest.hpp for the declarative chip-memory layout (ChipSlot,
// ChipManifest<N>, ManifestBusSpec) that feeds into Board<Spec>.
//
// =============================================================================
#pragma once

#include "core/chip_manifest.hpp"
#include "core/memory_bus.hpp"

// §4  Board<Spec> — runtime chip owner, buffer manager, auto-wiring
// =============================================================================
//
// Owns the flat unified buffer.  Provides pointer access into the buffer for
// each chip, handles dynamic chip add/remove for hot-swap, and can auto-wire
// a MemoryBus from the manifest + bound chips.
//
// Template parameter Spec must satisfy BusSpecConcept.
//
// Thread safety: none.  External synchronisation required if add/remove_chip
// or apply() is called concurrently with bus accesses.
//

template<BusSpecConcept Spec>
class Board {
public:
    using Bus         = MemoryBus<Spec>;
    using ChipId      = typename Bus::ChipId;
    using WriteChipId = typename Bus::WriteChipId;
    using PT          = PackingTraits<Spec>;
    using Addr        = typename Spec::AddrType;

    static constexpr size_t kPageSize = Bus::kPageSize;
    static constexpr size_t kPageBits = Spec::PageBits;

    // ── Per-slot runtime record ───────────────────────────────────────────

    struct SlotRecord {
        std::string_view name;                // From bound chip (or provided for dynamic)
        ChipId           base_id;             // First chip id (prefix sum)
        size_t           num_pages;           // Buffer pages (0 = MMIO-only)
        uint32_t         base_addr  = 0;      // Default address in bus space
        uint32_t         addr_mask  = 0;      // Sub-page decode mask
        bool             read_only  = false;  // Derived from bound chip
        bool             dynamic    = false;  // Runtime-added (can be removed)
        ChipBase*        chip       = nullptr; // Bound chip instance
        int              mmio_idx   = -1;     // Assigned MMIO handler index (-1 = none)
        int              sub_table_idx = -1;  // Assigned MaskedSubTable index (-1 = none)

        // Factory and label — copied from ChipSlot at construction.
        // Used by create_chips() to auto-create chip instances.
        ChipSlot::FactoryFn factory   = nullptr;
        const char*         label     = nullptr;
        uint16_t            condition = 0;
    };

    // =====================================================================
    // §4.1  Construction from a ChipManifest
    // =====================================================================
    //
    // Allocates the unified buffer to exactly fit all static chips plus the
    // dynamic pool declared in the manifest.
    //

    template<size_t N>
    explicit Board(const ChipManifest<N>& manifest) {
        const size_t total = manifest.total_pages(kPageBits);
        buffer_.assign(total * kPageSize, uint8_t(0xFF));  // default: 0xFF = pulled-high

        slots_.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            const ChipSlot& s = manifest.chips[i];
            slots_.push_back({
                {},                             // name: set during bind_chip
                ChipId(manifest.base_id(i, kPageBits)),
                s.pages(kPageBits),
                s.base_addr,
                s.addr_mask,
                false,                          // read_only: set during bind_chip
                false,                          // dynamic
                nullptr,                        // chip
                -1,                             // mmio_idx
                -1,                             // sub_table_idx
                s.factory,
                s.label,
                s.condition,
            });
        }

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
    // Associates a runtime ChipBase* with a manifest slot.  Derives the
    // chip's name and read_only from its virtual interface.  Assigns the
    // bus chip id on the chip.
    //
    // Returns a pointer to the chip's region in the unified buffer
    // (for MemoryChipBase::bind(), etc.), or nullptr for MMIO-only slots.
    //

    uint8_t* bind_chip(size_t slot_index, ChipBase* chip) {
        assert(slot_index < slots_.size());
        auto& slot = slots_[slot_index];
        slot.chip = chip;
        if (chip) {
            slot.name      = chip->chip_info().part_number;
            slot.read_only = chip->is_read_only();
            chip->set_bus_chip_id(uint16_t(slot.base_id));
        }
        return slot.num_pages > 0 ? chip_buffer(slot.base_id) : nullptr;
    }

    // =====================================================================
    // §4.3  Auto-wiring: apply()
    // =====================================================================
    //
    // Programs a MemoryBus viewer from the manifest slots + bound chips:
    //   - Resets the viewer and its masked sub-tables.
    //   - Maps buffer chips (read pages for all, write pages for non-read-only).
    //   - Registers MMIO handlers for chips with has_mmio().
    //   - Auto-creates MaskedSubTables for sub-page MMIO regions.
    //
    // Safe to call multiple times (e.g. after RAM size reconfiguration).
    // MMIO handlers are re-used on subsequent calls (update, not re-register).
    //

    void apply(Bus& bus, size_t viewer_id = 0) {
        bus.set_unified_buffer(buffer_.data());
        bus.reset_viewer(viewer_id);
        if constexpr (Bus::kHasMaskedSub) bus.reset_masked_subs(viewer_id);

        // ── Phase 1: Map buffer chips ─────────────────────────────────────
        //
        // Clip num_pages to the address space so that bank-switching pools
        // (buffer larger than the visible window) don't overflow the page
        // table.  The system's configure_bus_memory_map() remaps banks later.
        //
        static constexpr size_t kNumPages = Bus::kNumPages;
        for (const auto& slot : slots_) {
            if (slot.num_pages == 0 || slot.dynamic) continue;
            const size_t first_page = slot.base_addr >> kPageBits;
            const size_t mappable   = std::min(slot.num_pages, kNumPages - first_page);
            bus.fill_read_pages(viewer_id, first_page, mappable, slot.base_id);
            if (!slot.read_only) {
                bus.fill_write_pages(viewer_id, first_page, mappable,
                                     WriteChipId(slot.base_id));
            }
        }

        // ── Phase 2: MMIO handlers and sub-tables ─────────────────────────
        if constexpr (Spec::EnableMmio) {
            // Track MaskedSubTable assignments for page deduplication.
            struct SubInfo { size_t page; int sub_idx; };
            static constexpr size_t kMaxSubs =
                Bus::kHasMaskedSub ? Bus::kMaxMaskedSubs : 1;
            std::array<SubInfo, kMaxSubs> sub_infos{};
            size_t num_subs = 0;

            for (auto& slot : slots_) {
                if (!slot.chip || !slot.chip->has_mmio() || slot.dynamic)
                    continue;

                // Register (or update) MMIO handler
                MmioHandler handler{
                    slot.chip,
                    mmio_read_trampoline_,
                    mmio_write_trampoline_
                };
                if (slot.mmio_idx >= 0) {
                    bus.update_handler(size_t(slot.mmio_idx), handler);
                } else {
                    slot.mmio_idx = bus.register_handler(handler);
                    assert(slot.mmio_idx >= 0);
                }

                if constexpr (Bus::kHasMaskedSub) {
                    if (slot.addr_mask != 0) {
                        // Sub-page MMIO — route through a MaskedSubTable
                        const size_t page = slot.base_addr >> kPageBits;

                        // Find existing sub-table for this page, or create one
                        int sub_idx = -1;
                        for (size_t k = 0; k < num_subs; ++k) {
                            if (sub_infos[k].page == page) {
                                sub_idx = sub_infos[k].sub_idx;
                                break;
                            }
                        }
                        if (sub_idx < 0) {
                            // Capture the page's current base chip before overwriting
                            auto base_rd = bus.viewer(viewer_id).read_chip(page);
                            auto base_wr = bus.viewer(viewer_id).write_chip(page);
                            sub_idx = bus.add_masked_sub_table(
                                viewer_id, base_rd, base_wr);
                            assert(sub_idx >= 0);
                            bus.map_to_masked_sub(
                                viewer_id, page, size_t(sub_idx));
                            sub_infos[num_subs++] = {page, sub_idx};
                        }

                        slot.sub_table_idx = sub_idx;

                        bus.add_masked_region(
                            viewer_id, size_t(sub_idx),
                            Addr(slot.addr_mask),
                            Addr(slot.base_addr & slot.addr_mask),
                            ChipId(size_t(PT::kRegChipBase) + size_t(slot.mmio_idx)),
                            WriteChipId(size_t(PT::kRegChipBaseWrite)
                                        + size_t(slot.mmio_idx)));
                        continue;  // skip full-page fallback
                    }
                }

                // Full-page MMIO (or no masked sub-table support)
                const size_t first_page = slot.base_addr >> kPageBits;
                const size_t count = slot.num_pages > 0 ? slot.num_pages : 1;
                bus.map_register_file(
                    viewer_id, first_page, count, size_t(slot.mmio_idx));
            }
        }
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
    //   bus_mem_.initialize(bus_, ram_, monitor_rom_, basic_rom_, &pia_);
    //

    template<typename... Chips>
    void initialize(Bus& bus, Chips*... chips) {
        assert(sizeof...(Chips) <= slots_.size());
        size_t idx = 0;
        (bind_one_(idx++, chips), ...);
        apply(bus);
    }

    // =====================================================================
    // §4.4a  Factory-driven chip creation
    // =====================================================================
    //
    // Iterates all manifest slots and calls each slot's factory function to
    // create the chip, bind it to the unified buffer, and take ownership.
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
    //   bus_mem_.bind_chip(kPiaSlot, &pia_);   // pre-bind custom chip
    //   bus_mem_.create_chips(&pins_);          // auto-create the rest
    //   bus_mem_.apply(bus_);
    //
    // With conditions:
    //   bus_mem_.create_chips(&pins_, my_condition_fn, &config_);
    //

    /// Condition evaluation callback — returns true if the chip should be
    /// created.  The condition tag is a system-defined uint16_t; the context
    /// pointer is opaque and typically points to a SystemConfiguration.
    using ConditionFn = bool (*)(uint16_t condition, const void* context);

    void create_chips(const bus_state_t* system_bus,
                      ConditionFn condition_fn = nullptr,
                      const void* condition_ctx = nullptr) {
        for (size_t i = 0; i < slots_.size(); ++i) {
            auto& rec = slots_[i];
            if (rec.chip) continue;        // Already manually bound
            if (!rec.factory) continue;    // No factory (must be bound manually)

            // Evaluate condition — skip if condition tag is set and not met
            if (rec.condition != 0) {
                if (!condition_fn || !condition_fn(rec.condition, condition_ctx))
                    continue;
            }

            uint8_t* buf = rec.num_pages > 0 ? chip_buffer(rec.base_id) : nullptr;

            // Reconstruct a ChipSlot from the SlotRecord for the factory call.
            ChipSlot slot{rec.base_addr, rec.num_pages * kPageSize,
                          rec.addr_mask, rec.factory, rec.label,
                          rec.condition};
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
        assert(slot_index < slots_.size());
        return static_cast<T*>(slots_[slot_index].chip);
    }

    template<typename T>
    [[nodiscard]] const T* chip_as(size_t slot_index) const noexcept {
        assert(slot_index < slots_.size());
        return static_cast<const T*>(slots_[slot_index].chip);
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
    //   vic_ = bus_mem_.first_chip<vic_base_t>({kVicPal, kVicNtsc});
    //

    template<typename T>
    [[nodiscard]] T* first_chip(std::initializer_list<size_t> slot_indices) noexcept {
        for (size_t idx : slot_indices) {
            if (idx < slots_.size() && slots_[idx].chip)
                return static_cast<T*>(slots_[idx].chip);
        }
        return nullptr;
    }

    template<typename T>
    [[nodiscard]] const T* first_chip(std::initializer_list<size_t> slot_indices) const noexcept {
        for (size_t idx : slot_indices) {
            if (idx < slots_.size() && slots_[idx].chip)
                return static_cast<const T*>(slots_[idx].chip);
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
        for (const auto& rec : slots_) {
            if (rec.chip)
                rec.chip->reset();
        }
    }

    // =====================================================================
    // §4.5  Buffer and chip access
    // =====================================================================

    // Pointer to the start of the chip's region in the unified buffer.
    [[nodiscard]] uint8_t* chip_buffer(ChipId base_id) noexcept {
        return buffer_.data() + size_t(base_id) * kPageSize;
    }

    [[nodiscard]] const uint8_t* chip_buffer(ChipId base_id) const noexcept {
        return buffer_.data() + size_t(base_id) * kPageSize;
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
        const SlotRecord* info = find(base_id);
        if (!info) return false;
        const size_t capacity = info->num_pages * kPageSize;
        if (data.size() > capacity) return false;
        std::memcpy(chip_buffer(base_id), data.data(), data.size());
        return true;
    }

    // Fill a chip's region with a constant byte (e.g. 0xFF for unpopulated ROM).
    void fill(ChipId base_id, uint8_t value = 0xFF) noexcept {
        const SlotRecord* info = find(base_id);
        if (!info) return;
        std::memset(chip_buffer(base_id), value, info->num_pages * kPageSize);
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
                slots_.push_back({
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
        auto it = std::find_if(slots_.begin(), slots_.end(),
            [base_id](const SlotRecord& s){ return s.base_id == base_id; });
        if (it == slots_.end() || !it->dynamic) return;

        const size_t base   = size_t(it->base_id);
        const size_t npages = it->num_pages;
        slots_.erase(it);

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
    // Passes the unified buffer pointer to the bus.  Called automatically by
    // apply(), but can also be used standalone for manual page-table setup.
    //

    void connect(Bus& bus) noexcept {
        bus.set_unified_buffer(buffer_.data());
    }

    // =====================================================================
    // §4.8  Slot lookup
    // =====================================================================

    [[nodiscard]] const SlotRecord* find(ChipId base_id) const noexcept {
        for (const auto& s : slots_)
            if (s.base_id == base_id) return &s;
        return nullptr;
    }

    [[nodiscard]] const SlotRecord* find(std::string_view name) const noexcept {
        for (const auto& s : slots_)
            if (s.name == name) return &s;
        return nullptr;
    }

    [[nodiscard]] SlotRecord& slot(size_t index) noexcept {
        assert(index < slots_.size());
        return slots_[index];
    }

    [[nodiscard]] const SlotRecord& slot(size_t index) const noexcept {
        assert(index < slots_.size());
        return slots_[index];
    }

    [[nodiscard]] std::span<const SlotRecord> slots() const noexcept {
        return slots_;
    }

    // =====================================================================
    // §4.9  Buffer introspection
    // =====================================================================

    [[nodiscard]] uint8_t*       buffer()       noexcept { return buffer_.data(); }
    [[nodiscard]] const uint8_t* buffer() const noexcept { return buffer_.data(); }
    [[nodiscard]] size_t buffer_size() const noexcept { return buffer_.size(); }

    // =====================================================================
    // §4.10  Convenience: manual page mapping
    // =====================================================================
    //
    // Helpers for systems that need manual page-table manipulation beyond
    // what apply() provides (e.g. cartridge bank switching, PLA modes).
    //

    void map_chip_read(Bus& bus, size_t viewer_id,
                       size_t first_page, ChipId base_id) const noexcept {
        const SlotRecord* info = find(base_id);
        if (!info) return;
        bus.fill_read_pages(viewer_id, first_page, info->num_pages, base_id);
    }

    void map_chip_write(Bus& bus, size_t viewer_id,
                        size_t first_page, ChipId base_id) const noexcept {
        const SlotRecord* info = find(base_id);
        if (!info) return;
        bus.fill_write_pages(viewer_id, first_page, info->num_pages,
                             WriteChipId(base_id));
    }

    void map_chip(Bus& bus, size_t viewer_id,
                  size_t first_page, ChipId base_id) const noexcept {
        const SlotRecord* info = find(base_id);
        if (!info) return;
        bus.fill_pages(viewer_id, first_page, info->num_pages,
                       base_id, WriteChipId(base_id));
    }

private:
    // ── MMIO dispatch trampolines ────────────────────────────────────────
    // Bridge ChipBase virtual on_bus_read/write to MmioHandler function ptrs.

    static bus_state_t mmio_read_trampoline_(void* ctx, bus_state_t bus) noexcept {
        return static_cast<ChipBase*>(ctx)->on_bus_read(bus);
    }

    static bus_state_t mmio_write_trampoline_(void* ctx, bus_state_t bus) noexcept {
        return static_cast<ChipBase*>(ctx)->on_bus_write(bus);
    }

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

    std::vector<uint8_t>     buffer_;
    std::vector<SlotRecord>  slots_;

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
//      Slot<RAMChip>{0x0000, 65536},        // RAM: 64 KB
//      Slot<ROMChip>{0xFF00,   256},        // Monitor ROM: 256 bytes at $FF00
//      Slot<ROMChip>{0xE000,  4096},        // BASIC ROM: 4 KB at $E000
//      Slot<pia6820_t> {0xD010,     0, 0xFFFC} // PIA: MMIO-only, 4-byte window
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
//      Slot<ROMChip>{0x8000,  4096},       // ROML:    4 KB at $8000
//      Slot<ROMChip>{0xA000,  4096},       // ROMH:    4 KB at $A000
//      Slot<ROMChip>{0xE000,  8192},       // KERNAL:  8 KB at $E000
//      Slot<ROMChip>{0xA000,  8192},       // BASIC:   8 KB at $A000
//      Slot<ROMChip>{0xD000,  8192},       // CHARROM: 8 KB (VIC-II only)
//      Slot<RAMChip>{0x0000, 65536}        // RAM:    64 KB at $0000
//  );
//
//  // C64 uses a hand-written BusSpec due to indexed sub-tables for the I/O
//  // page at $D000 and dual viewers (CPU + VIC-II).  ManifestBusSpec handles
//  // the common case; complex systems extend it or write their own spec.
//
// ── NES cartridge hot-swap ────────────────────────────────────────────────────
//
//  inline constexpr auto kNesChips = make_chip_manifest(
//      Slot<RAMChip>{0x0000, 2048},       // WRAM:  2 KB at $0000
//      Slot<RAMChip>{0x2000, 2048},       // CIRAM: 2 KB nametable RAM
//      Slot<RAMChip>{0x6000, 8192}        // SRAM:  8 KB battery-backed RAM
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
//  bus_mem_.create_chips(&bus_.state, vic20_condition, &config_);
//  bus_mem_.apply(mem_bus_);
//
//  // Typed access — exact slot or variant lookup:
//  cpu_ = bus_mem_.chip_as<MOS6502>(kCpu);
//  vic_ = bus_mem_.first_chip<vic_base_t>({kVicPal, kVicNtsc});
//
// =============================================================================
