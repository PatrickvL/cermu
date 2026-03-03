#pragma once
/*
 * mapper_152_bandai.h — iNES Mapper 152 (Bandai, with mirror control)
 *
 * Like mapper 070 but D7 controls 1-screen mirroring.
 *   $8000-$FFFF write:
 *     D6-D4 = 16KB PRG bank at $8000,
 *     D3-D0 = 8KB CHR bank,
 *     D7 = 1-screen mirror (0=LO, 1=HI).
 *   Fixed last 16KB PRG at $C000.
 *
 * Games: Arkanoid II, Gegege no Kitarou 2, Saint Seiya.
 */

#include "../nes_mapper.h"
#include "mapper_helpers.h"

namespace nes_system {

class Mapper152 : public Mapper {
private:
    uint8_t prg_bank_ = 0;
    uint8_t chr_bank_ = 0;
    Mirror mirror_mode_ = Mirror::ONESCREEN_LO;

public:
    Mapper152(uint8_t prgBanks, uint8_t chrBanks) { (void)prgBanks; (void)chrBanks; }

    void reset() override {
        prg_bank_ = 0;
        chr_bank_ = 0;
        mirror_mode_ = Mirror::ONESCREEN_LO;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;
        prg_bank_ = (data >> 4) & 0x07;
        chr_bank_ = data & 0x0F;
        mirror_mode_ = (data & 0x80) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
        return true;
    }
};

} // namespace nes_system
