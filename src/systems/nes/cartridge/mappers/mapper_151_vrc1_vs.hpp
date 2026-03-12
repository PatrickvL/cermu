#pragma once
/*
 * mapper_151_vrc1_vs.h — iNES Mapper 151 (Konami VRC1 - Vs. System variant)
 *
 * Similar to Mapper 075 (VRC1) but simplified for Vs. System boards.
 * 3 × 8KB switchable PRG + 8KB fixed, 2 × 4KB CHR.
 * No mirroring control (handled by Vs. System hardware).
 *
 * Games: Vs. The Goonies, Vs. Gradius
 *
 * Registers:
 *   $8000: D3-D0 = 8KB PRG at $8000
 *   $A000: D3-D0 = 8KB PRG at $A000
 *   $C000: D3-D0 = 8KB PRG at $C000
 *   $E000: D3-D0 = 4KB CHR at $0000
 *   $F000: D3-D0 = 4KB CHR at $1000
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper151 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t prg_bank_[3] = {};
    uint8_t chr_bank_[2] = {};

public:
    Mapper151(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_[0] = prg_bank_[1] = prg_bank_[2] = 0;
        chr_bank_[0] = chr_bank_[1] = 0;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (num_8k == 0) num_8k = 1;
        uint32_t last = num_8k - 1;

        uint32_t banks[4] = {
            prg_bank_[0] % num_8k,
            prg_bank_[1] % num_8k,
            prg_bank_[2] % num_8k,
            last
        };

        for (int slot = 0; slot < 4; slot++) {
            uint32_t base = banks[slot] * 0x2000;
            for (int h = 0; h < 2; h++) {
                uint32_t off = base + h * 0x1000;
                config.prg_pages[slot * 2 + h] = (off < prg_rom_size_) ? prg_rom_ + off : nullptr;
            }
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t max_4k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x1000) : 1;

        for (int half = 0; half < 2; half++) {
            uint32_t bank = (max_4k > 0) ? (chr_bank_[half] % max_4k) : 0;
            uint32_t base = bank * 0x1000;
            for (int i = 0; i < 4; i++) {
                uint32_t off = base + i * 0x0400;
                config.chr_pages[half * 4 + i] = (off < chr_mem_size_) ? chr_mem_ + off : chr_mem_;
                config.chr_writable[half * 4 + i] = chr_is_ram_;
            }
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xF000) {
            case 0x8000: prg_bank_[0] = data & 0x0F; return true;
            case 0xA000: prg_bank_[1] = data & 0x0F; return true;
            case 0xC000: prg_bank_[2] = data & 0x0F; return true;
            case 0xE000: chr_bank_[0] = data & 0x0F; return true;
            case 0xF000: chr_bank_[1] = data & 0x0F; return true;
            default: return false;
        }
    }
};

} // namespace nes_system
