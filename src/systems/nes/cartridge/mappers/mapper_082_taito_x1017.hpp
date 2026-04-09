#pragma once
/*
 * mapper_082_taito_x1017.h — iNES Mapper 082 (Taito X1-017)
 *
 * Similar to Taito X1-005 but with CHR inversion and PRG-RAM control.
 * 3×8KB switchable PRG + 8KB fixed + 6×CHR banks (2×2KB + 4×1KB).
 * All registers at $7EF0-$7EFF.
 *
 * Register map:
 *   $7EF0: CHR bank 0 (2KB)
 *   $7EF1: CHR bank 1 (2KB)
 *   $7EF2: CHR bank 2 (1KB)
 *   $7EF3: CHR bank 3 (1KB)
 *   $7EF4: CHR bank 4 (1KB)
 *   $7EF5: CHR bank 5 (1KB)
 *   $7EF6: mirroring (D0) + CHR inversion (D1)
 *   $7EF7-$7EF9: PRG-RAM enable (write $CA to enable)
 *   $7EFA: PRG bank 0 (8KB, right-shifted by 2)
 *   $7EFB: PRG bank 1 (8KB, right-shifted by 2)
 *   $7EFC: PRG bank 2 (8KB, right-shifted by 2)
 *
 * Games: Kyuukyoku Harikiri Koushien, Harikiri Stadium 3, SD Detective Blader.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper082 : public Mapper {
private:

    uint8_t prg_bank_[3] = {};
    uint8_t chr_bank_[6] = {};
    Mirror mirror_mode_ = Mirror::VERTICAL;
    bool chr_inversion_ = false;

public:
    Mapper082(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        for (int i = 0; i < 3; i++) prg_bank_[i] = 0;
        for (int i = 0; i < 6; i++) chr_bank_[i] = 0;
        mirror_mode_ = header_mirror_;
        chr_inversion_ = false;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(prg_bank_[0] >> 2) % n,
            static_cast<uint32_t>(prg_bank_[1] >> 2) % n,
            static_cast<uint32_t>(prg_bank_[2] >> 2) % n,
            n - 1
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);

        // PRG-RAM at $6000-$7FFF — reads work normally but writes must
        // go through register_write() so register writes at $7EF0-$7EFF
        // are intercepted instead of silently absorbed by RAM.
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
        config.prg_ram_write_protected = true;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t num_1k = mapper_helpers::chr_1k_count(chr_mem_size_);

        // With inversion: 1KB at $0000, 2KB at $1000; without: 2KB at $0000, 1KB at $1000
        if (!chr_inversion_) {
            // 2×2KB at $0000-$0FFF
            for (int i = 0; i < 2; i++) {
                uint32_t b = (static_cast<uint32_t>(chr_bank_[i]) & 0xFE) % num_1k;
                config.chr_pages[i * 2]     = chr_mem_ + b * 0x0400;
                config.chr_pages[i * 2 + 1] = chr_mem_ + ((b + 1) % num_1k) * 0x0400;
            }
            // 4×1KB at $1000-$1FFF
            for (int i = 0; i < 4; i++) {
                uint32_t b = static_cast<uint32_t>(chr_bank_[2 + i]) % num_1k;
                config.chr_pages[4 + i] = chr_mem_ + b * 0x0400;
            }
        } else {
            // 4×1KB at $0000-$0FFF
            for (int i = 0; i < 4; i++) {
                uint32_t b = static_cast<uint32_t>(chr_bank_[2 + i]) % num_1k;
                config.chr_pages[i] = chr_mem_ + b * 0x0400;
            }
            // 2×2KB at $1000-$1FFF
            for (int i = 0; i < 2; i++) {
                uint32_t b = (static_cast<uint32_t>(chr_bank_[i]) & 0xFE) % num_1k;
                config.chr_pages[4 + i * 2]     = chr_mem_ + b * 0x0400;
                config.chr_pages[4 + i * 2 + 1] = chr_mem_ + ((b + 1) % num_1k) * 0x0400;
            }
        }
        for (int i = 0; i < 8; i++)
            config.chr_writable[i] = chr_is_ram_;
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x7EF0 && addr <= 0x7EFF) {
            uint8_t reg = addr & 0x0F;
            switch (reg) {
                case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
                    chr_bank_[reg] = data;
                    return true;

                case 0x06:
                    mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                    chr_inversion_ = (data & 0x02) != 0;
                    return true;

                case 0x07: case 0x08: case 0x09:
                    // PRG-RAM enable — writing $CA enables
                    return false;

                case 0x0A: prg_bank_[0] = data; return true;
                case 0x0B: prg_bank_[1] = data; return true;
                case 0x0C: prg_bank_[2] = data; return true;
            }
            return false;
        }

        // Write-through to PRG-RAM for non-register addresses ($6000-$7EEF, $7F00-$7FFF)
        if (addr >= 0x6000 && addr <= 0x7FFF && prg_ram_ != nullptr) {
            uint16_t offset = addr - 0x6000;
            if (offset < prg_ram_size_)
                prg_ram_[offset] = data;
            return false;
        }

        return false;
    }
};

} // namespace nes_system
