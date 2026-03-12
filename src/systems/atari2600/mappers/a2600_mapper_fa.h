#pragma once
/*
 * a2600_mapper_fa.h — Atari 2600 FA (CBS RAM Plus) bank switching
 *
 * 12KB ROM with 3 × 4KB banks, plus 256 bytes of extra RAM:
 *
 *   Bank select hotspots:
 *     $1FF8 → bank 0
 *     $1FF9 → bank 1
 *     $1FFA → bank 2
 *
 *   Extra RAM (256 bytes):
 *     $1000–$10FF → write port (store data)
 *     $1100–$11FF → read port  (load data)
 *
 * The RAM overlays the current ROM bank at those addresses.
 * Games: Mountain King, Omega Race, Tunnel Runner.
 */

#include "systems/atari2600/mappers/a2600_mapper.h"
#include <cstring>

struct A2600MapperFA : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        check_hotspot(offset);

        // RAM read port: $1100–$11FF
        if (offset >= 0x0100 && offset <= 0x01FF)
            return ram_[offset & 0xFF];

        return rom_[static_cast<uint32_t>(bank_) * 0x1000 + offset];
    }

    void write(uint16_t offset, uint8_t data) override {
        check_hotspot(offset);

        // RAM write port: $1000–$10FF
        if (offset <= 0x00FF)
            ram_[offset] = data;
    }

    const char* name() const override { return "FA"; }

    void reset() override {
        bank_ = 2;  // Start in last bank
        memset(ram_, 0, sizeof(ram_));
    }

    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 3; }

private:
    uint8_t bank_ = 2;
    uint8_t ram_[256] = {};

    inline void check_hotspot(uint16_t offset) {
        if (offset >= 0x0FF8 && offset <= 0x0FFA)
            bank_ = static_cast<uint8_t>(offset - 0x0FF8);
    }
};
