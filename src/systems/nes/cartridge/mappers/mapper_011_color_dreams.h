#pragma once
/*
 * mapper_011_color_dreams.h — iNES Mapper 011 (Color Dreams)
 *
 * Simple discrete-logic mapper: a single latch selects both PRG and CHR banks.
 * 32KB PRG bank + 8KB CHR bank selection via writes to $8000-$FFFF.
 * Bus conflicts are present on real hardware.
 * Games: Crystal Mines, Bible Adventures, Wisdom Tree games, etc.
 *
 * Register ($8000-$FFFF):
 *   D7-D4: CHR 8KB bank select
 *   D1-D0: PRG 32KB bank select
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper011 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper011(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_select_ = 0;
    }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // Switchable 32KB PRG bank
        uint32_t max_prg_banks = static_cast<uint32_t>(prg_rom_size_ / 0x8000);
        uint32_t bank = (max_prg_banks > 0) ? (prg_bank_select_ % max_prg_banks) : 0;
        uint32_t bank_base = bank * 0x8000;
        for (int i = 0; i < 8; i++) {
            uint32_t offset = bank_base + i * 0x1000;
            config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }
        // Color Dreams has no PRG-RAM
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // Switchable 8KB CHR bank
        uint32_t max_chr_banks = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x2000) : 1;
        uint32_t bank = (max_chr_banks > 0) ? (chr_bank_select_ % max_chr_banks) : 0;
        uint32_t chr_base = bank * 0x2000;
        for (int i = 0; i < 8; i++) {
            uint32_t offset = chr_base + i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            prg_bank_select_ = data & 0x03;
            chr_bank_select_ = (data >> 4) & 0x0F;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
