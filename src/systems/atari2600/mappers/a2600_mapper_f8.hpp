#pragma once
/*
 * a2600_mapper_f8.h — Atari 2600 F8 bank switching (8KB, 2 banks)
 *
 * Standard 8KB scheme. Two 4KB banks selected by hotspot access:
 *   $1FF8 → bank 0
 *   $1FF9 → bank 1
 * Both reads and writes to the hotspot addresses trigger bank switching.
 * Games: Asteroids, Missile Command, Pitfall!, River Raid, many others.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"

struct A2600MapperF8 : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        check_hotspot(offset);
        return rom_[static_cast<uint32_t>(bank_) * 0x1000 + offset];
    }

    void write(uint16_t offset, uint8_t data) override {
        (void)data;
        check_hotspot(offset);
    }

    const char* name() const override { return "F8"; }
    void reset() override { bank_ = 1; }   // Start in last bank (reset vector)
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 2; }

private:
    uint8_t bank_ = 1;

    inline void check_hotspot(uint16_t offset) {
        if (offset == 0x0FF8)      bank_ = 0;
        else if (offset == 0x0FF9) bank_ = 1;
    }
};
