#pragma once
/*
 * mapper_229_multicart31.h — iNES Mapper 229 (31-in-1)
 *
 * Simple address-based multicart. Write value is ignored.
 * When address low 5 bits are 0, select 32KB PRG mode;
 * otherwise 16KB mode with CHR banking.
 *
 * Games: 31-in-1 multicart
 *
 * Write to $8000-$FFFF:
 *   A5:    Mirroring (0 = vertical, 1 = horizontal)
 *   A4-A0: PRG/CHR bank (when != 0 → 16KB PRG + 8KB CHR)
 *          When 0 → first 32KB of PRG, first 8KB of CHR
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper229 : public Mapper {
private:

    uint16_t latch_addr_ = 0;

public:
    Mapper229(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        latch_addr_ = 0;
    }

    Mirror mirror() override {
        return (latch_addr_ & 0x0020) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (num_16k == 0) num_16k = 1;

        uint32_t bank = latch_addr_ & 0x1F;

        uint32_t lo_bank, hi_bank;
        if (bank == 0) {
            // 32KB mode: first 32KB
            lo_bank = 0;
            hi_bank = 1 % num_16k;
        } else {
            // 16KB repeated
            lo_bank = bank % num_16k;
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
        uint32_t max_8k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x2000) : 1;
        uint32_t bank_select = latch_addr_ & 0x1F;
        uint32_t chr_bank = (bank_select == 0) ? 0 : (bank_select % max_8k);
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
            latch_addr_ = addr;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
