#pragma once
/*
 * mapper_003_cnrom.h — iNES Mapper 003 (CNROM)
 *
 * Simple CHR bank switching: writes to $8000-$FFFF select an 8KB CHR bank.
 * PRG is 16KB or 32KB fixed (same as NROM).
 * Games: Galaxian, Gradius, Arkista's Ring, etc.
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper003 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper003(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x8000) {
            mapped_addr = addr & (prg_banks_ > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }

    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
        if (addr >= 0x8000) {
            chr_bank_select_ = data & 0x03;
        }
        return false;  // No actual ROM write
    }

    bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF) {
            mapped_addr = chr_bank_select_ * 0x2000 + addr;
            return true;
        }
        return false;
    }

    bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) override {
        // CNROM uses CHR ROM, no writes
        return false;
    }

    void reset() override { chr_bank_select_ = 0; }
};

} // namespace nes_system
