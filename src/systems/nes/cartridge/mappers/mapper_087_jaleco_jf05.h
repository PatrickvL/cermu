#pragma once
/*
 * mapper_087_jaleco_jf05.h — iNES Mapper 087 (Jaleco JF-05/06/07/08/09)
 *
 * Very simple discrete CHR bank switching via the PRG-RAM port.
 * Fixed PRG (16KB or 32KB), switchable 8KB CHR-ROM.
 * Write to $6000-$7FFF: CHR bank = bit swap of D1-D0.
 *
 * Games: Argus, City Connection (J), Ninja Jajamaru Kun
 *
 * Register ($6000-$7FFF):
 *   CHR bank = ((D1 >> 1) | (D0 << 1))  — bits are swapped
 */

#include "systems/nes/cartridge/nes_mapper.h"
#include "systems/nes/cartridge/mappers/mapper_helpers.h"

namespace nes_system {

class Mapper087 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper087(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        chr_bank_select_ = 0;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_fixed(config, prg_rom_, prg_rom_size_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_select_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x6000 && addr < 0x8000) {
            // Swap bits: CHR bank = (D1 >> 1) | (D0 << 1)
            chr_bank_select_ = ((data & 0x02) >> 1) | ((data & 0x01) << 1);
            return true;
        }
        return false;
    }
};

} // namespace nes_system
