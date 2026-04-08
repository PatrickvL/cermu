#pragma once
/*
 * a2600_mapper_cv.h — Atari 2600 CV (Commavid) cartridge
 *
 * 2KB ROM + 1KB RAM:
 *   $1000–$13FF  RAM write port (1024 bytes)
 *   $1400–$17FF  RAM read port  (1024 bytes)
 *   $1800–$1FFF  ROM (2KB, contains reset/interrupt vectors)
 *
 * The 1KB RAM uses a split-port scheme: writing to $1000–$13FF stores data,
 * reading from $1400–$17FF retrieves it. This avoids the 6502's read-before-
 * write behavior corrupting RAM.
 *
 * Games: Magicard, Video Life.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include <cstring>

struct A2600MapperCV : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        if (offset >= 0x0800) {
            // $1800–$1FFF: ROM (2KB)
            return rom_[offset & 0x07FF];
        }
        if (offset >= 0x0400) {
            // $1400–$17FF: RAM read port
            return ram_[offset & 0x03FF];
        }
        // $1000–$13FF: RAM write port — reading returns open bus
        return 0;
    }

    void write(uint16_t offset, uint8_t data) override {
        if (offset < 0x0400) {
            // $1000–$13FF: RAM write port
            ram_[offset & 0x03FF] = data;
        }
    }

    const char* name() const override { return "CV"; }
    void reset() override { memset(ram_, 0, sizeof(ram_)); }

private:
    uint8_t ram_[1024] = {};
};
