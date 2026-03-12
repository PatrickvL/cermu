#pragma once
/*
 * mapper_193_ntdec.h — iNES Mapper 193 (NTDEC TC-112 / Fighting Hero)
 *
 * Simple CHR banking + PRG switching via writes to $6000-$6003.
 * Fixed last 16KB PRG at $C000. Switchable 16KB at $8000 (via $6003).
 * CHR: 4KB + 2KB + 2KB pattern (via $6000-$6002).
 *
 * Games: Fighting Hero, War on Wheels
 *
 * Registers ($6000-$6003):
 *   $6000: D7-D2 = 4KB CHR bank at $0000 (value >> 2)
 *   $6001: D7-D1 = 2KB CHR bank at $1000 (value >> 1)
 *   $6002: D7-D1 = 2KB CHR bank at $1800 (value >> 1)
 *   $6003: D7-D0 = 16KB PRG bank at $8000
 */

#include "systems/nes/cartridge/nes_mapper.h"
#include "systems/nes/cartridge/mappers/mapper_helpers.h"

namespace nes_system {

class Mapper193 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t prg_bank_select_ = 0;
    uint8_t chr_4k_ = 0;     // 4KB CHR bank at $0000
    uint8_t chr_2k_0_ = 0;   // 2KB CHR bank at $1000
    uint8_t chr_2k_1_ = 0;   // 2KB CHR bank at $1800

public:
    Mapper193(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_4k_ = 0;
        chr_2k_0_ = 0;
        chr_2k_1_ = 0;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_1k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;

        // $0000-$0FFF: 4KB bank (register value >> 2)
        uint32_t b4k = ((uint32_t)(chr_4k_ >> 2) * 4) % chr_1k;
        for (int i = 0; i < 4; i++) {
            config.chr_pages[i] = chr_mem_ + (((b4k + i) % chr_1k) * 0x0400);
        }

        // $1000-$17FF: 2KB bank (register value >> 1)
        uint32_t b2k_0 = ((uint32_t)(chr_2k_0_ >> 1) * 2) % chr_1k;
        config.chr_pages[4] = chr_mem_ + ((b2k_0 % chr_1k) * 0x0400);
        config.chr_pages[5] = chr_mem_ + (((b2k_0 + 1) % chr_1k) * 0x0400);

        // $1800-$1FFF: 2KB bank (register value >> 1)
        uint32_t b2k_1 = ((uint32_t)(chr_2k_1_ >> 1) * 2) % chr_1k;
        config.chr_pages[6] = chr_mem_ + ((b2k_1 % chr_1k) * 0x0400);
        config.chr_pages[7] = chr_mem_ + (((b2k_1 + 1) % chr_1k) * 0x0400);

        for (int i = 0; i < 8; i++)
            config.chr_writable[i] = chr_is_ram_;
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x6000 && addr <= 0x6003) {
            switch (addr & 0x03) {
                case 0: chr_4k_ = data;         return true;
                case 1: chr_2k_0_ = data;       return true;
                case 2: chr_2k_1_ = data;       return true;
                case 3: prg_bank_select_ = data; return true;
            }
        }
        return false;
    }
};

} // namespace nes_system
