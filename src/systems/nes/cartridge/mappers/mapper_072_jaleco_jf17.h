#pragma once
/*
 * mapper_072_jaleco_jf17.h — iNES Mapper 072 (Jaleco JF-17 / JF-26)
 *
 * 16KB switchable + 16KB fixed PRG, 8KB CHR banking.
 * Uses acknowledge-style latching: D7 (PRG) and D6 (CHR) select which
 * bank is loaded when the bit transitions from 1→0 (falling edge detect).
 *
 * Games: Pinball Quest, Moero!! Pro Tennis
 *
 * Register ($8000-$FFFF):
 *   On write: latch D3-D0 as potential bank number.
 *   D7 high = arm PRG bank select; next write with D7 low = apply PRG bank
 *   D6 high = arm CHR bank select; next write with D6 low = apply CHR bank
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper072 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;
    uint8_t prev_write_ = 0;     // Previous write to detect falling edges

public:
    Mapper072(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_select_ = 0;
        prev_write_ = 0;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
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
        uint32_t max_8k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x2000) : 1;
        uint32_t bank = (max_8k > 0) ? (chr_bank_select_ % max_8k) : 0;
        uint32_t base = bank * 0x2000;
        for (int i = 0; i < 8; i++) {
            uint32_t offset = base + i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        bool changed = false;

        // Detect falling edge of D7 (PRG select)
        if ((prev_write_ & 0x80) && !(data & 0x80)) {
            prg_bank_select_ = data & 0x0F;
            changed = true;
        }

        // Detect falling edge of D6 (CHR select)
        if ((prev_write_ & 0x40) && !(data & 0x40)) {
            chr_bank_select_ = data & 0x0F;
            changed = true;
        }

        prev_write_ = data;
        return changed;
    }
};

} // namespace nes_system
