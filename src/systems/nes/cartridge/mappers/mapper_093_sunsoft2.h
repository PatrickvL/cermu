#pragma once
/*
 * mapper_093_sunsoft2.h — iNES Mapper 093 (Sunsoft-2 IC on Sunsoft-3R board)
 *
 * Simple PRG switching with optional CHR-ROM enable.
 * 16KB switchable PRG at $8000, 16KB fixed at $C000.
 * Uses CHR-ROM (no CHR switching).
 *
 * Games: Fantasy Zone (J)
 *
 * Register ($8000-$FFFF):
 *   D6-D4: 16KB PRG bank at $8000
 *   D0:    CHR-ROM enable (1 = enabled)
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper093 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;

public:
    Mapper093(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_16k = prg_rom_size_ / 0x4000;
        if (num_16k == 0) num_16k = 1;

        // $8000-$BFFF: switchable
        uint32_t lo_bank = prg_bank_select_ % num_16k;
        uint32_t lo_base = lo_bank * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t off = lo_base + i * 0x1000;
            config.prg_pages[i] = (off < prg_rom_size_) ? prg_rom_ + off : nullptr;
        }

        // $C000-$FFFF: fixed to last 16KB
        uint32_t hi_base = (num_16k - 1) * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t off = hi_base + i * 0x1000;
            config.prg_pages[4 + i] = (off < prg_rom_size_) ? prg_rom_ + off : nullptr;
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // Fixed 8KB CHR (ROM or RAM)
        for (int i = 0; i < 8; i++) {
            uint32_t offset = i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            prg_bank_select_ = (data >> 4) & 0x07;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
