// =============================================================================
// viewer.hpp — Per-viewer chip-select tables and mode snapshots
// =============================================================================
//
// Each viewer (CPU, PPU, DMA, …) owns a pair of flat arrays indexed by page
// number — the emulator's equivalent of the PLA truth table.  Given a page
// number (= high address bits), the table produces a chip id that drives the
// rest of the access.
//
// Packed mode collapses both R/W arrays into one word per page slot (read id
// in low bits, write id in high bits), halving the memory footprint and
// keeping the entire table hot in L1 cache.
//
// ModeSnapshot captures a viewer's complete chip mapping for instant
// bank-switch restoration via a single memcpy.
//
// =============================================================================
#pragma once

#include "core/memory_bus/packing.hpp"

#include "core/cermu.h"          // FORCE_INLINE

#include <array>
#include <cstring>
#include <type_traits>
#include <variant>


// =============================================================================
// §1  ViewerState
// =============================================================================

template<BusSpecConcept Spec>
struct ViewerState {
    using PT          = PackingTraits<Spec>;
    using Addr        = typename Spec::AddrType;
    using ChipId      = typename PT::ChipId;
    using WriteChipId = typename PT::WriteChipId;
    using BlockId     = typename PT::BlockId;
    using PackedId    = typename PT::PackedId;
    using PageSlot    = typename PT::PageSlot;

    static constexpr size_t kNumPages = (size_t(1) << Spec::AddressBits) >> Spec::PageBits;
    static constexpr size_t kPageSize = size_t(1) << Spec::PageBits;
    static constexpr size_t kPageMask = kPageSize - 1;

    // Read-side chip table: page → ChipId.
    // In packed mode this also holds the write chip id in the upper bits.
    alignas(16) std::array<PageSlot, kNumPages> chip_table_{};

    // Write-side chip table: only materialised in separate (unpacked) mode.
    [[no_unique_address]]
    std::conditional_t<!PT::kPackedRW,
        std::array<BlockId, kNumPages>,
        std::monostate> write_chip_table_{};

    void reset() noexcept {
        if constexpr (PT::kPackedRW) {
            const PackedId no_chip_both =
                PackedId(PT::kNoChipSelected)
                | (PackedId(PT::kNoChipSelectedWrite) << PT::kWriteShift);
            chip_table_.fill(PageSlot(no_chip_both));
        } else {
            chip_table_.fill(PageSlot(PT::kNoChipSelected));
            write_chip_table_.fill(BlockId(PT::kNoChipSelectedWrite));
        }
    }

    // Return the chip id selected for a read from the given page.
    [[nodiscard]] FORCE_INLINE
    ChipId read_chip(size_t page) const noexcept {
        if constexpr (PT::kPackedRW) return ChipId(chip_table_[page] & PT::kReadMask);
        else                         return ChipId(chip_table_[page]);
    }

    // Return the chip id selected for a write to the given page.
    [[nodiscard]] FORCE_INLINE
    WriteChipId write_chip(size_t page) const noexcept {
        if constexpr (PT::kPackedRW) return WriteChipId(chip_table_[page] >> PT::kWriteShift);
        else                         return WriteChipId(write_chip_table_[page]);
    }

    FORCE_INLINE
    void set_read_chip(size_t page, ChipId id) noexcept {
        if constexpr (PT::kPackedRW) {
            auto& s = chip_table_[page];
            s = PageSlot((PackedId(s) & ~PackedId(PT::kReadMask)) | PackedId(id));
        } else { chip_table_[page] = PageSlot(id); }
    }

    FORCE_INLINE
    void set_write_chip(size_t page, WriteChipId id) noexcept {
        if constexpr (PT::kPackedRW) {
            auto& s = chip_table_[page];
            s = PageSlot((PackedId(s) & PackedId(PT::kReadMask))
                         | (PackedId(id) << PT::kWriteShift));
        } else { write_chip_table_[page] = BlockId(id); }
    }

    FORCE_INLINE
    void set_chip(size_t page, ChipId rd, WriteChipId wr) noexcept {
        if constexpr (PT::kPackedRW)
            chip_table_[page] = PageSlot(PackedId(rd) | (PackedId(wr) << PT::kWriteShift));
        else {
            chip_table_[page]       = PageSlot(rd);
            write_chip_table_[page] = BlockId(wr);
        }
    }

    [[nodiscard]] static constexpr size_t page_of  (Addr a) noexcept { return size_t(a) >> Spec::PageBits; }
    [[nodiscard]] static constexpr size_t offset_of(Addr a) noexcept { return size_t(a) &  kPageMask; }
};


// =============================================================================
// §2  ModeSnapshot — pre-computed chip table for instant mode switching
// =============================================================================
//
// Captures a viewer's complete chip mapping (both R and W tables).
// Restoring it with load_snapshot() is a single memcpy — the same cost as
// the current c64_bus_t mode_switch() which copies 16 bytes of
// cpu_encoded_chip_per_bank.
//

template<BusSpecConcept Spec>
struct ModeSnapshot {
    using V        = ViewerState<Spec>;
    using PageSlot = typename V::PageSlot;
    using BlockId  = typename PackingTraits<Spec>::BlockId;
    static constexpr size_t N = V::kNumPages;

    std::array<PageSlot, N> chip_table{};

    [[no_unique_address]]
    std::conditional_t<!PackingTraits<Spec>::kPackedRW,
        std::array<BlockId, N>,
        std::monostate> write_chip_table{};
};
