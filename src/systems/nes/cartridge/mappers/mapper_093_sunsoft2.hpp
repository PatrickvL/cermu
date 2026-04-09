#pragma once
/*
 * mapper_093_sunsoft2.h — iNES Mapper 093 (Sunsoft-2 IC on Sunsoft-3R board)
 *
 * Simple PRG switching with optional CHR-ROM enable.
 * 16KB switchable PRG at $8000, 16KB fixed at $C000.
 * Uses CHR-ROM (no CHR switching).
 *
 * Games: Fantasy Zone (J)
 *
 * Register ($8000-$FFFF):
 *   D6-D4: 16KB PRG bank at $8000
 *   D0:    CHR-ROM enable (1 = enabled)
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper093 : public Mapper {
private:
    uint8_t prg_bank_select_ = 0;

public:
    Mapper093(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        prg_bank_select_ = 0;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            data = mapper_helpers::apply_bus_conflict(data, *this, addr);
            prg_bank_select_ = (data >> 4) & 0x07;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
