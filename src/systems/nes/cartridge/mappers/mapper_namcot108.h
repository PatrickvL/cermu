#pragma once
/*
 * mapper_namcot108.h — Shared Namcot 108 / DxROM implementation
 *
 * Template-based mapper covering the Namcot-108 chip family:
 *   Mapper 088 (Namco 3433): CHR lower/upper half split (R0-R1 low, R2-R5 | $40)
 *   Mapper 095 (Namco 3425): CHR D5 controls nametable mirroring per slot
 *   Mapper 206 (DxROM):      Standard Namcot-108, no CHR tricks, no NT control
 *
 * All variants share the same register interface:
 *   $8000 (even): D2-D0 = register select (R0-R7)
 *   $8001 (odd):  D7-D0 = data for selected register
 *
 * R0-R1: 2KB CHR banks,  R2-R5: 1KB CHR banks,  R6-R7: 8KB PRG banks
 * Fixed last two 8KB PRG banks at $C000/$E000.
 * No IRQ, no mirroring control (except 095's D5 trick).
 */

#include "systems/nes/cartridge/nes_mapper.h"
#include <cstring>

namespace nes_system {

enum class Namcot108Variant : uint8_t {
    DxROM,       // 206: Namcot 108 / DxROM / MIMIC-1 — standard, full bank value
                 //      Games: Babel no Tou, Dragon Buster, Family Jockey, Mappy-Land.
    ChrSplit,    // 088: Namco 3433 — R0/R1 lower half, R2-R5 OR $40 upper half
                 //      Games: Dragon Spirit, Quinty.
    NtFromD5     // 095: Namco 3425 — CHR D5 → nametable, strip D5 from bank value
                 //      Games: Dragon Buster (alt board).
};

template<Namcot108Variant V>
class MapperNamcot108 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t target_register_ = 0;
    uint8_t registers_[8] = {};

    // Nametable map — only used by NtFromD5 variant
    uint8_t nt_map_[4] = {0, 0, 0, 0};

    void update_nametables() {
        if constexpr (V == Namcot108Variant::NtFromD5) {
            nt_map_[0] = (registers_[0] >> 5) & 0x01;
            nt_map_[1] = (registers_[0] >> 5) & 0x01;
            nt_map_[2] = (registers_[1] >> 5) & 0x01;
            nt_map_[3] = (registers_[1] >> 5) & 0x01;
        }
    }

public:
    MapperNamcot108(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        target_register_ = 0;
        std::memset(registers_, 0, sizeof(registers_));
        if constexpr (V == Namcot108Variant::NtFromD5) {
            update_nametables();
        }
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (num_8k == 0) num_8k = 1;
        uint32_t last = num_8k - 1;
        uint32_t second_last = (num_8k >= 2) ? num_8k - 2 : 0;

        // 088 uses 4-bit PRG mask; 095/206 use 6-bit
        constexpr uint8_t prg_mask = (V == Namcot108Variant::ChrSplit) ? 0x0F : 0x3F;

        uint32_t banks[4] = {
            static_cast<uint32_t>(registers_[6] & prg_mask) % num_8k,
            static_cast<uint32_t>(registers_[7] & prg_mask) % num_8k,
            second_last,
            last
        };

        for (int slot = 0; slot < 4; slot++) {
            uint32_t base = banks[slot] * 0x2000;
            for (int h = 0; h < 2; h++) {
                uint32_t off = base + h * 0x1000;
                config.prg_pages[slot * 2 + h] =
                    (off < prg_rom_size_) ? prg_rom_ + off : nullptr;
            }
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_1k = (chr_mem_size_ > 0)
                              ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;
        if (chr_1k == 0) chr_1k = 1;

        if constexpr (V == Namcot108Variant::ChrSplit) {
            // 088: R0/R1 stay in lower half ($00-$3F), R2-R5 OR $40 (upper half)
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

        } else if constexpr (V == Namcot108Variant::NtFromD5) {
            // 095: strip D5 from bank value, use D5 for nametable
            uint32_t b = (static_cast<uint32_t>(registers_[0] & 0x1F) & 0xFE) % chr_1k;
            config.chr_pages[0] = chr_mem_ + (b * 0x0400);
            config.chr_pages[1] = chr_mem_ + (((b + 1) % chr_1k) * 0x0400);

            b = (static_cast<uint32_t>(registers_[1] & 0x1F) & 0xFE) % chr_1k;
            config.chr_pages[2] = chr_mem_ + (b * 0x0400);
            config.chr_pages[3] = chr_mem_ + (((b + 1) % chr_1k) * 0x0400);

            for (int i = 0; i < 4; i++) {
                uint32_t bank = (registers_[2 + i] & 0x1F) % chr_1k;
                config.chr_pages[4 + i] = chr_mem_ + (bank * 0x0400);
            }
            for (int i = 0; i < 4; i++)
                config.nt_page[i] = nt_map_[i];

        } else {
            // 206: standard — full D0-D5 bank values
            uint32_t b = (static_cast<uint32_t>(registers_[0] & 0x3F) & 0xFE) % chr_1k;
            config.chr_pages[0] = chr_mem_ + (b * 0x0400);
            config.chr_pages[1] = chr_mem_ + (((b + 1) % chr_1k) * 0x0400);

            b = (static_cast<uint32_t>(registers_[1] & 0x3F) & 0xFE) % chr_1k;
            config.chr_pages[2] = chr_mem_ + (b * 0x0400);
            config.chr_pages[3] = chr_mem_ + (((b + 1) % chr_1k) * 0x0400);

            for (int i = 0; i < 4; i++) {
                uint32_t bank = (registers_[2 + i] & 0x3F) % chr_1k;
                config.chr_pages[4 + i] = chr_mem_ + (bank * 0x0400);
            }
        }

        for (int i = 0; i < 8; i++)
            config.chr_writable[i] = chr_is_ram_;
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        if (!(addr & 0x0001)) {
            // Even: bank select ($8000)
            target_register_ = data & 0x07;
        } else {
            // Odd: bank data ($8001)
            registers_[target_register_] = data;
            if constexpr (V == Namcot108Variant::NtFromD5) {
                if (target_register_ <= 1) update_nametables();
            }
        }
        return true;
    }
};

} // namespace nes_system
