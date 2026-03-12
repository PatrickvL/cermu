#pragma once
/*
 * mapper_086_jaleco_jf13.h — iNES Mapper 086 (Jaleco JF-13)
 *
 * Simple mapper using $6000-$7FFF register space.
 *   $6000-$7FFF write: D4-D5 = 32KB PRG bank, D0-D2|(D6<<3) = 8KB CHR bank.
 *   Single 32KB PRG window at $8000-$FFFF.
 *
 * Games: Moero Pro Yakyuu (Red).
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper086 : public Mapper {
private:
    uint8_t prg_bank_ = 0;
    uint8_t chr_bank_ = 0;

public:
    Mapper086(uint8_t prgBanks, uint8_t chrBanks) { (void)prgBanks; (void)chrBanks; }

    void reset() override { prg_bank_ = 0; chr_bank_ = 0; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_32k(config, prg_rom_, prg_rom_size_, prg_bank_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x6000 || addr >= 0x8000) return false;
        prg_bank_ = (data >> 4) & 0x03;
        chr_bank_ = (data & 0x07) | ((data >> 3) & 0x08);  // D0-D2 | (D6→D3)
        return true;
    }
};

} // namespace nes_system
