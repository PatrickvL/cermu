#pragma once
/*
 * a2600_mapper_f4.h — Atari 2600 F4 bank switching (32KB, 8 banks)
 *
 * Eight 4KB banks selected by hotspot access:
 *   $1FF4 → bank 0    $1FF8 → bank 4
 *   $1FF5 → bank 1    $1FF9 → bank 5
 *   $1FF6 → bank 2    $1FFA → bank 6
 *   $1FF7 → bank 3    $1FFB → bank 7
 * Games: Fatal Run, Jr. Pac-Man, Earthworld, some homebrew.
 */

#include "a2600_mapper.h"

struct A2600MapperF4 : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        check_hotspot(offset);
        return rom_[static_cast<uint32_t>(bank_) * 0x1000 + offset];
    }

    void write(uint16_t offset, uint8_t data) override {
        (void)data;
        check_hotspot(offset);
    }

    const char* name() const override { return "F4"; }
    void reset() override { bank_ = 7; }   // Start in last bank
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 8; }

private:
    uint8_t bank_ = 7;

    inline void check_hotspot(uint16_t offset) {
        if (offset >= 0x0FF4 && offset <= 0x0FFB)
            bank_ = static_cast<uint8_t>(offset - 0x0FF4);
    }
};
