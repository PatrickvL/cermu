#pragma once
/*
 * a2600_mapper_fe.h — Atari 2600 FE (Activision) bank switching
 *
 * 8KB ROM with 2 banks. Bank switching is triggered by CPU accesses
 * to address $01FE (the stack pointer area, used by JSR/RTS).
 * Bit 5 of the data bus during such accesses selects the bank:
 *   D5 = 0 → bank 0
 *   D5 = 1 → bank 1
 *
 * This works because JSR pushes the return address high byte to $01FE,
 * and RTS reads it back. By controlling the target addresses of JSR,
 * the game can select which bank is active.
 *
 * Since the trigger address ($01FE) is outside cartridge space, this
 * mapper requires bus snooping.
 *
 * Games: Decathlon, Robot Tank.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"

struct A2600MapperFE : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        return rom_[static_cast<uint32_t>(bank_) * 0x1000 + offset];
    }

    void bus_snoop(uint16_t addr, uint8_t data, bool is_write) override {
        (void)is_write;
        // Monitor accesses to $01FE — both reads (RTS) and writes (JSR)
        if (addr == 0x01FE) {
            bank_ = (data & 0x20) ? 1 : 0;
        }
    }

    bool needs_bus_snoop() const override { return true; }

    const char* name() const override { return "FE"; }
    void reset() override { bank_ = 0; }
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 2; }

private:
    uint8_t bank_ = 0;
};
