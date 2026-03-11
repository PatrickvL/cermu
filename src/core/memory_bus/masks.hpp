// =============================================================================
// masks.hpp — Per-chip data-bus width masks (EnablePartialBus)
// =============================================================================
//
// One DataType-wide mask per chip id, shared across all viewers (bus width is
// a physical property of the chip, not of the viewer).  All entries default
// to 0xFF (full bus, no bitmix overhead).
//
// Separate read and write masks are supported for chips whose output width
// differs from their input width (e.g. a display-list RAM written 8-bit but
// reading back only 6 significant bits).
//
// =============================================================================
#pragma once

#include "config.hpp"

#include <array>
#include <cassert>


template<BusSpecConcept Spec>
struct DataBusMasks {
    using DataType = spec_data_type_t<Spec>;
    static constexpr size_t   kTableSize = Spec::MaxChipId + 1;
    static constexpr DataType kFullMask  = DataType(~DataType(0));

    std::array<DataType, kTableSize> read_masks{};
    std::array<DataType, kTableSize> write_masks{};

    DataBusMasks() noexcept { reset(); }

    void reset() noexcept {
        read_masks.fill(kFullMask);
        write_masks.fill(kFullMask);
    }

    void set(size_t chip_id, DataType mask) noexcept {
        assert(chip_id < kTableSize);
        read_masks[chip_id] = write_masks[chip_id] = mask;
    }
    void set_read(size_t chip_id, DataType mask) noexcept {
        assert(chip_id < kTableSize);
        read_masks[chip_id] = mask;
    }
    void set_write(size_t chip_id, DataType mask) noexcept {
        assert(chip_id < kTableSize);
        write_masks[chip_id] = mask;
    }

    [[nodiscard]] DataType read (size_t chip_id) const noexcept { return read_masks[chip_id]; }
    [[nodiscard]] DataType write(size_t chip_id) const noexcept { return write_masks[chip_id]; }
};
