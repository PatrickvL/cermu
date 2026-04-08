#pragma once
/*
 * a2600_mapper_f4sc.h — Atari 2600 F4SC bank switching (32KB + Superchip)
 *
 * Standard F4 scheme (8 × 4KB banks) with 128 bytes Superchip RAM:
 *   Bank select: $1FF4–$1FFB → bank 0–7
 *   RAM write:   $1000–$107F
 *   RAM read:    $1080–$10FF
 *
 * Games: Fatal Run, some homebrew.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include "systems/atari2600/mappers/a2600_mapper_helpers.hpp"

struct A2600MapperF4SC : public A2600Mapper {
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

    const char* name() const override { return "F4SC"; }
    void reset() override { bank_ = 7; sc_.reset(); }
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 8; }

private:
    uint8_t bank_ = 7;
    SuperchipRAM sc_;

    inline void check_hotspot(uint16_t offset) {
        if (offset >= 0x0FF4 && offset <= 0x0FFB)
            bank_ = static_cast<uint8_t>(offset - 0x0FF4);
    }
};
