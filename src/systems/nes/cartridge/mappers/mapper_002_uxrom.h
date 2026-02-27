#pragma once
/*
 * mapper_002_uxrom.h — iNES Mapper 002 (UxROM)
 *
 * Simple PRG bank switching: switchable 16KB at $8000, fixed last 16KB at $C000.
 * CHR 8KB ROM/RAM with no switching.
 * Games: Castlevania, Contra, Metal Gear, Mega Man, etc.
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper002 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;

public:
    Mapper002(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x8000 && addr <= 0xBFFF) {
            // Switchable bank at $8000
            mapped_addr = prg_bank_select_ * 0x4000 + (addr & 0x3FFF);
            return true;
        }
        if (addr >= 0xC000) {
            // Fixed last bank at $C000
            mapped_addr = (prg_banks_ - 1) * 0x4000 + (addr & 0x3FFF);
            return true;
        }
        return false;
    }

    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
        if (addr >= 0x8000) {
            prg_bank_select_ = data & 0x0F;
        }
        return false;  // No actual ROM write
    }

    bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF && chr_banks_ == 0) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    void reset() override { prg_bank_select_ = 0; }
};

} // namespace nes_system
