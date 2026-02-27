#pragma once
/*
 * mapper_000_nrom.h — iNES Mapper 000 (NROM)
 *
 * The simplest NES mapper: no bank switching at all.
 * PRG: 16KB or 32KB fixed, CHR: 8KB fixed (ROM or RAM).
 * Games: Donkey Kong, Super Mario Bros., Excitebike, etc.
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper000 : public Mapper {
private:
    uint8_t prg_banks;
    uint8_t chr_banks;

public:
    Mapper000(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks(prgBanks), chr_banks(chrBanks) {}

    bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            mapped_addr = 0xFFFFFFFF;  // PRG RAM sentinel
            return true;
        }
        if (addr >= 0x8000) {
            mapped_addr = addr & (prg_banks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }

    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            mapped_addr = 0xFFFFFFFF;  // PRG RAM sentinel
            return true;
        }
        if (addr >= 0x8000) {
            mapped_addr = addr & (prg_banks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }

    bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF && chr_banks == 0) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    void reset() override {}
};

} // namespace nes_system
