#pragma once
/*
 * mapper_097_irem_tam_s1.h — iNES Mapper 097 (Irem TAM-S1)
 *
 * Unusual 16KB PRG layout: fixed FIRST bank at $8000, switchable at $C000.
 *   $8000-$FFFF write: D3-D0 = 16KB PRG bank at $C000.
 *   D6-D7 = mirroring (0=H, 1=V, 2/3=one-screen).
 *   No CHR switching (CHR-RAM or fixed CHR-ROM).
 *
 * Games: Kaiketsu Yanchamaru.
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper097 : public Mapper {
private:
    uint8_t prg_bank_ = 0;
    Mirror mirror_mode_ = Mirror::VERTICAL;

public:
    Mapper097(uint8_t prgBanks, uint8_t chrBanks) { (void)prgBanks; (void)chrBanks; }

    void reset() override { prg_bank_ = 0; mirror_mode_ = Mirror::VERTICAL; }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // Fixed first (bank 0) at $8000, switchable at $C000
        mapper_helpers::set_prg_16k_hi(config, prg_rom_, prg_rom_size_, prg_bank_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;
        prg_bank_ = data & 0x0F;
        switch ((data >> 6) & 0x03) {
            case 0: mirror_mode_ = Mirror::HORIZONTAL;   break;
            case 1: mirror_mode_ = Mirror::VERTICAL;     break;
            case 2: mirror_mode_ = Mirror::ONESCREEN_LO; break;
            case 3: mirror_mode_ = Mirror::ONESCREEN_HI; break;
        }
        return true;
    }
};

} // namespace nes_system
