#pragma once
/*
 * a2600_mapper_ef.h — Atari 2600 EF bank switching (64KB, 16 banks)
 *
 * Homebrew scheme. Sixteen 4KB banks selected by hotspot access:
 *   $1FE0 → bank 0     $1FE8 → bank 8
 *   $1FE1 → bank 1     $1FE9 → bank 9
 *   $1FE2 → bank 2     $1FEA → bank 10
 *   $1FE3 → bank 3     $1FEB → bank 11
 *   $1FE4 → bank 4     $1FEC → bank 12
 *   $1FE5 → bank 5     $1FED → bank 13
 *   $1FE6 → bank 6     $1FEE → bank 14
 *   $1FE7 → bank 7     $1FEF → bank 15
 *
 * Games: Homebrew titles (Paul Slocum's Homestar Runner, etc.)
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"

struct A2600MapperEF : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        check_hotspot(offset);
        return rom_[static_cast<uint32_t>(bank_) * 0x1000 + offset];
    }

    void write(uint16_t offset, uint8_t data) override {
        (void)data;
        check_hotspot(offset);
    }

    const char* name() const override { return "EF"; }
    void reset() override { bank_ = 15; }  // Start in last bank
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 16; }

private:
    uint8_t bank_ = 15;

    inline void check_hotspot(uint16_t offset) {
        if (offset >= 0x0FE0 && offset <= 0x0FEF)
            bank_ = static_cast<uint8_t>(offset - 0x0FE0);
    }
};
