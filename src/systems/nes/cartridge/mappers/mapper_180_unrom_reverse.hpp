#pragma once
/*
 * mapper_180_unrom_reverse.h — iNES Mapper 180 (UNROM with reversed fixed bank)
 *
 * Like UNROM (mapper 002) but the FIRST 16KB bank is fixed at $8000
 * and the switchable bank is at $C000.
 *   $8000-$FFFF write: D2-D0 = 16KB PRG bank at $C000.
 *   No CHR switching (CHR-RAM).
 *
 * Games: Crazy Climber (Japan).
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper180 : public Mapper {
private:
    uint8_t prg_bank_ = 0;

public:
    Mapper180(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override { prg_bank_ = 0; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // Fixed first (bank 0) at $8000, switchable at $C000
        mapper_helpers::set_prg_16k_hi(config, prg_rom_, prg_rom_size_, prg_bank_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;
        prg_bank_ = data & 0x07;
        return true;
    }
};

} // namespace nes_system
