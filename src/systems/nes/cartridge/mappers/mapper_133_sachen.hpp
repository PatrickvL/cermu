#pragma once
/*
 * mapper_133_sachen.h — iNES Mapper 133 (Sachen SA-72008)
 *
 * Very simple mapper: 32KB PRG + 8KB CHR bank switching via
 * a single register at $4100 (active when A8=0, A6=1, A5=0, A0=0).
 *
 * Games: Hummer Team Sachen unlicensed titles
 *
 * Register ($4100):
 *   D2:    32KB PRG bank select
 *   D1-D0: 8KB CHR bank select
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper133 : public Mapper {
private:
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper133(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

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
        // Active at $4100 (addr & $E100 == $4100)
        if ((addr & 0xE100) == 0x4100) {
            prg_bank_select_ = (data >> 2) & 0x01;
            chr_bank_select_ = data & 0x03;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
