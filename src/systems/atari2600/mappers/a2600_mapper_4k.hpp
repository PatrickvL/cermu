#pragma once
/*
 * a2600_mapper_4k.h — Atari 2600 4KB cartridge (no bank switching)
 *
 * 4KB ROMs map directly into the 4KB cartridge address window.
 * Games: Space Invaders, Pac-Man, Adventure, many mid-era titles.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"

struct A2600Mapper4K : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        return rom_[offset & 0x0FFF];
    }

    const char* name() const override { return "4K"; }
    void reset() override {}
};
