#pragma once
/*
 * mapper_079_nina.h — iNES Mapper 079 (NINA-03 / NINA-06)
 *
 * Simple discrete-logic mapper from American Video Entertainment.
 * 32KB PRG + 8KB CHR bank switching via address/data bus latch.
 * Register is written by putting data on D4-D7 while A15-A13 = $41xx.
 *
 * Games: Dudes with Attitude, Krazy Kreatures, Tiles of Fate, etc.
 *
 * Register ($4100-$5FFF):
 *   D5: PRG 32KB bank select
 *   D2-D0: CHR 8KB bank select
 */

#include "systems/nes/cartridge/nes_mapper.h"
#include "systems/nes/cartridge/mappers/mapper_helpers.h"

namespace nes_system {

class Mapper079 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper079(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

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
        // Responds to $4100-$5FFF (addr & $E100 == $4100)
        if ((addr & 0xE100) == 0x4100) {
            prg_bank_select_ = (data >> 3) & 0x01;
            chr_bank_select_ = data & 0x07;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
