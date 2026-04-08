#pragma once
/*
 * a2600_mapper_f8sc.h — Atari 2600 F8SC bank switching (8KB + Superchip)
 *
 * Standard F8 scheme (2 × 4KB banks) with 128 bytes Superchip RAM:
 *   Bank select: $1FF8 → bank 0, $1FF9 → bank 1
 *   RAM write:   $1000–$107F
 *   RAM read:    $1080–$10FF
 *
 * Games: Dig Dug, Stargate, some homebrew.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include "systems/atari2600/mappers/a2600_mapper_helpers.hpp"

struct A2600MapperF8SC : public A2600Mapper {
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

    const char* name() const override { return "F8SC"; }
    void reset() override { bank_ = 1; sc_.reset(); }
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 2; }

private:
    uint8_t bank_ = 1;
    SuperchipRAM sc_;

    inline void check_hotspot(uint16_t offset) {
        if (offset == 0x0FF8)      bank_ = 0;
        else if (offset == 0x0FF9) bank_ = 1;
    }
};
