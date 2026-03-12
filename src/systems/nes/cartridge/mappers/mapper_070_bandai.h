#pragma once
/*
 * mapper_070_bandai.h — iNES Mapper 070 (Bandai)
 *
 * Simple discrete mapper: 16KB PRG + 8KB CHR, no mirror control.
 *   $8000-$FFFF write: D7-D4 = 16KB PRG bank at $8000, D3-D0 = 8KB CHR bank.
 *   Fixed last 16KB PRG at $C000.
 *
 * Games: Kamen Rider Club, Space Shadow, Gegege no Kitarou 2.
 * See also mapper 152 (adds 1-screen mirror control via D7).
 */

#include "systems/nes/cartridge/nes_mapper.h"
#include "systems/nes/cartridge/mappers/mapper_helpers.h"

namespace nes_system {

class Mapper070 : public Mapper {
private:
    uint8_t prg_bank_ = 0;
    uint8_t chr_bank_ = 0;

public:
    Mapper070(uint8_t prgBanks, uint8_t chrBanks) { (void)prgBanks; (void)chrBanks; }

    void reset() override { prg_bank_ = 0; chr_bank_ = 0; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;
        prg_bank_ = (data >> 4) & 0x0F;
        chr_bank_ = data & 0x0F;
        return true;
    }
};

} // namespace nes_system
