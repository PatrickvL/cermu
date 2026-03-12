#pragma once
/*
 * mapper_094_un1rom.h — iNES Mapper 094 (UN1ROM)
 *
 * Ultra-simple: 16KB switchable PRG only.
 *   $8000-$FFFF write: D2-D4 = 16KB PRG bank at $8000.
 *   Fixed last 16KB PRG at $C000.  No CHR switching.
 *
 * Games: Senjou no Ookami (Commando).
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper094 : public Mapper {
private:
    uint8_t prg_bank_ = 0;

public:
    Mapper094(uint8_t prgBanks, uint8_t chrBanks) { (void)prgBanks; (void)chrBanks; }

    void reset() override { prg_bank_ = 0; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;
        prg_bank_ = (data >> 2) & 0x07;
        return true;
    }
};

} // namespace nes_system
