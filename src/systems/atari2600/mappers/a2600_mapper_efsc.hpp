#pragma once
/*
 * a2600_mapper_efsc.h — Atari 2600 EFSC bank switching (64KB + Superchip)
 *
 * Same as EF (16 × 4KB banks, hotspots $1FE0–$1FEF) with 128 bytes
 * Superchip RAM:
 *   RAM write: $1000–$107F
 *   RAM read:  $1080–$10FF
 *
 * Games: Various homebrew.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include "systems/atari2600/mappers/a2600_mapper_helpers.hpp"

struct A2600MapperEFSC : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        check_hotspot(offset);
        uint8_t val;
        if (sc_.try_read(offset, val)) return val;
        return rom_[static_cast<uint32_t>(bank_) * 0x1000 + offset];
    }

    void write(uint16_t offset, uint8_t data) override {
        check_hotspot(offset);
        sc_.try_write(offset, data);
    }

    const char* name() const override { return "EFSC"; }
    void reset() override { bank_ = 15; sc_.reset(); }
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 16; }

private:
    uint8_t bank_ = 15;
    SuperchipRAM sc_;

    inline void check_hotspot(uint16_t offset) {
        if (offset >= 0x0FE0 && offset <= 0x0FEF)
            bank_ = static_cast<uint8_t>(offset - 0x0FE0);
    }
};
