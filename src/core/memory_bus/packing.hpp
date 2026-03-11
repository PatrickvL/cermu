// =============================================================================
// packing.hpp — Chip-id types, sentinel layout, packing decision
// =============================================================================
//
// PackingTraits derives every compile-time constant the bus needs from the
// user's spec struct: chip-id integer types, sentinel thresholds, the
// packed/separate R/W decision, CS-line bit-width, and sub-table capacity.
//
// Sentinel layout (read side — write side mirrors with MaxWriteChipId):
//
//   [0, MaxChipId]                                      → direct buffer chip
//   kNoChipSelected                                     → bus floats (open bus)
//   kIndexedSubBase  .. +MaxIndexedSubTables-1           → indexed sub-table dispatch
//   kMaskedSubBase   .. +MaxMaskedSubTables-1            → masked sub-table dispatch
//   kRegChipBase     .. +MaxMmioHandlers-1               → MMIO handler dispatch
//
// When MaxIndexedSubTables = 0  the indexed range is empty (zero sentinels).
// When MaxMaskedSubTables  = 0  the masked  range is empty (zero sentinels).
// When EnableMmio = false       the MMIO    range is empty.
//
// The hot-path comparison  chip_id < kReadSentinelMin  catches all direct
// chips in one branch.  Everything ≥ kReadSentinelMin enters the slow path.
//
// =============================================================================
#pragma once

#include "config.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>


template<BusSpecConcept Spec>
struct PackingTraits {
    // ── Sub-table configuration ─────────────────────────────────────────────
    static constexpr size_t kMaxIndexedSubs   = spec_max_indexed_subs_v<Spec>;
    static constexpr size_t kIndexedSubBits   = spec_indexed_sub_bits_v<Spec>;
    static constexpr size_t kMaxMaskedSubs    = spec_max_masked_subs_v<Spec>;
    static constexpr size_t kMaxMaskedRegions = spec_max_masked_regions_v<Spec>;

    static constexpr bool kHasIndexedSub = (kMaxIndexedSubs > 0);
    static constexpr bool kHasMaskedSub  = (kMaxMaskedSubs  > 0);
    static constexpr bool kHasSubTables  = kHasIndexedSub || kHasMaskedSub;

    // ── Sentinel count ──────────────────────────────────────────────────────
    //
    // Every sentinel consumes one id in the chip-id space.  The total number
    // is: 1 (kNoChipSelected) + indexed sub-tables + masked sub-tables + MMIO.
    //
    static constexpr size_t kNumSentinels =
        1
        + kMaxIndexedSubs
        + kMaxMaskedSubs
        + (Spec::EnableMmio ? Spec::MaxMmioHandlers : 0);

    // ── Chip-id range and type derivation ───────────────────────────────────
    static constexpr size_t kReadMaxId  = Spec::MaxChipId      + kNumSentinels;
    static constexpr size_t kWriteMaxId = Spec::MaxWriteChipId + kNumSentinels;

    static constexpr size_t kReadBits  = std::bit_width(kReadMaxId);
    static constexpr size_t kWriteBits = std::bit_width(kWriteMaxId);

    using ChipId      = uint_least_bits_t<kReadBits>;
    using WriteChipId = uint_least_bits_t<kWriteBits>;
    using BlockId = uint_least_bits_t<
        (kReadBits > kWriteBits ? kReadBits : kWriteBits)>;

    // ── Packing decision ────────────────────────────────────────────────────
    static constexpr size_t kPackedBits = kReadBits + kWriteBits;
    using PackedId = uint_least_bits_t<kPackedBits>;

    static constexpr bool kPackedRW = [] {
        if constexpr (membus_detail::has_packed_rw_override<Spec>::value)
            return Spec::PackedRW;
        else
            return (kPackedBits <= 32);
    }();

    using PageSlot = std::conditional_t<kPackedRW, PackedId, BlockId>;

    static constexpr PackedId kReadMask   = (PackedId(1) << kReadBits) - 1;
    static constexpr size_t   kWriteShift = kReadBits;

    // ── Sentinel thresholds ─────────────────────────────────────────────────

    static constexpr ChipId      kReadSentinelMin  = ChipId     (Spec::MaxChipId      + 1);
    static constexpr WriteChipId kWriteSentinelMin = WriteChipId(Spec::MaxWriteChipId + 1);

    // kNoChipSelected: no chip's /CS line is asserted; data bus floats.
    static constexpr ChipId      kNoChipSelected      = kReadSentinelMin;
    static constexpr WriteChipId kNoChipSelectedWrite = kWriteSentinelMin;

    // kIndexedSubBase: indexed sub-table dispatch sentinels.
    // Range [kIndexedSubBase, kIndexedSubBase + kMaxIndexedSubs).
    // Empty range when kMaxIndexedSubs = 0.
    static constexpr ChipId      kIndexedSubBase      = ChipId     (Spec::MaxChipId      + 2);
    static constexpr WriteChipId kIndexedSubBaseWrite = WriteChipId(Spec::MaxWriteChipId + 2);

    // kMaskedSubBase: masked sub-table dispatch sentinels.
    // Range [kMaskedSubBase, kMaskedSubBase + kMaxMaskedSubs).
    // Empty range when kMaxMaskedSubs = 0.
    static constexpr ChipId      kMaskedSubBase      =
        ChipId     (size_t(kIndexedSubBase)      + kMaxIndexedSubs);
    static constexpr WriteChipId kMaskedSubBaseWrite =
        WriteChipId(size_t(kIndexedSubBaseWrite) + kMaxIndexedSubs);

    // kRegChipBase: register-file (MMIO) handler dispatch sentinels.
    // Range [kRegChipBase, kRegChipBase + MaxMmioHandlers).
    static constexpr ChipId      kRegChipBase      =
        ChipId     (size_t(kMaskedSubBase)      + kMaxMaskedSubs);
    static constexpr WriteChipId kRegChipBaseWrite =
        WriteChipId(size_t(kMaskedSubBaseWrite) + kMaxMaskedSubs);

    // ── CS line bits (multiplexed encoding) ─────────────────────────────────
    static constexpr size_t kCsLineBits = spec_cs_line_bits_v<Spec>;
    static constexpr size_t kCsBitShift = spec_cs_bit_shift_v<Spec>;
    static constexpr bool   kCsLines    = (kCsLineBits > 0);
    static constexpr uint64_t kCsMask =
        kCsLines ? (((uint64_t(1) << kCsLineBits) - 1) << kCsBitShift) : 0;

    // ── Static assertions ───────────────────────────────────────────────────
    static_assert(Spec::MaxWriteChipId <= Spec::MaxChipId);
    static_assert(Spec::PageBits >= 1 && Spec::PageBits < Spec::AddressBits);
    static_assert(kReadMaxId  < (size_t(1) << (sizeof(ChipId)      * 8)));
    static_assert(kWriteMaxId < (size_t(1) << (sizeof(WriteChipId) * 8)));
    static_assert(!kCsLines || kCsLineBits >= std::bit_width(kReadMaxId),
        "CsLineBits is too narrow to represent all chip ids including sentinels");
    static_assert(!kCsLines || (kCsBitShift + kCsLineBits <= 64),
        "CS field overflows bus_state_t (64 bits)");
    static_assert(!kHasIndexedSub || kIndexedSubBits > 0,
        "IndexedSubBits must be > 0 when MaxIndexedSubTables > 0");
    static_assert(!kHasMaskedSub || kMaxMaskedRegions > 0,
        "MaxMaskedRegions must be > 0 when MaxMaskedSubTables > 0");
};
