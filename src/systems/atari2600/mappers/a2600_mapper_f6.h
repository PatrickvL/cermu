#pragma once
/*
 * a2600_mapper_f6.h — Atari 2600 F6 bank switching (16KB, 4 banks)
 *
 * Four 4KB banks selected by hotspot access:
 *   $1FF6 → bank 0    $1FF8 → bank 2
 *   $1FF7 → bank 1    $1FF9 → bank 3
 * Games: Solaris, Crystal Castles, Keystone Kapers, many others.
 */

#include "systems/atari2600/mappers/a2600_mapper.h"

struct A2600MapperF6 : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        check_hotspot(offset);
        return rom_[static_cast<uint32_t>(bank_) * 0x1000 + offset];
    }

    void write(uint16_t offset, uint8_t data) override {
        (void)data;
        check_hotspot(offset);
    }

    const char* name() const override { return "F6"; }
    void reset() override { bank_ = 3; }   // Start in last bank
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 4; }

private:
    uint8_t bank_ = 3;

    inline void check_hotspot(uint16_t offset) {
        if (offset >= 0x0FF6 && offset <= 0x0FF9)
            bank_ = static_cast<uint8_t>(offset - 0x0FF6);
    }
};
