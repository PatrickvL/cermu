#pragma once
/*
 * mapper_011_color_dreams.h — iNES Mapper 011 (Color Dreams)
 *
 * Simple discrete-logic mapper: a single latch selects both PRG and CHR banks.
 * 32KB PRG bank + 8KB CHR bank selection via writes to $8000-$FFFF.
 * Bus conflicts are present on real hardware.
 * Games: Crystal Mines, Bible Adventures, Wisdom Tree games, etc.
 *
 * Register ($8000-$FFFF):
 *   D7-D4: CHR 8KB bank select
 *   D1-D0: PRG 32KB bank select
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper011 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper011(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_select_ = 0;
    }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_32k(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_select_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            prg_bank_select_ = data & 0x03;
            chr_bank_select_ = (data >> 4) & 0x0F;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
