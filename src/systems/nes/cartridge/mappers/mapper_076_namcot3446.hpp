#pragma once
/*
 * mapper_076_namcot3446.h — iNES Mapper 076 (Namcot 3446)
 *
 * Simplified Namcot 108: 2×8KB PRG + 4×2KB CHR, no 2KB CHR pairing.
 * Register interface identical to Namcot 108 ($8000 select, $8001 data)
 * but only R2-R5 select 2KB CHR banks individually, and R6/R7 select
 * 8KB PRG banks.  R0/R1 are unused.
 *
 * Games: Digital Devil Story - Megami Tensei.
 *
 * PRG: R6 → $8000, R7 → $A000, fixed $C000-$FFFF (last two 8KB).
 * CHR: R2 → $0000(2KB), R3 → $0800(2KB), R4 → $1000(2KB), R5 → $1800(2KB).
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper076 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t target_register_ = 0;
    uint8_t registers_[8] = {};

public:
    Mapper076(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        target_register_ = 0;
        std::memset(registers_, 0, sizeof(registers_));
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(registers_[6] & 0x3F) % n,
            static_cast<uint32_t>(registers_[7] & 0x3F) % n,
            (n >= 2) ? n - 2 : 0u, n - 1
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        using namespace mapper_helpers;
        uint8_t chr[4] = {registers_[2], registers_[3], registers_[4], registers_[5]};
        set_chr_2k_pages(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;
        if (!(addr & 0x0001))
            target_register_ = data & 0x07;
        else
            registers_[target_register_] = data;
        return true;
    }
};

} // namespace nes_system
