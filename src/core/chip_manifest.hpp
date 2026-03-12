// =============================================================================
// chip_manifest.hpp — Chip manifest, unified buffer ownership, auto-wiring
// =============================================================================
//
// Declarative chip-memory layout for emulated systems.  A constexpr manifest
// describes WHERE each chip lives in address space and HOW BIG it is; the
// framework derives everything else automatically:
//
//   ChipSlot          — one memory-mapped region (size, address, mask)
//
//   ChipManifest<N>   — compile-time array of N slots.  Assigns chip ids as
//                       prefix sums of page counts.  Provides constexpr queries
//                       for auto-deriving a BusSpec (MMIO count, sub-tables).
//
//   ManifestBusSpec   — auto-derived BusSpec from a constexpr manifest.
//                       Systems declare a manifest and get a BusSpec for free:
//                         using Spec = ManifestBusSpec<kMyChips, 16, 8>;
//
//   BusMemory<Spec>   — runtime owner of the unified buffer.  After binding
//                       runtime ChipBase* instances to slots, a single apply()
//                       call programs all page tables, creates sub-tables, and
//                       registers MMIO handlers automatically.
//
// Design principles:
//   - Name and read_only are derived from ChipBase at runtime, not declared
//     in the manifest.  The manifest carries only layout: sizes and addresses.
//   - MMIO capability is detected via ChipBase::has_mmio() at bind time.
//   - Sub-tables are auto-created for sub-page MMIO regions.
//   - Banking mode switches remain explicit (ModeSnapshot / manual).
//
// Usage model:
//
//   1.  Define a constexpr ChipManifest using typed Slot<T> entries.
//   2.  Derive a BusSpec: using Spec = ManifestBusSpec<kManifest, 16, 8>;
//   3.  Construct a BusMemory<Spec> from the manifest; it allocates the buffer.
//   4.  Bind ChipBase* instances via initialize(bus, chips...) — one call
//       binds all slots, auto-calls MemoryChipBase::bind(), and wires the bus.
//   5.  Load ROM/RAM content via mem.load() or mem.chip_buffer().
//   6.  For banking: save/load snapshots after apply() has set up the default.
//
// =============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "chip.h"
#include "memory_bus.hpp"


// =============================================================================
// §1  ChipSlot — compile-time description of one memory-mapped region
// =============================================================================
//
// Each slot describes WHERE a chip lives and HOW BIG its buffer is.
// Properties like name, read_only, and MMIO capability are derived from the
// ChipBase subclass bound at runtime.
//
//   size_bytes — chip buffer size in bytes; must be 0 or a power of two.
//                0 means MMIO-only (no buffer allocation).
//   base_addr  — default base address in the bus address space
//   addr_mask  — sub-page address decode mask (0 = full-page decode)
//                When non-zero, (addr & addr_mask) == (base_addr & addr_mask)
//                selects this chip.  Triggers MaskedSubTable auto-creation.
//

struct ChipSlot {
    uint32_t base_addr  = 0;
    size_t   size_bytes = 0;
    uint32_t addr_mask  = 0;

    // Factory — creates a chip of the type declared in the corresponding
    // Slot<T>.  Stored by make_chip_manifest() and called by
    // BusMemory::create_chips().  nullptr means the slot must be manually
    // bound before create_chips() (e.g. for chips that need system-specific
    // initialization).
    using FactoryFn = ChipBase* (*)(const ChipSlot& slot,
                                    const bus_state_t* system_bus,
                                    uint8_t* buffer);
    FactoryFn factory = nullptr;

    // Human-readable role label — becomes the chip's short_name in the
    // Hardware menu (e.g. "Work RAM", "KERNAL", "PIA").  nullptr is fine;
    // the chip's own part_number is used as fallback.
    const char* label = nullptr;

    // Condition tag for configuration-dependent chips.  0 means the chip is
    // always present (unconditional).  Values >0 are system-defined and
    // evaluated at runtime by a ConditionFn callback passed to
    // create_chips().  Use this for PAL/NTSC variants, optional RAM
    // expansions, optional sound chips, etc.
    uint16_t condition = 0;

    // Page count for a given page size (size_bytes >> page_bits).
    [[nodiscard]] constexpr size_t pages(size_t page_bits) const noexcept {
        return size_bytes >> page_bits;
    }

    // True when the chip has no buffer (MMIO-only).
    [[nodiscard]] constexpr bool is_mmio_only() const noexcept {
        return size_bytes == 0;
    }
};

// Typed slot wrapper — carries a chip type for expressive manifest declarations.
// The type parameter provides:
//   - Compile-time automation (auto MemoryChipBase::bind() detection)
//   - Factory resolution: make_chip_manifest() stores a factory function
//     pointer derived from T via the SlotCreatable concept.
//
// Category is NOT declared here — it is derived at runtime from the chip's
// base-class constructor (CpuChipBase → "CPU", VideoChipBase → "Video",
// etc.).  See ChipBase::category().
template<typename T>
struct Slot {
    uint32_t    base_addr  = 0;
    size_t      size_bytes = 0;
    uint32_t    addr_mask  = 0;
    const char* label      = nullptr;
    uint16_t    condition  = 0;
};


// =============================================================================
// §1.1  Slot factory resolution
// =============================================================================
//
// Each chip type can opt into automatic creation by providing a static
// create_from_slot() method (the SlotCreatable concept).  Types without it
// fall back to default construction (if available); otherwise the factory is
// nullptr and the slot must be manually bound.
//
// The resolved factory pointer is stored in ChipSlot::factory by
// make_chip_manifest() — a constexpr-compatible function pointer.
//

/// Concept: T provides a static factory suitable for ChipSlot::FactoryFn.
template<typename T>
concept SlotCreatable = requires(const ChipSlot& s, const bus_state_t* b, uint8_t* buf) {
    { T::create_from_slot(s, b, buf) } -> std::convertible_to<ChipBase*>;
};

/// Default factory for types without create_from_slot — default-constructs T.
template<typename T>
    requires std::is_default_constructible_v<T>
ChipBase* default_slot_factory(const ChipSlot& /*slot*/,
                               const bus_state_t* /*system_bus*/,
                               uint8_t* /*buffer*/) {
    return new T();
}

/// Resolve the factory function pointer for a given chip type T.
/// Priority: T::create_from_slot > default construction > nullptr.
template<typename T>
constexpr ChipSlot::FactoryFn resolve_slot_factory() {
    if constexpr (SlotCreatable<T>) {
        return &T::create_from_slot;
    } else if constexpr (std::is_default_constructible_v<T>) {
        return &default_slot_factory<T>;
    } else {
        return nullptr;
    }
}


// =============================================================================
// §2  ChipManifest<N> — compile-time chip-id assignment
// =============================================================================
//
// Chip ids are assigned as the prefix sum of page counts across the array.
// A chip of M pages occupies ids [base_id, base_id + M - 1].
//
// The hot-path formula for a chip_id → buffer offset is:
//   offset = (chip_id << PageBits) | page_local_offset
// which requires that the first page of chip K sits at the page-granular
// position sum(chips[0..K-1].pages()) in the unified buffer — exactly
// what this assignment guarantees.
//
// An optional dynamic pool of num_dynamic_pages pages is appended at the end
// of the buffer.  BusMemory uses this pool for runtime chip addition (hot-swap).
//
// Constexpr query methods enable ManifestBusSpec to auto-derive all BusSpec
// fields from the manifest alone.
//
// Example — Apple 1:
//
//   inline constexpr auto kApple1Chips = make_chip_manifest(
//       Slot<RAMChip>{0x0000, 65536},        // RAM: 64 KB at $0000
//       Slot<ROMChip>{0xFF00,   256},        // Monitor ROM: 256 bytes at $FF00
//       Slot<ROMChip>{0xE000,  4096},        // BASIC ROM: 4 KB at $E000
//       Slot<pia6820_t> {0xD010,     0, 0xFFFC} // PIA: MMIO-only, 4-byte window
//   );
//
//   // With PageBits = 8 (256-byte pages):
//   constexpr auto kRamId      = kApple1Chips.base_id(0, 8);   // = 0
//   constexpr auto kMonitorId  = kApple1Chips.base_id(1, 8);   // = 256
//   constexpr auto kBasicId    = kApple1Chips.base_id(2, 8);   // = 257
//   // PIA has no buffer pages → no chip id assignment
//

template<size_t N>
struct ChipManifest {
    std::array<ChipSlot, N> chips{};
    size_t num_dynamic_pages = 0;  // reserved for runtime-added chips

    // ── Chip-id assignment ─────────────────────────────────────────────────
    //
    // All page-count-dependent queries take page_bits so that manifests
    // declared with size_bytes remain page-size-independent.
    //

    // Base chip id of chip at index chip_index (0-based).
    [[nodiscard]] constexpr size_t base_id(size_t chip_index, size_t page_bits) const noexcept {
        size_t id = 0;
        for (size_t i = 0; i < chip_index; ++i)
            id += chips[i].pages(page_bits);
        return id;
    }

    // Total pages occupied by all static chips.
    [[nodiscard]] constexpr size_t static_pages(size_t page_bits) const noexcept {
        return base_id(N, page_bits);
    }

    // First chip id in the dynamic pool (= one past the last static chip id).
    [[nodiscard]] constexpr size_t dynamic_base_id(size_t page_bits) const noexcept {
        return static_pages(page_bits);
    }

    // Total pages in the unified buffer (static + dynamic pool).
    [[nodiscard]] constexpr size_t total_pages(size_t page_bits) const noexcept {
        return static_pages(page_bits) + num_dynamic_pages;
    }

    // Highest chip id that will ever appear in a page table.
    // Set MaxChipId in your BusSpec to this value.
    [[nodiscard]] constexpr size_t max_chip_id(size_t page_bits) const noexcept {
        const size_t tp = total_pages(page_bits);
        return tp > 0 ? tp - 1 : 0;
    }

    // Buffer size in bytes for a given page size.
    [[nodiscard]] constexpr size_t buffer_bytes(size_t page_bits) const noexcept {
        return total_pages(page_bits) << page_bits;
    }

    // Convenience: number of static chips.
    [[nodiscard]] static constexpr size_t num_chips() noexcept { return N; }

    // ── BusSpec derivation helpers ─────────────────────────────────────────

    // Count of MMIO-only slots (size_bytes == 0).
    // These will need MmioHandler registration during apply().
    [[nodiscard]] constexpr size_t mmio_slot_count() const noexcept {
        size_t count = 0;
        for (size_t i = 0; i < N; ++i) {
            if (chips[i].is_mmio_only())
                ++count;
        }
        return count;
    }

    // Count of unique pages requiring a MaskedSubTable.
    // A sub-table is needed when an MMIO-only slot has addr_mask != 0,
    // meaning it occupies a sub-page region carved from the page's base chip.
    [[nodiscard]] constexpr size_t masked_sub_count(size_t page_bits) const noexcept {
        const size_t page_size = size_t(1) << page_bits;
        size_t count = 0;
        for (size_t i = 0; i < N; ++i) {
            if (chips[i].is_mmio_only() && chips[i].addr_mask != 0) {
                const size_t page = chips[i].base_addr / page_size;
                // Count only the first slot on each unique page
                bool unique = true;
                for (size_t j = 0; j < i; ++j) {
                    if (chips[j].is_mmio_only() && chips[j].addr_mask != 0 &&
                        chips[j].base_addr / page_size == page) {
                        unique = false;
                        break;
                    }
                }
                if (unique) ++count;
            }
        }
        return count;
    }

    // Maximum number of MMIO regions sharing any single MaskedSubTable page.
    [[nodiscard]] constexpr size_t max_masked_regions_per_sub(size_t page_bits) const noexcept {
        const size_t page_size = size_t(1) << page_bits;
        size_t max_count = 0;
        for (size_t i = 0; i < N; ++i) {
            if (chips[i].is_mmio_only() && chips[i].addr_mask != 0) {
                const size_t page = chips[i].base_addr / page_size;
                size_t count = 0;
                for (size_t j = 0; j < N; ++j) {
                    if (chips[j].is_mmio_only() && chips[j].addr_mask != 0 &&
                        chips[j].base_addr / page_size == page)
                        ++count;
                }
                if (count > max_count) max_count = count;
            }
        }
        return max_count;
    }

    // ── Chainable modifiers ────────────────────────────────────────────────

    // Returns a copy with the dynamic pool set to the given number of pages.
    // Use with the variadic make_chip_manifest() overload:
    //
    //   inline constexpr auto kChips = make_chip_manifest(
    //       Slot<RAMChip>{0x0000, 2}, ...
    //   ).with_dynamic_pool(256);
    //
    [[nodiscard]] constexpr ChipManifest with_dynamic_pool(size_t pages) const noexcept {
        auto copy = *this;
        copy.num_dynamic_pages = pages;
        return copy;
    }
};


// =============================================================================
// §2.1  make_chip_manifest — variadic factory
// =============================================================================
//
// Each entry carries its chip type as a template parameter for
// self-documenting declarations:
//
//   inline constexpr auto kChips = make_chip_manifest(
//       Slot<RAMChip>{0x0000, 65536},
//       Slot<ROMChip>{0xFF00,   256},
//       Slot<pia6820_t> {0xD010,     0, 0xFFFC}
//   );
//
// Types are visible in the source but stripped at compile time — the result
// is a plain ChipManifest<N>.  size_bytes must be 0 or a power of two.
// Chain .with_dynamic_pool(n) to reserve a dynamic chip pool.

template<typename... Chips>
[[nodiscard]] constexpr ChipManifest<sizeof...(Chips)>
make_chip_manifest(Slot<Chips>... slots) noexcept
{
    ChipManifest<sizeof...(Chips)> manifest{};
    size_t i = 0;
    ((assert(slots.size_bytes == 0 || (slots.size_bytes & (slots.size_bytes - 1)) == 0),
      manifest.chips[i++] = ChipSlot{
          slots.base_addr, slots.size_bytes, slots.addr_mask,
          resolve_slot_factory<Chips>(),
          slots.label,
          slots.condition
      }), ...);
    return manifest;
}


// =============================================================================
// §3  ManifestBusSpec — auto-derive BusSpec from a constexpr manifest
// =============================================================================
//
// Given a constexpr ChipManifest, derives all BusSpec fields automatically.
// Systems declare a manifest and alias the spec:
//
//   inline constexpr auto kChips = make_chip_manifest(Slot<MyChip>{...}, ...);
//   using MyBusSpec = ManifestBusSpec<kChips, 16, 8>;
//
// Template parameters:
//   Manifest  — const reference to a constexpr ChipManifest<N>
//   AddrBits  — address bus width (8, 14, 16, 24, …)
//   PgBits    — log2(page size in bytes)
//   NViewers  — number of independent bus viewers (default 1)
//

template<const auto& Manifest,
         size_t AddrBits = 16,
         size_t PgBits   = 8,
         size_t NViewers = 1>
struct ManifestBusSpec {
    using AddrType = uint_least_bits_t<AddrBits>;

    static constexpr size_t AddressBits    = AddrBits;
    static constexpr size_t PageBits       = PgBits;
    static constexpr size_t NumViewers     = NViewers;

    // Chip id range — derived from manifest prefix-sum layout.
    // MaxWriteChipId = MaxChipId (conservative; is_read_only() known at runtime only).
    static constexpr size_t MaxChipId      = Manifest.max_chip_id(PgBits);
    static constexpr size_t MaxWriteChipId = MaxChipId;

    // MMIO — derived from manifest slot analysis.
    static constexpr bool   EnableMmio       = Manifest.mmio_slot_count() > 0;
    static constexpr size_t MaxMmioHandlers  = Manifest.mmio_slot_count();

    // Masked sub-tables — auto-detected from sub-page MMIO slots.
    static constexpr size_t MaxMaskedSubTables = Manifest.masked_sub_count(PgBits);
    static constexpr size_t MaxMaskedRegions   = Manifest.max_masked_regions_per_sub(PgBits);
};


// =============================================================================
// §4  BusMemory<Spec> — runtime chip owner, buffer manager, auto-wiring
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
class BusMemory {
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
    explicit BusMemory(const ChipManifest<N>& manifest) {
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
//  BusMemory<Apple1Spec> mem{kApple1Chips};
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
