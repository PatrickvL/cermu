#pragma once
/*
 * mapper_041_caltron.h — iNES Mapper 041 (Caltron 6-in-1)
 *
 * Multicart with two register ranges:
 *   $6000-$67FF: Outer bank select (from address bits)
 *   $8000-$FFFF: Inner CHR bank select (when enabled)
 *
 * Games: Caltron 6-in-1
 *
 * Register $6000-$67FF (address-based):
 *   A2-A0: 32KB PRG bank
 *   A3:    CHR bank high bit
 *   A4:    CHR bank mid bit
 *   A5:    Inner CHR enable
 *
 * Register $8000-$FFFF (when inner CHR enabled):
 *   D1-D0: CHR bank low bits
 */

#include "systems/nes/cartridge/nes_mapper.h"
#include "systems/nes/cartridge/mappers/mapper_helpers.h"

namespace nes_system {

class Mapper041 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t prg_bank_select_ = 0;
    uint8_t chr_outer_ = 0;
    uint8_t chr_inner_ = 0;
    bool inner_chr_enabled_ = false;
    Mirror mirror_mode_ = Mirror::VERTICAL;

public:
    Mapper041(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_outer_ = 0;
        chr_inner_ = 0;
        inner_chr_enabled_ = false;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_32k(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint8_t chr_bank = static_cast<uint8_t>((chr_outer_ << 2) | chr_inner_);
        mapper_helpers::set_chr_8k(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x6000 && addr <= 0x67FF) {
            prg_bank_select_ = addr & 0x07;
            chr_outer_ = (addr >> 3) & 0x03;
            inner_chr_enabled_ = (addr & 0x20) != 0;
            mirror_mode_ = (addr & 0x40) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            return true;
        }
        if (addr >= 0x8000 && inner_chr_enabled_) {
            chr_inner_ = data & 0x03;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
