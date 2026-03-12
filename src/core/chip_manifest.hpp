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
//   Board<Spec>       — runtime owner of the unified buffer.  After binding
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
// Factory resolution — two equivalent paths:
//   - Compile-time:  Slot<T> resolves factories via resolve_slot_factory<T>()
//                    at compile time.  Used by C++ manifests (make_chip_manifest).
//   - Runtime:       ChipRegistry maps string names to the same FactoryFn
//                    at runtime.  Used by file-driven declarations (TOML/LJON).
//   Both produce ChipSlot::FactoryFn — same type, same contract.
//   See chip_registry.h for the runtime path.
//
// Usage model:
//
//   1.  Define a constexpr ChipManifest using typed Slot<T> entries.
//   2.  Derive a BusSpec: using Spec = ManifestBusSpec<kManifest, 16, 8>;
//   3.  Construct a Board<Spec> from the manifest; it allocates the buffer.
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
    // Board::create_chips().  nullptr means the slot must be manually
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
// of the buffer.  Board uses this pool for runtime chip addition (hot-swap).
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
