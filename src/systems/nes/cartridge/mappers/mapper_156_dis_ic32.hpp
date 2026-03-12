#pragma once
/*
 * mapper_156_dis_ic32.h — iNES Mapper 156 (Daou Infosys DIS IC32)
 *
 * Unlicensed Korean board (DAOU ROM Controller DIS23C01 DAOU 245 ASIC).
 * Used by Koko Adventure (a.k.a. Buzz & Waldog), Metal Force,
 * General's Son, and a few other Daou Infosys titles.
 *
 * Banking:
 *   PRG: 16KB switchable at $8000, fixed last bank at $C000-$FFFF.
 *   CHR: 8 independently switchable 1KB banks (9-bit bank numbers).
 *   RAM: 8KB PRG-RAM at $6000-$7FFF.
 *
 * Registers (directly addressed):
 *   $C000-$C003  CHR low byte  (banks 0-3, PPU $0000-$0FFF)
 *   $C004-$C007  CHR high bit  (banks 0-3, bit 0 = bank bit 8)
 *   $C008-$C00B  CHR low byte  (banks 4-7, PPU $1000-$1FFF)
 *   $C00C-$C00F  CHR high bit  (banks 4-7, bit 0 = bank bit 8)
 *   $C010        PRG bank      (CPU $8000-$BFFF)
 *   $C014        Mirroring     (00=vert, 01=horiz, 1x=one-screen)
 *
 * Mirroring defaults to one-screen (CIRAM page 0).
 *
 * Reference: https://www.nesdev.org/wiki/INES_Mapper_156
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper156 : public Mapper {
private:
    // 9-bit CHR bank registers: low 8 bits + high 1 bit
    uint8_t chr_lo_[8] = {};   // Low bytes  ($C000-$C003, $C008-$C00B)
    uint8_t chr_hi_[8] = {};   // High bits  ($C004-$C007, $C00C-$C00F)
    uint8_t prg_bank_  = 0;    // 16KB PRG bank at $8000
    uint8_t mirror_reg_ = 2;   // Default: one-screen

public:
    Mapper156(uint8_t prgBanks, uint8_t chrBanks) {
        (void)prgBanks; (void)chrBanks;
    }

    void reset() override {
        for (int i = 0; i < 8; i++) { chr_lo_[i] = 0; chr_hi_[i] = 0; }
        prg_bank_  = 0;
        mirror_reg_ = 2;  // one-screen default
    }

    Mirror mirror() override {
        switch (mirror_reg_ & 0x03) {
            case 0:  return Mirror::VERTICAL;
            case 1:  return Mirror::HORIZONTAL;
            default: return Mirror::ONESCREEN_LO;  // 2 and 3
        }
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_);

        // Enable 8KB PRG-RAM at $6000-$7FFF
        if (prg_ram_ && prg_ram_size_ > 0) {
            config.prg_ram_base = prg_ram_;
            config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
            config.prg_ram_enabled = true;
            config.prg_ram_write_protected = false;
        }
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // Nametable mirroring from register
        Mirror m = const_cast<Mapper156*>(this)->mirror();
        const auto& nt = MIRROR_NT_PAGES[static_cast<int>(m)];
        for (int i = 0; i < 4; i++) config.nt_page[i] = nt[i];

        // 8 independently switchable 1KB CHR banks (9-bit bank numbers)
        uint32_t chr_1k_count = chr_mem_size_ > 0
            ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;
        if (chr_1k_count == 0) chr_1k_count = 1;

        for (int i = 0; i < 8; i++) {
            uint32_t bank = (static_cast<uint32_t>(chr_hi_[i] & 0x01) << 8)
                          |  static_cast<uint32_t>(chr_lo_[i]);
            bank %= chr_1k_count;
            config.chr_pages[i]    = chr_mem_ + bank * 0x0400;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        switch (addr) {
            // CHR low bytes — banks 0-3 (PPU $0000-$0FFF)
            case 0xC000: chr_lo_[0] = data; return true;
            case 0xC001: chr_lo_[1] = data; return true;
            case 0xC002: chr_lo_[2] = data; return true;
            case 0xC003: chr_lo_[3] = data; return true;
            // CHR high bits — banks 0-3
            case 0xC004: chr_hi_[0] = data; return true;
            case 0xC005: chr_hi_[1] = data; return true;
            case 0xC006: chr_hi_[2] = data; return true;
            case 0xC007: chr_hi_[3] = data; return true;
            // CHR low bytes — banks 4-7 (PPU $1000-$1FFF)
            case 0xC008: chr_lo_[4] = data; return true;
            case 0xC009: chr_lo_[5] = data; return true;
            case 0xC00A: chr_lo_[6] = data; return true;
            case 0xC00B: chr_lo_[7] = data; return true;
            // CHR high bits — banks 4-7
            case 0xC00C: chr_hi_[4] = data; return true;
            case 0xC00D: chr_hi_[5] = data; return true;
            case 0xC00E: chr_hi_[6] = data; return true;
            case 0xC00F: chr_hi_[7] = data; return true;
            // PRG bank select
            case 0xC010: prg_bank_ = data & 0x0F; return true;
            // Nametable mirroring
            case 0xC014: mirror_reg_ = data;       return true;
            default:     return false;
        }
    }
};

} // namespace nes_system
