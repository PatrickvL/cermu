#pragma once
/*
 * mapper_088_namco_3433.h — iNES Mapper 088 (Namco 108 / 3433 / 3443)
 *
 * Simplified DxROM/TENGEN — similar to Mapper 206 and a subset of MMC3.
 * Uses two registers: bank select at $8000, bank data at $8001.
 * 6 bank registers: R0-R1 (2KB CHR), R2-R5 (1KB CHR), R6-R7 (8KB PRG).
 * No IRQ, no mirroring control.
 *
 * CHR banking has a twist: R0-R1 (2KB) banks only address $00-$3F (0-63),
 * while R2-R5 (1KB) banks add $40 to address the second CHR half.
 *
 * Games: Dragon Spirit, Quinty
 *
 * Registers:
 *   $8000: D2-D0 = register select (R0-R7, but only R0-R5,R6,R7 used)
 *   $8001: D7-D0 = data for selected register
 */

#include "../nes_mapper.h"
#include <cstring>

namespace nes_system {

class Mapper088 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t target_register_ = 0;
    uint8_t registers_[8] = {};

public:
    Mapper088(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        target_register_ = 0;
        std::memset(registers_, 0, sizeof(registers_));
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (num_8k == 0) num_8k = 1;
        uint32_t last = num_8k - 1;
        uint32_t second_last = (num_8k >= 2) ? num_8k - 2 : 0;

        uint32_t banks[4] = {
            (registers_[6] & 0x0F) % num_8k,    // $8000
            (registers_[7] & 0x0F) % num_8k,    // $A000
            second_last,                          // $C000
            last                                  // $E000
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
        uint32_t chr_1k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;

        // R0: 2KB at $0000 — stays in lower half
        uint32_t b = ((uint32_t)(registers_[0] & 0x3F) & 0xFE) % chr_1k;
        config.chr_pages[0] = chr_mem_ + (b * 0x0400);
        config.chr_pages[1] = chr_mem_ + (((b + 1) % chr_1k) * 0x0400);

        // R1: 2KB at $0800 — stays in lower half
        b = ((uint32_t)(registers_[1] & 0x3F) & 0xFE) % chr_1k;
        config.chr_pages[2] = chr_mem_ + (b * 0x0400);
        config.chr_pages[3] = chr_mem_ + (((b + 1) % chr_1k) * 0x0400);

        // R2-R5: 1KB at $1000-$1C00 — OR with $40 to use upper half
        for (int i = 0; i < 4; i++) {
            uint32_t bank = ((uint32_t)(registers_[2 + i] & 0x3F) | 0x40) % chr_1k;
            config.chr_pages[4 + i] = chr_mem_ + (bank * 0x0400);
        }

        for (int i = 0; i < 8; i++)
            config.chr_writable[i] = chr_is_ram_;
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        if (!(addr & 0x0001)) {
            // Even: bank select ($8000)
            target_register_ = data & 0x07;
        } else {
            // Odd: bank data ($8001)
            registers_[target_register_] = data;
        }
        return true;
    }
};

} // namespace nes_system
