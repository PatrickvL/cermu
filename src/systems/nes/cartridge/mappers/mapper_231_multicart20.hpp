#pragma once
/*
 * mapper_231_multicart20.h — iNES Mapper 231 (20-in-1)
 *
 * Simple address-based multicart. Write value is ignored.
 * PRG bank from address bits, mirroring control.
 *
 * Games: 20-in-1 multicart
 *
 * Write to $8000-$FFFF:
 *   A7:    Mirroring (0 = vertical, 1 = horizontal)
 *   A6-A5: Outer PRG bank (64KB block select)
 *   A4-A0: Inner PRG bank (16KB within block)
 *          Mode: 32KB when inner bank is even, 16KB when odd
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper231 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint16_t latch_addr_ = 0;

public:
    Mapper231(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        latch_addr_ = 0;
    }

    Mirror mirror() override {
        return (latch_addr_ & 0x80) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (num_16k == 0) num_16k = 1;

        // Outer bank (64KB block) + inner bank
        uint32_t outer = ((latch_addr_ >> 5) & 0x03) * 4;  // 4 × 16KB per outer block
        uint32_t inner = latch_addr_ & 0x1F;

        uint32_t lo_bank = (outer | (inner & 0x1E)) % num_16k;  // Even inner → 32KB
        uint32_t hi_bank;

        if (inner & 0x01) {
            // Odd: 16KB repeated
            lo_bank = (outer | inner) % num_16k;
            hi_bank = lo_bank;
        } else {
            // Even: 32KB pair
            hi_bank = (lo_bank | 1) % num_16k;
        }

        uint32_t lo_base = lo_bank * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t off = lo_base + i * 0x1000;
            config.prg_pages[i] = (off < prg_rom_size_) ? prg_rom_ + off : nullptr;
        }

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
        (void)data;
        if (addr >= 0x8000) {
            latch_addr_ = addr;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
