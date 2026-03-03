#pragma once
/*
 * mapper_227_multicart1200.h — iNES Mapper 227 (1200-in-1)
 *
 * Address-based multicart. Write value is ignored;
 * all banking derived from address bits.
 *
 * Games: 1200-in-1 multicart compilations
 *
 * Write to $8000-$FFFF (address decodes as):
 *   A8:    Mirroring (0 = vertical, 1 = horizontal)
 *   A7:    PRG chip / outer bank high bit
 *   A6-A2: PRG bank (5 bits)
 *   A1:    PRG size (0 = 32KB, 1 = 16KB)
 *   A0:    PRG bank low bit (for 32KB), or upper/lower select
 *   A9:    NROM-256 mode (0 = normal, 1 = NROM-256 / 32KB)
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper227 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint16_t latch_addr_ = 0;

public:
    Mapper227(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        latch_addr_ = 0;
    }

    Mirror mirror() override {
        return (latch_addr_ & 0x0100) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (num_16k == 0) num_16k = 1;

        uint32_t outer = (latch_addr_ >> 7) & 0x01;
        uint32_t bank = ((latch_addr_ >> 2) & 0x1F) | (outer << 5);
        bool mode_16k = (latch_addr_ & 0x02) != 0;
        bool nrom256 = (latch_addr_ & 0x0200) != 0;

        uint32_t lo_bank, hi_bank;
        if (mode_16k) {
            // 16KB mode: same bank repeated
            lo_bank = bank % num_16k;
            hi_bank = lo_bank;
        } else if (nrom256) {
            // NROM-256: 32KB consecutive
            lo_bank = (bank & 0xFE) % num_16k;
            hi_bank = (lo_bank | 1) % num_16k;
        } else {
            // NROM-128 selecting inner bank via A0
            uint32_t b = ((bank & 0xFE) | (latch_addr_ & 0x01));
            lo_bank = b % num_16k;
            hi_bank = lo_bank;
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
            latch_addr_ = addr & 0x03FF;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
