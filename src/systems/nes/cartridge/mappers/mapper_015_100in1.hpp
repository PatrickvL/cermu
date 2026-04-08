#pragma once
/*
 * mapper_015_100in1.h — iNES Mapper 015 (100-in-1 Contra Function 16)
 *
 * Multicart mapper with 4 PRG banking modes selected by address bits.
 * No CHR banking (uses 8KB CHR-RAM).
 *
 * Register ($8000-$FFFF):
 *   Address A1-A0 selects mode (0-3).
 *   D7:    Sub-bank select (used in mode 2 only)
 *   D6:    Mirroring (0 = vertical, 1 = horizontal)
 *   D5-D0: PRG bank base selection
 *
 * Banking (8KB granularity, per FCEUX reference):
 *   Mode 0: 32KB — four consecutive 8KB pages from (bank << 1)
 *   Mode 1: 16KB switchable at $8000 + 16KB at (bank|7)<<1 at $C000
 *   Mode 2: Single 8KB page repeated across all 32KB — page = (bank<<1)+(D7)
 *   Mode 3: 16KB switchable at $8000, repeated at $C000
 *
 * CHR writes protected in mode 3, writable otherwise.
 *
 * Reference: FCEUX src/boards/15.cpp (CaH4e3)
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper015 : public Mapper {
private:

    uint8_t mode_ = 0;
    uint8_t latched_ = 0;          // Full register data byte
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // Total 8KB PRG page count
    uint32_t num_8k() const {
        uint32_t n = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        return (n > 0) ? n : 1;
    }

public:
    Mapper015(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        mode_ = 0;
        latched_ = 0;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t n8k = num_8k();
        uint32_t bank = latched_ & 0x3F;

        switch (mode_) {
            case 0: {
                // 32KB — four consecutive 8KB pages from (bank << 1)
                for (int i = 0; i < 4; i++) {
                    uint32_t page = ((bank << 1) + i) % n8k;
                    uint32_t off = page * 0x2000;
                    config.prg_pages[i * 2]     = prg_rom_ + off;
                    config.prg_pages[i * 2 + 1] = prg_rom_ + off + 0x1000;
                }
                break;
            }
            case 1: {
                // 16KB switchable at $8000 + 16KB at (bank|7) at $C000
                uint32_t lo = bank;
                uint32_t hi = bank | 0x07;
                for (int i = 0; i < 2; i++) {
                    uint32_t lp = (((lo << 1) + i) % n8k) * 0x2000;
                    uint32_t hp = (((hi << 1) + i) % n8k) * 0x2000;
                    config.prg_pages[i * 2]     = prg_rom_ + lp;
                    config.prg_pages[i * 2 + 1] = prg_rom_ + lp + 0x1000;
                    config.prg_pages[4 + i * 2]     = prg_rom_ + hp;
                    config.prg_pages[4 + i * 2 + 1] = prg_rom_ + hp + 0x1000;
                }
                break;
            }
            case 2: {
                // Single 8KB page repeated — page = (bank << 1) + D7
                uint32_t page = ((bank << 1) + (latched_ >> 7)) % n8k;
                uint32_t off = page * 0x2000;
                for (int i = 0; i < 4; i++) {
                    config.prg_pages[i * 2]     = prg_rom_ + off;
                    config.prg_pages[i * 2 + 1] = prg_rom_ + off + 0x1000;
                }
                break;
            }
            case 3:
            default: {
                // 16KB repeated — same 16KB in both $8000 and $C000
                for (int i = 0; i < 2; i++) {
                    uint32_t page = ((bank << 1) + i) % n8k;
                    uint32_t off = page * 0x2000;
                    config.prg_pages[i * 2]         = prg_rom_ + off;
                    config.prg_pages[i * 2 + 1]     = prg_rom_ + off + 0x1000;
                    config.prg_pages[4 + i * 2]     = prg_rom_ + off;
                    config.prg_pages[4 + i * 2 + 1] = prg_rom_ + off + 0x1000;
                }
                break;
            }
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // 8KB CHR-RAM; write-protected in mode 3 only (per FCEUX)
        bool writable = (mode_ != 3);
        for (int i = 0; i < 8; i++) {
            uint32_t offset = i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = writable;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            mode_ = addr & 0x03;
            latched_ = data;
            mirror_mode_ = (data & 0x40) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
