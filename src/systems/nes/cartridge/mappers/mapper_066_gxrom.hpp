#pragma once
/*
 * mapper_066_gxrom.h — iNES Mapper 066 (GxROM / MxROM)
 *
 * Simple discrete-logic mapper: 32KB PRG + 8KB CHR bank switching
 * via a single latch at $8000-$FFFF.
 * No bus conflicts on most boards.
 * Games: Super Mario Bros. + Duck Hunt, Doraemon, etc.
 *
 * Register ($8000-$FFFF):
 *   D5-D4: PRG 32KB bank select
 *   D1-D0: CHR 8KB bank select
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper066 : public Mapper {
private:
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper066(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

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
        if (addr >= 0x8000) {
            data = mapper_helpers::apply_bus_conflict(data, prg_rom_, prg_rom_size_, addr);
            prg_bank_select_ = (data >> 4) & 0x03;
            chr_bank_select_ = data & 0x03;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
