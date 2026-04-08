#pragma once
/*
 * mapper_033_taito_tc0190.h — iNES Mapper 033 (Taito TC0190/TC0350)
 *
 * 2 × 8KB switchable PRG + fixed last bank.
 * 2 × 2KB + 2 × 1KB CHR banking.
 * Mirroring control via D6 of first PRG register.
 * No IRQ. (Mapper 048 is the variant with IRQ.)
 *
 * Games: Flintstones (J), Power Blazer, Don Doko Don
 *
 * Registers:
 *   $8000: D5-D0 = 8KB PRG at $8000, D6 = mirroring
 *   $8001: D5-D0 = 8KB PRG at $A000
 *   $8002: D7-D0 = 2KB CHR at $0000
 *   $8003: D7-D0 = 2KB CHR at $0800
 *   $A000: D7-D0 = 1KB CHR at $1000
 *   $A001: D7-D0 = 1KB CHR at $1400
 *   $A002: D7-D0 = 1KB CHR at $1800
 *   $A003: D7-D0 = 1KB CHR at $1C00
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper033 : public Mapper {
private:

    uint8_t prg_bank_[2] = {};
    uint8_t chr_2k_[2] = {};         // 2KB banks for $0000 and $0800
    uint8_t chr_1k_[4] = {};         // 1KB banks for $1000-$1C00
    Mirror mirror_mode_ = Mirror::VERTICAL;

public:
    Mapper033(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        prg_bank_[0] = 0;
        prg_bank_[1] = 0;
        chr_2k_[0] = 0;
        chr_2k_[1] = 1;
        for (int i = 0; i < 4; i++) chr_1k_[i] = i;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (num_8k == 0) num_8k = 1;
        uint32_t last = num_8k - 1;
        uint32_t second_last = (num_8k >= 2) ? num_8k - 2 : 0;

        uint32_t banks[4] = {
            prg_bank_[0] % num_8k,    // $8000
            prg_bank_[1] % num_8k,    // $A000
            second_last,               // $C000
            last                       // $E000
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
        uint32_t chr_1k_count = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;

        // $0000-$07FF: 2KB bank 0
        uint32_t b = ((uint32_t)chr_2k_[0] * 2) % chr_1k_count;
        config.chr_pages[0] = chr_mem_ + (b * 0x0400);
        config.chr_pages[1] = chr_mem_ + (((b + 1) % chr_1k_count) * 0x0400);

        // $0800-$0FFF: 2KB bank 1
        b = ((uint32_t)chr_2k_[1] * 2) % chr_1k_count;
        config.chr_pages[2] = chr_mem_ + (b * 0x0400);
        config.chr_pages[3] = chr_mem_ + (((b + 1) % chr_1k_count) * 0x0400);

        // $1000-$1FFF: four 1KB banks
        for (int i = 0; i < 4; i++) {
            uint32_t bank = chr_1k_[i] % chr_1k_count;
            config.chr_pages[4 + i] = chr_mem_ + (bank * 0x0400);
        }

        for (int i = 0; i < 8; i++)
            config.chr_writable[i] = chr_is_ram_;
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xE003) {
            case 0x8000:
                prg_bank_[0] = data & 0x3F;
                mirror_mode_ = (data & 0x40) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                return true;
            case 0x8001:
                prg_bank_[1] = data & 0x3F;
                return true;
            case 0x8002:
                chr_2k_[0] = data;
                return true;
            case 0x8003:
                chr_2k_[1] = data;
                return true;
            case 0xA000:
                chr_1k_[0] = data;
                return true;
            case 0xA001:
                chr_1k_[1] = data;
                return true;
            case 0xA002:
                chr_1k_[2] = data;
                return true;
            case 0xA003:
                chr_1k_[3] = data;
                return true;
            default:
                return false;
        }
    }
};

} // namespace nes_system
