#pragma once
/*
 * mapper_095_namco_3425.h — iNES Mapper 095 (Namco 3425 / Namcot 108 variant)
 *
 * Like Mapper 088/206 but CHR bank D5 controls nametable mirroring.
 * Uses the standard Namcot-108 register interface ($8000/$8001).
 * The CHR bank D5 bit for the currently selected CHR slot maps the
 * corresponding nametable to CIRAM page 0 or 1.
 *
 * Games: Dragon Buster (J)
 *
 * Registers:
 *   $8000: D2-D0 = register select (R0-R7)
 *   $8001: D7-D0 = data for selected register
 *           For CHR registers: D5 controls nametable mirroring for that region
 */

#include "../nes_mapper.h"
#include <cstring>

namespace nes_system {

class Mapper095 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t target_register_ = 0;
    uint8_t registers_[8] = {};

    // Nametable mapping: derived from CHR D5 bits
    uint8_t nt_map_[4] = {0, 0, 0, 0};

    void update_nametables() {
        // R0 2KB bank covers CHR $0000-$07FF → nametable 0 and 1
        nt_map_[0] = (registers_[0] >> 5) & 0x01;
        nt_map_[1] = (registers_[0] >> 5) & 0x01;
        // R1 2KB bank covers CHR $0800-$0FFF → nametable 2 and 3
        nt_map_[2] = (registers_[1] >> 5) & 0x01;
        nt_map_[3] = (registers_[1] >> 5) & 0x01;
    }

public:
    Mapper095(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        target_register_ = 0;
        std::memset(registers_, 0, sizeof(registers_));
        update_nametables();
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (num_8k == 0) num_8k = 1;
        uint32_t last = num_8k - 1;
        uint32_t second_last = (num_8k >= 2) ? num_8k - 2 : 0;

        uint32_t banks[4] = {
            (registers_[6] & 0x3F) % num_8k,
            (registers_[7] & 0x3F) % num_8k,
            second_last,
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
        uint32_t chr_1k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;

        // R0: 2KB at $0000 (D5 stripped for CHR address)
        uint32_t b = ((uint32_t)(registers_[0] & 0x1F) & 0xFE) % chr_1k;
        config.chr_pages[0] = chr_mem_ + (b * 0x0400);
        config.chr_pages[1] = chr_mem_ + (((b + 1) % chr_1k) * 0x0400);

        // R1: 2KB at $0800
        b = ((uint32_t)(registers_[1] & 0x1F) & 0xFE) % chr_1k;
        config.chr_pages[2] = chr_mem_ + (b * 0x0400);
        config.chr_pages[3] = chr_mem_ + (((b + 1) % chr_1k) * 0x0400);

        // R2-R5: 1KB at $1000-$1C00
        for (int i = 0; i < 4; i++) {
            uint32_t bank = (registers_[2 + i] & 0x1F) % chr_1k;
            config.chr_pages[4 + i] = chr_mem_ + (bank * 0x0400);
        }

        for (int i = 0; i < 8; i++)
            config.chr_writable[i] = chr_is_ram_;

        // Nametable mirroring from CHR D5 bits
        for (int i = 0; i < 4; i++)
            config.nt_page[i] = nt_map_[i];
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        if (!(addr & 0x0001)) {
            target_register_ = data & 0x07;
        } else {
            registers_[target_register_] = data;
            if (target_register_ <= 1) {
                update_nametables();
            }
        }
        return true;
    }
};

} // namespace nes_system
