#pragma once
/*
 * mapper_007_axrom.h — iNES Mapper 007 (AxROM)
 *
 * 32KB PRG bank switching with single-screen mirroring control.
 * No CHR banking — uses 8KB CHR-RAM.
 * Bus conflicts exist on real hardware but are rarely relevant.
 * Games: Battletoads, Marble Madness, Wizards & Warriors, etc.
 *
 * Register ($8000-$FFFF):
 *   D4: Nametable select (0 = lower, 1 = upper → one-screen mirroring)
 *   D2-D0: 32KB PRG bank select
 */

#include "systems/nes/cartridge/nes_mapper.h"
#include "systems/nes/cartridge/mappers/mapper_helpers.h"

namespace nes_system {

class Mapper007 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;
    Mirror mirror_mode_ = Mirror::ONESCREEN_LO;

public:
    Mapper007(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        mirror_mode_ = Mirror::ONESCREEN_LO;
    }

    Mirror mirror() override { return mirror_mode_; }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_32k(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, true);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            prg_bank_select_ = data & 0x07;
            mirror_mode_ = (data & 0x10) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
