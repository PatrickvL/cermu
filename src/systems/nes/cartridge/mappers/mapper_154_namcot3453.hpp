#pragma once
/*
 * mapper_154_namcot3453.h — iNES Mapper 154 (Namcot 3453)
 *
 * Variant of Namcot 108 / 088: register select uses D6 for single-screen
 * mirroring control.  D6=0 → CIRAM page 0, D6=1 → CIRAM page 1.
 * CHR uses the same split as mapper 088 (R0-R1 lower half, R2-R5 | $40).
 * PRG: 2×8KB switchable + 2×8KB fixed (identical to Namcot 108).
 *
 * Games: Devil Man.
 *
 * $8000 (even): D2-D0 = register select, D6 = nametable page
 * $8001 (odd):  D7-D0 = bank data
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper154 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t target_register_ = 0;
    uint8_t registers_[8] = {};
    Mirror mirror_mode_ = Mirror::ONESCREEN_LO;

public:
    Mapper154(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        target_register_ = 0;
        std::memset(registers_, 0, sizeof(registers_));
        mirror_mode_ = Mirror::ONESCREEN_LO;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(registers_[6] & 0x0F) % n,
            static_cast<uint32_t>(registers_[7] & 0x0F) % n,
            (n >= 2) ? n - 2 : 0u, n - 1
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_1k = mapper_helpers::chr_1k_count(chr_mem_size_);

        // R0/R1: 2KB from lower half; R2-R5: 1KB from upper half (| $40)
        uint32_t b = (static_cast<uint32_t>(registers_[0] & 0x3F) & 0xFE) % chr_1k;
        config.chr_pages[0] = chr_mem_ + (b * 0x0400);
        config.chr_pages[1] = chr_mem_ + (((b + 1) % chr_1k) * 0x0400);

        b = (static_cast<uint32_t>(registers_[1] & 0x3F) & 0xFE) % chr_1k;
        config.chr_pages[2] = chr_mem_ + (b * 0x0400);
        config.chr_pages[3] = chr_mem_ + (((b + 1) % chr_1k) * 0x0400);

        for (int i = 0; i < 4; i++) {
            uint32_t bank = (static_cast<uint32_t>(registers_[2 + i] & 0x3F) | 0x40) % chr_1k;
            config.chr_pages[4 + i] = chr_mem_ + (bank * 0x0400);
        }

        for (int i = 0; i < 8; i++)
            config.chr_writable[i] = chr_is_ram_;
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;
        if (!(addr & 0x0001)) {
            target_register_ = data & 0x07;
            mirror_mode_ = (data & 0x40) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
        } else {
            registers_[target_register_] = data;
        }
        return true;
    }
};

} // namespace nes_system
