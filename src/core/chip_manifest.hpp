// =============================================================================
// chip_manifest.hpp — Chip manifest, flat memory ownership, auto-wiring
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
//   Board<Spec>       — runtime owner of the flat memory.  After binding
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
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "core/chip.hpp"


// =============================================================================
// §0.1  RomFileInfo — ROM file loading metadata
// =============================================================================
//
// Attached to a ChipSlot to enable automatic ROM loading via
// Board::load_roms().  The filenames array is tried in order by
// rom_loader_load_from_root() until one succeeds.  Optional ROMs
// (e.g. Apple 1 BASIC) set optional=true so the loader reports
// but does not fail the system.
//

struct RomFileInfo {
    const char* filenames = nullptr;  // Pipe-separated candidate filenames ("name1|name2|name3")
    bool optional = false;            // false = required for boot; true = nice-to-have

    [[nodiscard]] constexpr bool has_rom() const noexcept { return filenames != nullptr; }
};


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
//   addr_mask  — For MMIO-only slots (size_bytes == 0):
//                  sub-page address decode mask (0 = full-page decode).
//                  When non-zero, (addr & addr_mask) == (base_addr & addr_mask)
//                  selects this chip.  Triggers MaskedSubTable auto-creation.
//                For buffer slots (size_bytes > 0):
//                  hardware mirroring mask.  When non-zero, all banks share
//                  the same base offset and use addr_mask instead of the
//                  bank-size derived mask, so the larger address range wraps
//                  to the physical chip size.  E.g. a 1 KB chip mirrored
//                  across 2 KB: size_bytes = 2048, addr_mask = 0x03FF.
//

struct ChipSlot {
    uint32_t base_addr  = 0;
    size_t   size_bytes = 0;
    uint32_t addr_mask  = 0;
    size_t   bank_size  = 0;     // Buffer slots: bank granularity in bytes (0 = one bank per page).
                                 // MMIO-only slots: total address-decode range in bytes.
                                 //   When > page size, apply() mirrors the MMIO handler
                                 //   (or sub-table) to all pages in the range.
    size_t   effective_size = 0; // 0 = use size_bytes (default).
                                 // When > 0 and < size_bytes, apply() Phase 1 maps
                                 // only this many bytes.  Phase 0 chip_info covers
                                 // the full size_bytes so bank switching and buffer
                                 // access beyond the visible window still work.
                                 // Use for slots whose visible size is a compile-time
                                 // constant smaller than the allocation (e.g. KC85/4
                                 // IRM: 64 KB allocated, 16 KB visible at a time).
                                 // For runtime-variable sizes, use set_effective_size().
    uint8_t  overlay_group = 0;  // 0 = always visible (base layer, e.g. RAM)
                                 // >0 = banking group: visible when that group's
                                 //       bit is set in the mode index.  Read-only
                                 //       overlay (writes pass through to group 0).

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

    // ROM file loading metadata — filenames==nullptr for non-ROM slots
    // and ROM slots that need custom loading logic (e.g. split ROMs).
    RomFileInfo rom;

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
    size_t      bank_size  = 0;
    uint8_t     overlay_group = 0;
    size_t      effective_size = 0;  // 0 = use size_bytes.  See ChipSlot::effective_size.
    RomFileInfo rom{};                 // ROM file metadata (filenames==nullptr → no auto-load)

    // Returns a copy with ROM file metadata attached.
    // filenames is a pipe-separated string: "name1|name2|name3"
    [[nodiscard]] constexpr Slot with_rom(const char* filenames,
                                          bool optional = false) const noexcept {
        auto copy = *this;
        copy.rom = {filenames, optional};
        return copy;
    }
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
// Chip ids are assigned as bank ids: each chip of K banks occupies ids
// [base_id, base_id + K - 1].  The bank granularity is set per-chip via
// ChipSlot::bank_size:
//
//   bank_size > 0  → explicit: K = size_bytes / bank_size
//   bank_size = 0  → legacy page-per-bank: K = size_bytes >> page_bits
//
// Each bank_id maps to a region in the flat memory through a ChipInfo
// entry (base byte offset + address mask), populated by BusMap::apply().
//
// An optional dynamic pool of num_dynamic_pages pages is appended at the end
// of the buffer.  Board uses this pool for runtime chip addition (hot-swap).
//
// Constexpr query methods enable ManifestBusSpec to auto-derive all BusSpec
// fields from the manifest alone.
//
// Example — Apple 1 (page-per-bank, bank_size=0):
//
//   inline constexpr auto kApple1Chips = make_chip_manifest(
//       Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
//       Slot<ROMChip>{0xFF00,   256, 0, "Monitor ROM"},
//       Slot<ROMChip>{0xE000,  4096, 0, "BASIC ROM"},
//       Slot<pia6820_t>{0xD010, 0, 0xFFFC, "PIA"} // MMIO-only, 4-byte window
//   );
//
//   // With PageBits = 8 (256-byte pages), bank_size=0 → bank = page:
//   constexpr auto kRamId      = kApple1Chips.base_id(0, 8);   // = 0
//   constexpr auto kMonitorId  = kApple1Chips.base_id(1, 8);   // = 256
//   constexpr auto kBasicId    = kApple1Chips.base_id(2, 8);   // = 257
//   // PIA has no buffer → no chip id assignment
//
// Example — C264 (whole-chip banks, bank_size = size_bytes):
//
//   Slot<RAMChip>{0x0000, 65536, 0, "RAM", 0, 65536}     → 1 bank, id 0
//   Slot<ROMChip>{0x8000, 16384, 0, "BASIC", 0, 16384}   → 1 bank, id 1
//   Slot<ROMChip>{0xC000, 16384, 0, "KERNAL", 0, 16384}  → 1 bank, id 2
//   // MaxChipId = 2 (vs. 383 with page-per-bank)
//

template<size_t N>
struct ChipManifest {
    std::array<ChipSlot, N> chips{};
    size_t num_dynamic_pages = 0;  // reserved for runtime-added chips
    bool   sorted_ids        = false; // When true, base_id() and byte_offset()
                                       // assign IDs in size-ascending order
                                       // regardless of declaration order.
                                       // Enables address-sorted chip lists
                                       // while preserving optimal shift-
                                       // addressable chip-id layout.

    // ── Chip-id assignment ─────────────────────────────────────────────────
    //
    // Chip ids are assigned as bank ids: each chip with bank_size > 0
    // occupies size_bytes / bank_size consecutive ids.  When bank_size == 0
    // the chip falls back to one bank per page (backward compatible with
    // the legacy page-index model).
    //
    // When sorted_ids is true, ids are assigned as if buffer chips were
    // sorted by size ascending (stable, declaration order breaks ties).
    // This decouples declaration order (readability) from id assignment
    // (shift-addressable optimization).
    //
    // All queries take page_bits for the fallback page size.
    //

    // Number of bank ids consumed by chip `i`.
    [[nodiscard]] constexpr size_t bank_count(size_t i, size_t page_bits) const noexcept {
        if (chips[i].size_bytes == 0) return 0;
        const size_t bs = chips[i].bank_size > 0
            ? chips[i].bank_size : (size_t(1) << page_bits);
        return chips[i].size_bytes / bs;
    }

    // Effective bank size for chip `i` (bank_size=0 → page size).
    [[nodiscard]] constexpr size_t effective_bank_size(size_t i, size_t page_bits) const noexcept {
        return chips[i].bank_size > 0 ? chips[i].bank_size : (size_t(1) << page_bits);
    }

    // Base chip id of chip at index chip_index (0-based).
    //
    // When sorted_ids is false (default): prefix sum of bank_counts in
    // declaration order (chips[0..chip_index-1]).
    //
    // When sorted_ids is true: prefix sum of bank_counts of all buffer
    // chips whose size is strictly smaller, plus same-size chips with a
    // lower declaration index (stable size-ascending order).
    //
    [[nodiscard]] constexpr size_t base_id(size_t chip_index, size_t page_bits) const noexcept {
        if (!sorted_ids) {
            size_t id = 0;
            for (size_t i = 0; i < chip_index; ++i)
                id += bank_count(i, page_bits);
            return id;
        }
        // chip_index == N → total banks (all chips sort before the sentinel)
        if (chip_index >= N) {
            size_t id = 0;
            for (size_t i = 0; i < N; ++i)
                id += bank_count(i, page_bits);
            return id;
        }
        if (chips[chip_index].size_bytes == 0) return 0;
        const size_t my_size = chips[chip_index].size_bytes;
        size_t id = 0;
        for (size_t j = 0; j < N; ++j) {
            if (j == chip_index) continue;
            if (chips[j].size_bytes == 0) continue;
            if (chips[j].size_bytes < my_size ||
                (chips[j].size_bytes == my_size && j < chip_index))
                id += bank_count(j, page_bits);
        }
        return id;
    }

    // Total bank ids across all static chips.
    [[nodiscard]] constexpr size_t total_banks(size_t page_bits) const noexcept {
        size_t id = 0;
        for (size_t i = 0; i < N; ++i)
            id += bank_count(i, page_bits);
        return id;
    }

    // First chip id in the dynamic pool (= one past the last static bank id).
    [[nodiscard]] constexpr size_t dynamic_base_id(size_t page_bits) const noexcept {
        return total_banks(page_bits);
    }

    // Total chip ids in use (static banks + dynamic pool pages).
    [[nodiscard]] constexpr size_t total_ids(size_t page_bits) const noexcept {
        return total_banks(page_bits) + num_dynamic_pages;
    }

    // Highest chip id that will ever appear in a page table.
    // Set MaxChipId in your BusSpec to this value.
    [[nodiscard]] constexpr size_t max_chip_id(size_t page_bits) const noexcept {
        const size_t ti = total_ids(page_bits);
        return ti > 0 ? ti - 1 : 0;
    }

    // Byte offset of chip at index chip_index in the flat memory.
    //
    // When sorted_ids is true, the flat memory layout follows size-ascending
    // order so that byte_offset(i) == base_id(i, page_bits) << page_bits
    // holds for all addressable buffer chips (shift-addressable invariant).
    // Sub-page chips (size > 0, bank_count == 0) are placed after all
    // addressable chips since they have no chip IDs to align with.
    //
    // page_bits is required in sorted mode to distinguish addressable from
    // sub-page chips; ignored in non-sorted mode.
    //
    [[nodiscard]] constexpr size_t byte_offset(size_t chip_index, size_t page_bits = 0) const noexcept {
        if (!sorted_ids) {
            size_t off = 0;
            for (size_t i = 0; i < chip_index; ++i)
                off += chips[i].size_bytes;
            return off;
        }
        if (chip_index >= N) return total_buffer_bytes();
        if (chips[chip_index].size_bytes == 0) return total_buffer_bytes();
        const size_t my_bc = bank_count(chip_index, page_bits);
        const size_t my_size = chips[chip_index].size_bytes;
        if (my_bc == 0) {
            // Sub-page chip: placed after all addressable chips.
            size_t off = 0;
            for (size_t j = 0; j < N; ++j) {
                if (chips[j].size_bytes == 0) continue;
                if (bank_count(j, page_bits) > 0)
                    off += chips[j].size_bytes;  // addressable: always before
                else if (j < chip_index)
                    off += chips[j].size_bytes;  // earlier sub-page chip
            }
            return off;
        }
        // Addressable chip: sorted by size ascending, ties by declaration order.
        // Only count other addressable chips (skip sub-page).
        size_t off = 0;
        for (size_t j = 0; j < N; ++j) {
            if (j == chip_index) continue;
            if (chips[j].size_bytes == 0) continue;
            if (bank_count(j, page_bits) == 0) continue;
            if (chips[j].size_bytes < my_size ||
                (chips[j].size_bytes == my_size && j < chip_index))
                off += chips[j].size_bytes;
        }
        return off;
    }

    // Total static buffer bytes (sum of all chip size_bytes).
    [[nodiscard]] constexpr size_t total_buffer_bytes() const noexcept {
        size_t sum = 0;
        for (size_t i = 0; i < N; ++i)
            sum += chips[i].size_bytes;
        return sum;
    }

    // Buffer size in bytes (static chips + dynamic pool).
    [[nodiscard]] constexpr size_t buffer_bytes(size_t page_bits) const noexcept {
        return total_buffer_bytes() + (num_dynamic_pages << page_bits);
    }

    // Convenience: number of static chips.
    [[nodiscard]] static constexpr size_t num_chips() noexcept { return N; }

    // Number of distinct non-zero overlay groups in the manifest.
    [[nodiscard]] constexpr size_t num_overlay_groups() const noexcept {
        uint8_t max_g = 0;
        for (size_t i = 0; i < N; ++i)
            if (chips[i].overlay_group > max_g)
                max_g = chips[i].overlay_group;
        return max_g;
    }

    // Number of overlay modes: 2^num_overlay_groups.
    // Mode 0 = base only.  Mode k has overlay groups active for each set bit.
    [[nodiscard]] constexpr size_t overlay_mode_count() const noexcept {
        return size_t(1) << num_overlay_groups();
    }

    // ── Shift-addressable chip-id assignment ───────────────────────────────
    //
    // When every buffer chip's effective bank size is uniform and a power of
    // two, chip-id → flat_mem offset can be computed as `id << log2(stride)`
    // instead of a table lookup.
    //
    // The effective bank size is:
    //   bank_size > 0  → bank_size
    //   bank_size == 0 → page size (1 << page_bits)
    //
    // Chips in the manifest must be ordered by size ascending (smallest
    // first, largest last).  Each chip consumes (size / stride) consecutive
    // IDs; the base_id is the prefix sum of all preceding chips' bank counts.
    //
    // Example (C64, stride = 4096 with page banking):
    //
    //   CHARROM  4 KB  →  1 ID  → base_id = 0
    //   ROML     8 KB  →  2 IDs → base_id = 1
    //   ROMH     8 KB  →  2 IDs → base_id = 3
    //   BASIC    8 KB  →  2 IDs → base_id = 5
    //   KERNAL   8 KB  →  2 IDs → base_id = 7
    //   RAM     64 KB  → 16 IDs → base_id = 9
    //
    // The technique requires: all buffer chips share the same effective bank
    // size, and that size is a power of two.

    // True when chip-id → offset can be a shift instead of a table lookup.
    // Checks that all buffer chips share the same effective bank size and
    // that size is a power of two.
    [[nodiscard]] constexpr bool has_shift_addressable_banks(size_t page_bits) const noexcept {
        size_t common = 0;
        for (size_t i = 0; i < N; ++i) {
            if (chips[i].size_bytes == 0) continue;
            const size_t ebs = effective_bank_size(i, page_bits);
            if (common == 0)
                common = ebs;
            else if (ebs != common)
                return false;
        }
        return common > 0 && std::has_single_bit(common);
    }

    // log2(stride) — valid only when has_shift_addressable_banks() is true.
    [[nodiscard]] constexpr size_t shift_bank_bits(size_t page_bits) const noexcept {
        for (size_t i = 0; i < N; ++i)
            if (chips[i].size_bytes > 0)
                return std::bit_width(effective_bank_size(i, page_bits)) - 1;
        return 0;
    }

    // Number of buffer-backed chips (size_bytes > 0).
    [[nodiscard]] constexpr size_t buffer_chip_count() const noexcept {
        size_t n = 0;
        for (size_t i = 0; i < N; ++i)
            if (chips[i].size_bytes > 0) ++n;
        return n;
    }

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

    // Count of MMIO-only slots that participate in bus address decode.
    // These are chips with size_bytes == 0 AND base_addr != 0, excluding
    // CPUs and non-bus peripherals (which have base_addr == 0).
    // Used by ManifestBusSpec to compute the CS line width.
    [[nodiscard]] constexpr size_t bus_mmio_slot_count() const noexcept {
        size_t count = 0;
        for (size_t i = 0; i < N; ++i) {
            if (chips[i].size_bytes == 0 && chips[i].base_addr != 0)
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

    // ── Type-based slot lookup ─────────────────────────────────────────────

    // Find the slot index of the Nth chip of type T (0-based).
    // Matching uses the factory function pointer, which is unique per Slot<T>
    // type (each template instantiation produces a distinct pointer).
    // Returns N (past-end) when fewer than (nth + 1) chips of type T exist.
    //
    // Example:
    //   constexpr size_t pio1 = manifest.find<z80_pio_t>();     // first PIO
    //   constexpr size_t pio2 = manifest.find<z80_pio_t>(1);    // second PIO
    //   static_assert(pio1 < manifest.num_chips(), "PIO not found");
    //
    template<typename T>
    [[nodiscard]] constexpr size_t find(size_t nth = 0) const noexcept {
        constexpr auto target = resolve_slot_factory<T>();
        size_t count = 0;
        for (size_t i = 0; i < N; ++i) {
            if (chips[i].factory == target) {
                if (count == nth) return i;
                ++count;
            }
        }
        return N;
    }

    // Find the last slot of type T.  Returns N (past-end) if none exist.
    template<typename T>
    [[nodiscard]] constexpr size_t find_last() const noexcept {
        constexpr auto target = resolve_slot_factory<T>();
        size_t result = N;
        for (size_t i = 0; i < N; ++i) {
            if (chips[i].factory == target)
                result = i;
        }
        return result;
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

    // Returns a copy with bank_size set to 0 on every buffer chip.
    // This causes effective_bank_size(i, page_bits) to return the page size,
    // giving each chip ceil(size / page_size) bank IDs.  Use this when the
    // system's page table stores per-bank chip IDs (e.g. C64 PLA banking).
    //
    //   inline constexpr auto kChips = make_chip_manifest(
    //       Slot<ROMChip>{0xD000, 4096}, ...
    //   ).with_page_banking();
    //
    [[nodiscard]] constexpr ChipManifest with_page_banking() const noexcept {
        auto copy = *this;
        for (size_t i = 0; i < N; ++i)
            if (copy.chips[i].size_bytes > 0)
                copy.chips[i].bank_size = 0;
        return copy;
    }

    // Returns a copy with sorted_ids enabled.  Chip IDs are assigned in
    // size-ascending order regardless of declaration order.  Use after
    // with_page_banking() so that ID assignment matches the optimal
    // shift-addressable layout.
    //
    [[nodiscard]] constexpr ChipManifest with_sorted_ids() const noexcept {
        auto copy = *this;
        copy.sorted_ids = true;
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
          slots.bank_size,
          slots.effective_size,
          slots.overlay_group,
          resolve_slot_factory<Chips>(),
          slots.label,
          slots.condition,
          slots.rom
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
         size_t NViewers = 1,
         bool   EnableCs = false>
struct ManifestBusSpec {
    using AddrType = uint_least_bits_t<AddrBits>;

    // Compact type for ChipInfo byte offsets into flat memory.
    using BaseType = uint_least_bits_t<std::bit_width(
        Manifest.buffer_bytes(PgBits) > 0 ? Manifest.buffer_bytes(PgBits) - 1 : size_t(0))>;
    // Address mask type (same as address width).
    using MaskType = AddrType;

    static constexpr size_t AddressBits    = AddrBits;
    static constexpr size_t PageBits       = PgBits;
    static constexpr size_t NumViewers     = NViewers;

    // Chip id range — derived from manifest bank-id layout.
    // When CS is enabled, MMIO-only slots (bus-decoded, zero-sized) also get
    // real chip IDs above the buffer range, so MaxChipId includes them.
    // MaxWriteChipId = MaxChipId (conservative; is_read_only() known at runtime only).
    static constexpr size_t BufferMaxChipId = Manifest.max_chip_id(PgBits);
    static constexpr size_t BusMmioCount    = Manifest.bus_mmio_slot_count();
    static constexpr size_t MaxChipId       = EnableCs
                                            ? BufferMaxChipId + BusMmioCount
                                            : BufferMaxChipId;
    static constexpr size_t MaxWriteChipId  = MaxChipId;

    // MMIO — derived from manifest slot analysis.
    static constexpr bool   EnableMmio       = Manifest.mmio_slot_count() > 0;
    static constexpr size_t MaxMmioHandlers  = Manifest.mmio_slot_count();

    // Masked sub-tables — auto-detected from sub-page MMIO slots.
    static constexpr size_t MaxMaskedSubTables = Manifest.masked_sub_count(PgBits);
    static constexpr size_t MaxMaskedRegions   = Manifest.max_masked_regions_per_sub(PgBits);

    // ── Shift-addressable chip-id optimization ─────────────────────────
    //
    // When all buffer chip sizes share a power-of-two GCD ("stride"),
    // Board can compute flat_mem offsets as `id << ShiftBankBits` instead
    // of indexing a chip_byte_offsets_[] table.  Chip IDs are assigned in
    // ascending chip-size order so the largest chip (most banks) uses the
    // highest IDs, minimising the MaxChipId that appears in page tables.
    //
    // When not shift-addressable, Board falls back to the offset table.
    //
    static constexpr bool   ShiftAddressable = Manifest.has_shift_addressable_banks(PgBits);
    static constexpr size_t ShiftBankBits    = ShiftAddressable ? Manifest.shift_bank_bits(PgBits) : 0;

    // ── CS line support (chip-select field in bus_state_t) ─────────────
    //
    // When EnableCs is true, resolve() embeds the decoded chip ID into the
    // global CS field (BUS_CS_SHIFT / BUS_CS_BITS defined in system_lines.hpp).
    // Each chip's tick() checks it against its own bus_chip_id_.
    //
    // EnableCs = false (the default) disables CS — the system uses the
    // callback-driven tick() path instead.
    //
    static constexpr size_t CsBitShift = EnableCs ? BUS_CS_SHIFT : 0;
    static constexpr size_t CsLineBits = EnableCs ? BUS_CS_BITS  : 0;
};


// =============================================================================
