#pragma once
/*
 * mapper_002_uxrom.h — iNES Mapper 002 (UxROM)
 *
 * Simple PRG bank switching: switchable 16KB at $8000, fixed last 16KB at $C000.
 * CHR 8KB ROM/RAM with no switching.
 * Games: Castlevania, Contra, Metal Gear, Mega Man, etc.
 */

#include "../nes_mapper.h"
#include "mapper_helpers.h"

namespace nes_system {

class Mapper002 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;

public:
    Mapper002(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override { prg_bank_select_ = 0; }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            prg_bank_select_ = data & 0x0F;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
