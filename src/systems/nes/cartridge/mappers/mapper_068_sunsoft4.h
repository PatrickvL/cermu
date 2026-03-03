#pragma once
/*
 * mapper_068_sunsoft4.h — iNES Mapper 068 (Sunsoft-4)
 *
 * 16KB switchable + 16KB fixed PRG, 4×2KB CHR banks.
 * Unique nametable mapping feature: NT slots can use CHR-ROM pages.
 * Games: After Burner, Maharaja.
 *
 * $8000: CHR 2KB bank 0 ($0000)
 * $9000: CHR 2KB bank 1 ($0800)
 * $A000: CHR 2KB bank 2 ($1000)
 * $B000: CHR 2KB bank 3 ($1800)
 * $C000: NT register 0 (for CIRAM replacement with CHR-ROM)
 * $D000: NT register 1
 * $E000: Mirroring + NT source
 * $F000: PRG 16KB bank select
 */

#include "../nes_mapper.h"
#include "mapper_helpers.h"

namespace nes_system {

class Mapper068 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_[4] = {};

    uint8_t nt_reg_[2] = {};   // NT CHR-ROM bank registers
    bool use_chr_for_nt_ = false;
    Mirror mirror_mode_ = Mirror::VERTICAL;

public:
    Mapper068(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        for (int i = 0; i < 4; i++) chr_bank_[i] = i;
        nt_reg_[0] = 0;
        nt_reg_[1] = 0;
        use_chr_for_nt_ = false;
        mirror_mode_ = header_mirror_;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_select_);
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_2k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0800) : 1;
        if (chr_2k == 0) chr_2k = 1;

        for (int slot = 0; slot < 4; slot++) {
            uint32_t b = chr_bank_[slot] % chr_2k;
            for (int i = 0; i < 2; i++) {
                uint32_t offset = b * 0x0800 + i * 0x0400;
                config.chr_pages[slot * 2 + i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                config.chr_writable[slot * 2 + i] = chr_is_ram_;
            }
        }
        // NT mapping using CHR-ROM is not supported through the standard
        // MapperChrConfig interface — would require bus-level integration.
        // For now, standard CIRAM mirroring is used.
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xF000) {
            case 0x8000: chr_bank_[0] = data; return true;
            case 0x9000: chr_bank_[1] = data; return true;
            case 0xA000: chr_bank_[2] = data; return true;
            case 0xB000: chr_bank_[3] = data; return true;
            case 0xC000: nt_reg_[0] = data | 0x80; return false;
            case 0xD000: nt_reg_[1] = data | 0x80; return false;
            case 0xE000:
                use_chr_for_nt_ = (data & 0x10) != 0;
                switch (data & 0x03) {
                    case 0: mirror_mode_ = Mirror::VERTICAL;     break;
                    case 1: mirror_mode_ = Mirror::HORIZONTAL;   break;
                    case 2: mirror_mode_ = Mirror::ONESCREEN_LO; break;
                    case 3: mirror_mode_ = Mirror::ONESCREEN_HI; break;
                }
                return true;
            case 0xF000:
                prg_bank_select_ = data;
                return true;
        }
        return false;
    }
};

} // namespace nes_system
