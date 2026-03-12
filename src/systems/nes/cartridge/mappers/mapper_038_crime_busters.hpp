#pragma once
/*
 * mapper_038_crime_busters.h — iNES Mapper 038 (PD-8080 / Crime Busters)
 *
 * Simple discrete-logic mapper: 32KB PRG + 8KB CHR bank switching
 * via a single register in the $6000-$7FFF range.
 *
 * Games: Crime Busters (Bit Corp.)
 *
 * Register ($7000-$7FFF):
 *   D1-D0: PRG 32KB bank select
 *   D3-D2: CHR 8KB bank select
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper038 : public Mapper {
private:
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper038(uint8_t prgBanks, uint8_t chrBanks) {
        (void)prgBanks; (void)chrBanks;
    }

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_select_ = 0;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_32k(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_select_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x7000 && addr < 0x8000) {
            prg_bank_select_ = data & 0x03;
            chr_bank_select_ = (data >> 2) & 0x03;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
