#pragma once
/*
 * a2600_mapper_f6sc.h — Atari 2600 F6SC bank switching (16KB + Superchip)
 *
 * Standard F6 scheme (4 × 4KB banks) with 128 bytes Superchip RAM:
 *   Bank select: $1FF6 → bank 0, $1FF7 → bank 1,
 *                $1FF8 → bank 2, $1FF9 → bank 3
 *   RAM write:   $1000–$107F
 *   RAM read:    $1080–$10FF
 *
 * Games: Kung-Fu Master, Super Football, Radar Lock.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include "systems/atari2600/mappers/a2600_mapper_helpers.hpp"

struct A2600MapperF6SC : public A2600Mapper {
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

    const char* name() const override { return "F6SC"; }
    void reset() override { bank_ = 3; sc_.reset(); }
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 4; }

private:
    uint8_t bank_ = 3;
    SuperchipRAM sc_;

    inline void check_hotspot(uint16_t offset) {
        if (offset >= 0x0FF6 && offset <= 0x0FF9)
            bank_ = static_cast<uint8_t>(offset - 0x0FF6);
    }
};
