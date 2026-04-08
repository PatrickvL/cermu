#pragma once
/*
 * mapper_075_vrc1.h — iNES Mapper 075 (Konami VRC1)
 *
 * 3 × 8KB switchable PRG (+ 8KB fixed last bank).
 * 2 × 4KB CHR banks.  CHR high bits come from a mirroring/CHR register.
 *
 * Games: King Kong 2, TwinBee, Tetsuwan Atom
 *
 * Registers:
 *   $8000: D3-D0 = 8KB PRG bank at $8000
 *   $9000: D0 = mirroring (0=vert, 1=horiz), D1 = CHR0 high bit, D2 = CHR1 high bit
 *   $A000: D3-D0 = 8KB PRG bank at $A000
 *   $C000: D3-D0 = 8KB PRG bank at $C000
 *   $E000: D3-D0 = 4KB CHR bank 0 (low nibble)
 *   $F000: D3-D0 = 4KB CHR bank 1 (low nibble)
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper075 : public Mapper {
private:

    uint8_t prg_bank_[3] = {};       // Three 8KB PRG banks
    uint8_t chr_lo_[2] = {};         // CHR bank low nibbles (4 bits each)
    uint8_t chr_hi_[2] = {};         // CHR bank high bits (1 bit each, from $9000)
    Mirror mirror_mode_ = Mirror::VERTICAL;

public:
    Mapper075(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        prg_bank_[0] = 0;
        prg_bank_[1] = 0;
        prg_bank_[2] = 0;
        chr_lo_[0] = chr_lo_[1] = 0;
        chr_hi_[0] = chr_hi_[1] = 0;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (num_8k == 0) num_8k = 1;
        uint32_t last = num_8k - 1;

        uint32_t banks[4] = {
            prg_bank_[0] % num_8k,
            prg_bank_[1] % num_8k,
            prg_bank_[2] % num_8k,
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
        uint32_t max_4k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x1000) : 1;

        for (int half = 0; half < 2; half++) {
            // Combine high bit from $9000 with low nibble from $E000/$F000
            uint32_t bank = ((chr_hi_[half] << 4) | chr_lo_[half]) % max_4k;
            uint32_t base = bank * 0x1000;
            for (int i = 0; i < 4; i++) {
                uint32_t off = base + i * 0x0400;
                config.chr_pages[half * 4 + i] = (off < chr_mem_size_) ? chr_mem_ + off : chr_mem_;
                config.chr_writable[half * 4 + i] = chr_is_ram_;
            }
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xF000) {
            case 0x8000:
                prg_bank_[0] = data & 0x0F;
                return true;
            case 0x9000:
                mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                chr_hi_[0] = (data >> 1) & 0x01;
                chr_hi_[1] = (data >> 2) & 0x01;
                return true;
            case 0xA000:
                prg_bank_[1] = data & 0x0F;
                return true;
            case 0xC000:
                prg_bank_[2] = data & 0x0F;
                return true;
            case 0xE000:
                chr_lo_[0] = data & 0x0F;
                return true;
            case 0xF000:
                chr_lo_[1] = data & 0x0F;
                return true;
            default:
                return false;
        }
    }
};

} // namespace nes_system
