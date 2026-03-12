// =============================================================================
// sub_tables.hpp — Indexed and masked sub-page dispatch tables
// =============================================================================
//
// Models secondary address decoding within a single chip-select domain.
// A page whose chip table holds a sub-table sentinel dispatches through
// these structures to resolve the final chip id.
//
// Two dispatch strategies, chosen per sub-table at setup time:
//
//   IndexedSubTable — O(1), one shift + one mask + one array access.
//     Models uniform sub-division by top bits of the page offset.
//     Example: C64 I/O page ($D000–$DFFF) = 16 × 256 B sub-regions,
//     each mapped to a different MMIO handler (VIC-II, SID, CIA, …).
//     Lookup: sub_table[(addr >> bit_shift) & entry_mask]
//
//   MaskedSubTable — O(N) for N regions, one AND + CMP per region.
//     Models sparse carve-outs where a few address ranges override a
//     base chip.  Ideal when N ≤ 3.
//     Example: C16 TED ($FF00–$FF3F) overlaying KERNAL ROM/RAM.
//     Lookup: for each region, if (addr & mask) == match → region chip
//             else → base chip
//
// Entries in both table types use the same chip-id encoding as the main
// page table.  This enables recursion: an entry can be a kIndexedSubBase
// or kMaskedSubBase sentinel, dispatching to a deeper sub-table.
//
// =============================================================================
#pragma once

#include "packing.hpp"

#include "../cermu.h"          // FORCE_INLINE

#include <array>
#include <cassert>
#include <type_traits>
#include <variant>


// =============================================================================
// §1  IndexedSubTable — O(1) sub-page dispatch
// =============================================================================
//
// Storage uses the same packed R/W format as the main page table for minimal
// memory footprint.  Entry count = 2^kIndexedSubBits (compile-time maximum).
// Actual sub_bits ≤ kIndexedSubBits; unused entries default to kNoChipSelected.
//
// Initialise with init(sub_bits, bit_shift):
//   sub_bits  — number of address bits to extract (e.g. 4 → 16 entries)
//   bit_shift — addr >> bit_shift before masking (e.g. 8 for bits 11-8)
//

template<BusSpecConcept Spec>
struct IndexedSubTable {
    using PT          = PackingTraits<Spec>;
    using Addr        = typename Spec::AddrType;
    using ChipId      = typename PT::ChipId;
    using WriteChipId = typename PT::WriteChipId;
    using PageSlot    = typename PT::PageSlot;
    using PackedId    = typename PT::PackedId;
    using BlockId     = typename PT::BlockId;

    // Compile-time maximum entry count.  Actual count = 2^sub_bits ≤ this.
    static constexpr size_t kMaxEntries = size_t(1) << PT::kIndexedSubBits;

    uint8_t  sub_bits   = 0;    // actual bits used (≤ kIndexedSubBits)
    uint8_t  bit_shift  = 0;    // (addr >> bit_shift) & entry_mask → index
    uint16_t entry_mask = 0;    // (1 << sub_bits) - 1

    // Packed or separate R/W entry storage (same pattern as ViewerState).
    std::array<PageSlot, kMaxEntries> entries_{};

    [[no_unique_address]]
    std::conditional_t<!PT::kPackedRW,
        std::array<BlockId, kMaxEntries>,
        std::monostate> write_entries_{};

    // ── Initialise ──────────────────────────────────────────────────────────

    void init(uint8_t bits, uint8_t shift) noexcept {
        assert(bits > 0 && bits <= PT::kIndexedSubBits);
        sub_bits   = bits;
        bit_shift  = shift;
        entry_mask = uint16_t((size_t(1) << bits) - 1);
        reset();
    }

    void reset() noexcept {
        if constexpr (PT::kPackedRW) {
            const PackedId no_chip =
                PackedId(PT::kNoChipSelected)
                | (PackedId(PT::kNoChipSelectedWrite) << PT::kWriteShift);
            entries_.fill(PageSlot(no_chip));
        } else {
            entries_.fill(PageSlot(PT::kNoChipSelected));
            write_entries_.fill(BlockId(PT::kNoChipSelectedWrite));
        }
    }

    // ── Read-side lookup ────────────────────────────────────────────────────

    [[nodiscard]] FORCE_INLINE
    ChipId read_chip(Addr addr) const noexcept {
        const size_t idx = (size_t(addr) >> bit_shift) & entry_mask;
        if constexpr (PT::kPackedRW)
            return ChipId(entries_[idx] & PT::kReadMask);
        else
            return ChipId(entries_[idx]);
    }

    // ── Write-side lookup ───────────────────────────────────────────────────

    [[nodiscard]] FORCE_INLINE
    WriteChipId write_chip(Addr addr) const noexcept {
        const size_t idx = (size_t(addr) >> bit_shift) & entry_mask;
        if constexpr (PT::kPackedRW)
            return WriteChipId(entries_[idx] >> PT::kWriteShift);
        else
            return WriteChipId(write_entries_[idx]);
    }

    // ── Entry setters ───────────────────────────────────────────────────────

    void set_entry(size_t idx, ChipId rd, WriteChipId wr) noexcept {
        assert(idx <= entry_mask);
        if constexpr (PT::kPackedRW)
            entries_[idx] = PageSlot(PackedId(rd) | (PackedId(wr) << PT::kWriteShift));
        else {
            entries_[idx]       = PageSlot(rd);
            write_entries_[idx] = BlockId(wr);
        }
    }

    void set_read_entry(size_t idx, ChipId id) noexcept {
        assert(idx <= entry_mask);
        if constexpr (PT::kPackedRW) {
            auto& s = entries_[idx];
            s = PageSlot((PackedId(s) & ~PackedId(PT::kReadMask)) | PackedId(id));
        } else {
            entries_[idx] = PageSlot(id);
        }
    }

    void set_write_entry(size_t idx, WriteChipId id) noexcept {
        assert(idx <= entry_mask);
        if constexpr (PT::kPackedRW) {
            auto& s = entries_[idx];
            s = PageSlot((PackedId(s) & PackedId(PT::kReadMask))
                         | (PackedId(id) << PT::kWriteShift));
        } else {
            write_entries_[idx] = BlockId(id);
        }
    }
};


// =============================================================================
// §2  MaskedSubTable — sparse address-match sub-page dispatch
// =============================================================================
//
// Each region defines a (mask, match) pair tested against the full address:
//   if (addr & mask) == match  →  region's chip id wins
//
// Regions are front-to-back; first match wins.  For the typical 1–3 regions,
// this is a straight-line sequence of AND + CMP instructions.
//
// The base chip is the default for addresses not matching any region.
// It can be changed at runtime (e.g. C16 ROM ↔ RAM toggle) via set_base().
//

template<BusSpecConcept Spec>
struct MaskedSubTable {
    using PT          = PackingTraits<Spec>;
    using Addr        = typename Spec::AddrType;
    using ChipId      = typename PT::ChipId;
    using WriteChipId = typename PT::WriteChipId;

    static constexpr size_t kMaxRegions = PT::kMaxMaskedRegions;

    struct Region {
        Addr        addr_mask;    // AND mask applied to full address
        Addr        addr_match;   // expected result after masking
        ChipId      read_chip;    // chip id or sentinel for reads
        WriteChipId write_chip;   // chip id or sentinel for writes
    };

    ChipId      base_read  = PT::kNoChipSelected;
    WriteChipId base_write = PT::kNoChipSelectedWrite;
    size_t      num_regions = 0;
    std::array<Region, (kMaxRegions > 0 ? kMaxRegions : 1)> regions{};

    // ── Region management ───────────────────────────────────────────────────

    bool add_region(Addr mask, Addr match,
                    ChipId rd, WriteChipId wr) noexcept {
        if (num_regions >= kMaxRegions) return false;
        regions[num_regions++] = {mask, match, rd, wr};
        return true;
    }

    void set_base(ChipId rd, WriteChipId wr) noexcept {
        base_read  = rd;
        base_write = wr;
    }

    // ── Resolve ─────────────────────────────────────────────────────────────

    [[nodiscard]] ChipId resolve_read(Addr addr) const noexcept {
        for (size_t i = 0; i < num_regions; ++i) {
            if ((addr & regions[i].addr_mask) == regions[i].addr_match)
                return regions[i].read_chip;
        }
        return base_read;
    }

    [[nodiscard]] WriteChipId resolve_write(Addr addr) const noexcept {
        for (size_t i = 0; i < num_regions; ++i) {
            if ((addr & regions[i].addr_mask) == regions[i].addr_match)
                return regions[i].write_chip;
        }
        return base_write;
    }

    void reset() noexcept {
        base_read   = PT::kNoChipSelected;
        base_write  = PT::kNoChipSelectedWrite;
        num_regions = 0;
    }
};
