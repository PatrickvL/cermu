#pragma once
/*
 * mapper_113_nina06.h — iNES Mapper 113 (NINA-03/06 HES variant)
 *
 * Similar to mapper 079 but with larger bank ranges.
 * 32KB PRG + 8KB CHR bank switching via writes to $4100-$5FFF.
 * Used by HES multicart games and some AVE titles.
 *
 * Register ($4100-$5FFF):
 *   D7:    Mirroring (0=vertical, 1=horizontal)  [on some boards]
 *   D6:    CHR A15 (high bit of 8KB CHR bank)
 *   D5-D3: PRG 32KB bank (3 bits → 8 banks)
 *   D2-D0: CHR A14-A12 (low 3 bits of 8KB CHR bank)
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper113 : public Mapper {
private:
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;
    Mirror mirror_mode_ = Mirror::VERTICAL;

public:
    Mapper113(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_select_ = 0;
        mirror_mode_ = header_mirror_;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_32k(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_select_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if ((addr & 0xE100) == 0x4100) {
            prg_bank_select_ = (data >> 3) & 0x07;
            chr_bank_select_ = (data & 0x07) | ((data >> 3) & 0x08);
            mirror_mode_ = (data & 0x80) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
