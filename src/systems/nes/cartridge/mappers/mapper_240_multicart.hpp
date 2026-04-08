#pragma once
/*
 * mapper_240_multicart.h — iNES Mapper 240 (simple multicart)
 *
 * Simple multicart mapper: 32KB PRG + 8KB CHR bank switching
 * via a register in the expansion area $4020-$5FFF.
 *
 * Games: Various Chinese/Taiwanese multicarts (Jing Ke Xin Zhuan, etc.)
 *
 * Register ($4020-$5FFF):
 *   D7-D4: PRG 32KB bank select (high nibble)
 *   D3-D0: CHR 8KB bank select  (low nibble)
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper240 : public Mapper {
private:
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper240(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

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
        if (addr >= 0x4020 && addr < 0x6000) {
            prg_bank_select_ = (data >> 4) & 0x0F;
            chr_bank_select_ = data & 0x0F;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
