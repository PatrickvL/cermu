#pragma once
/*
 * mapper_003_cnrom.h — iNES Mapper 003 (CNROM)
 *
 * Simple CHR bank switching: writes to $8000-$FFFF select an 8KB CHR bank.
 * PRG is 16KB or 32KB fixed (same as NROM).
 * Games: Galaxian, Gradius, Arkista's Ring, etc.
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper003 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper003(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override { chr_bank_select_ = 0; }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // Fixed PRG — same as NROM
        mapper_helpers::set_prg_fixed(config, prg_rom_, prg_rom_size_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k(config, chr_mem_, chr_mem_size_, false, chr_bank_select_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            chr_bank_select_ = data & 0x03;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
