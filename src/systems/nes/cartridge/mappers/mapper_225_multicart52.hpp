#pragma once
/*
 * mapper_225_multicart52.h — iNES Mapper 225 (52-in-1 / 58-in-1 multicart)
 *
 * Address-based PRG/CHR selection — the written value is ignored.
 * All banking is derived from the address bits of the write.
 *
 * Games: Various multicart compilations
 *
 * Write to $8000-$FFFF (address decodes as):
 *   A14:      PRG ROM chip select (for 1MB+ carts)
 *   A13-A6:   PRG bank (combined with chip select)
 *   A12:      PRG mode (0 = 32KB, 1 = 16KB)
 *   A11:      Mirroring (0 = vertical, 1 = horizontal)
 *   A5-A0:    CHR 8KB bank
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper225 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint16_t latch_addr_ = 0;  // Latched address bits

public:
    Mapper225(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        latch_addr_ = 0;
    }

    Mirror mirror() override {
        return (latch_addr_ & 0x0800) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (num_16k == 0) num_16k = 1;

        // D14-D6 give the PRG bank
        uint32_t prg_full = (latch_addr_ >> 6) & 0x1FF;
        bool prg_mode_16k = (latch_addr_ & 0x1000) != 0;

        uint32_t lo_bank, hi_bank;
        if (prg_mode_16k) {
            // 16KB mode: same bank in both halves
            lo_bank = prg_full % num_16k;
            hi_bank = lo_bank;
        } else {
            // 32KB mode: consecutive pair
            lo_bank = (prg_full & 0xFE) % num_16k;
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
        uint32_t max_8k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x2000) : 1;
        uint32_t chr_bank = (latch_addr_ & 0x3F) % max_8k;
        uint32_t base = chr_bank * 0x2000;
        for (int i = 0; i < 8; i++) {
            uint32_t offset = base + i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        (void)data;
        if (addr >= 0x8000) {
            latch_addr_ = addr & 0x7FFF;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
