#pragma once
/*
 * a2600_mapper_2k.h — Atari 2600 2KB cartridge (no bank switching)
 *
 * 2KB ROMs are mirrored into the 4KB cartridge address window.
 * Games: Combat, Air-Sea Battle, many early titles.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"

struct A2600Mapper2K : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        return rom_[offset & 0x07FF];
    }

    const char* name() const override { return "2K"; }
    void reset() override {}
};
