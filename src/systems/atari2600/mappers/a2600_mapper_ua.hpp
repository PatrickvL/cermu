#pragma once
/*
 * a2600_mapper_ua.h — Atari 2600 UA Limited bank switching (8KB)
 *
 * Two 4KB banks selected by accessing specific addresses in the
 * RIOT/TIA address range (outside cartridge space):
 *   Access $0220 → bank 0 (Swapped In)
 *   Access $0240 → bank 1 (Swapped Out)
 *
 * Since the hotspots are outside cartridge space ($1000–$1FFF),
 * this mapper requires bus snooping.
 *
 * The scheme was used by UA Limited (a small company, sometimes
 * attributed to "Answer Software").
 *
 * Games: Pleiades, Funky Fish, Cat Trax.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"

struct A2600MapperUA : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        return rom_[static_cast<uint32_t>(bank_) * 0x1000 + offset];
    }

    void bus_snoop(uint16_t addr, uint8_t data, bool is_write) override {
        (void)data;
        (void)is_write;
        // Hotspot addresses use bits A7 and A5 to distinguish.
        // $0220 = bank 0, $0240 = bank 1.
        // Mask to relevant bits: addr & 0x1260 — A12 clear (not cart),
        // check bits 5–6 at address offsets $220/$240.
        if ((addr & 0x1260) == 0x0220)      bank_ = 0;
        else if ((addr & 0x1260) == 0x0240) bank_ = 1;
    }

    bool needs_bus_snoop() const override { return true; }

    const char* name() const override { return "UA"; }
    void reset() override { bank_ = 0; }
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 2; }

private:
    uint8_t bank_ = 0;
};
