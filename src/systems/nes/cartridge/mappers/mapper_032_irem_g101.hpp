#pragma once
/*
 * mapper_032_irem_g101.h — iNES Mapper 032 (Irem G-101)
 *
 * 2 × 8KB switchable PRG + fixed last bank, 8 × 1KB CHR, mirroring control.
 * PRG mode bit swaps which slot ($8000 or $C000) is switchable vs fixed.
 *
 * Games: Image Fight, Kaiketsu Yanchamaru 2, Major League
 *
 * Registers:
 *   $8000: 8KB PRG bank at slot 0 (D5-D0)
 *   $9000: D1 = mirroring (0=vert, 1=horiz), D1 = PRG mode
 *   $A000: 8KB PRG bank at slot 1 (D5-D0)
 *   $B000-$B007: 1KB CHR banks 0-7 (D7-D0)
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper032 : public Mapper {
private:

    uint8_t prg_bank_[2] = {};       // Two switchable 8KB PRG banks
    uint8_t chr_bank_[8] = {};       // Eight 1KB CHR banks
    bool prg_mode_ = false;          // false = $8000 switchable, true = $C000 switchable
    Mirror mirror_mode_ = Mirror::VERTICAL;

public:
    Mapper032(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        prg_bank_[0] = 0;
        prg_bank_[1] = 0;
        for (int i = 0; i < 8; i++) chr_bank_[i] = i;
        prg_mode_ = false;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override {
        // Submapper 1 (Major League): fixed one-screen mirroring
        if (submapper_ == 1) return Mirror::ONESCREEN_LO;
        return mirror_mode_;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (num_8k == 0) num_8k = 1;
        uint32_t last = num_8k - 1;
        uint32_t second_last = (num_8k >= 2) ? num_8k - 2 : 0;

        uint32_t banks[4];
        if (!prg_mode_) {
            banks[0] = prg_bank_[0] % num_8k;   // $8000
            banks[1] = prg_bank_[1] % num_8k;   // $A000
            banks[2] = second_last;              // $C000 = second-to-last
            banks[3] = last;                     // $E000 = last
        } else {
            banks[0] = second_last;              // $8000 = second-to-last
            banks[1] = prg_bank_[1] % num_8k;   // $A000
            banks[2] = prg_bank_[0] % num_8k;   // $C000
            banks[3] = last;                     // $E000 = last
        }

        for (int slot = 0; slot < 4; slot++) {
            uint32_t base = banks[slot] * 0x2000;
            for (int h = 0; h < 2; h++) {
                uint32_t off = base + h * 0x1000;
                config.prg_pages[slot * 2 + h] = (off < prg_rom_size_) ? prg_rom_ + off : nullptr;
            }
        }
        config.prg_ram_enabled = (prg_ram_ != nullptr);
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_1k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;
        for (int i = 0; i < 8; i++) {
            uint32_t bank = (chr_1k > 0) ? (chr_bank_[i] % chr_1k) : 0;
            uint32_t offset = bank * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        uint16_t reg = addr & 0xF000;

        if (reg == 0x8000) {
            prg_bank_[0] = data & 0x3F;
            return true;
        } else if (reg == 0x9000) {
            mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            prg_mode_ = (data & 0x02) != 0;
            return true;
        } else if (reg == 0xA000) {
            prg_bank_[1] = data & 0x3F;
            return true;
        } else if (reg == 0xB000) {
            uint8_t index = addr & 0x07;
            chr_bank_[index] = data;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
