#pragma once
/*
 * mapper_185_cnrom_protect.h — iNES Mapper 185 (CNROM with copy protection)
 *
 * Like Mapper 003 (CNROM) but includes a simple protection mechanism:
 * writes with specific low bits trigger CHR output; writes with other
 * values cause CHR reads to return open bus / all zeroes.
 *
 * Games: Bird Week, Mighty Bomb Jack (J), Spy vs Spy (J)
 *
 * Register ($8000-$FFFF):
 *   Same as CNROM but if low bits of written value don't match expected
 *   protection value, PPU reads from CHR space return open bus.
 *   In practice, any write with (data & 0x03) != 0 is accepted.
 */

#include "systems/nes/cartridge/nes_mapper.h"
#include "systems/nes/cartridge/mappers/mapper_helpers.h"

namespace nes_system {

class Mapper185 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    bool chr_enabled_ = false;

public:
    Mapper185(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        chr_enabled_ = false;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_fixed(config, prg_rom_, prg_rom_size_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        for (int i = 0; i < 8; i++) {
            uint32_t offset = i * 0x0400;
            if (chr_enabled_ && offset < chr_mem_size_) {
                config.chr_pages[i] = chr_mem_ + offset;
            } else {
                // Protection active — return base of CHR (will show garbage/blank)
                config.chr_pages[i] = chr_mem_;
            }
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            // Protection check: enable CHR only if low bits are non-zero
            // Different boards use different magic values, but (data & 0x03) != 0
            // is the most common check. Some variants check (data & 0x33) != 0.
            chr_enabled_ = (data & 0x03) != 0;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
