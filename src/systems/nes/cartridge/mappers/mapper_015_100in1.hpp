#pragma once
/*
 * mapper_015_100in1.h — iNES Mapper 015 (100-in-1 Contra Function 16)
 *
 * Multicart mapper with 4 PRG banking modes selected by address bits.
 * No CHR banking (uses 8KB CHR-RAM).
 *
 * Register ($8000-$FFFF):
 *   Address A1-A0 selects mode (0-3).
 *   D7:    Mirroring (0 = vertical, 1 = horizontal)
 *   D5-D0: PRG bank selection
 *
 * Mode 0: 32KB PRG (bank >> 1, consecutive pair)
 * Mode 1: 128KB — 16KB switchable at $8000, 16KB fixed last at $C000
 * Mode 2: 16KB repeated (same 16KB in both $8000 and $C000)
 * Mode 3: 16KB switchable at $8000, next 16KB at $C000
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper015 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t mode_ = 0;
    uint8_t bank_select_ = 0;
    Mirror mirror_mode_ = Mirror::VERTICAL;

public:
    Mapper015(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        mode_ = 0;
        bank_select_ = 0;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (num_16k == 0) num_16k = 1;
        uint32_t bank = bank_select_ % num_16k;
        uint32_t last = num_16k - 1;

        uint32_t lo_bank, hi_bank;

        switch (mode_) {
            case 0:  // 32KB: two consecutive 16KB banks
                lo_bank = (bank & 0xFE) % num_16k;
                hi_bank = (lo_bank | 1) % num_16k;
                break;
            case 1:  // 16KB switchable + fixed last
                lo_bank = bank;
                hi_bank = last;
                break;
            case 2:  // 16KB repeated
                lo_bank = bank;
                hi_bank = bank;
                break;
            case 3:  // 16KB + next 16KB
            default:
                lo_bank = bank;
                hi_bank = (bank | 1) % num_16k;
                break;
        }

        // $8000-$BFFF
        uint32_t lo_base = lo_bank * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t off = lo_base + i * 0x1000;
            config.prg_pages[i] = (off < prg_rom_size_) ? prg_rom_ + off : nullptr;
        }

        // $C000-$FFFF
        uint32_t hi_base = hi_bank * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t off = hi_base + i * 0x1000;
            config.prg_pages[4 + i] = (off < prg_rom_size_) ? prg_rom_ + off : nullptr;
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // 8KB CHR-RAM
        for (int i = 0; i < 8; i++) {
            uint32_t offset = i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = true;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            mode_ = addr & 0x03;
            bank_select_ = data & 0x3F;
            mirror_mode_ = (data & 0x80) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
