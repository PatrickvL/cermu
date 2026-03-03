#pragma once
/*
 * mapper_133_sachen.h — iNES Mapper 133 (Sachen SA-72008)
 *
 * Very simple mapper: 32KB PRG + 8KB CHR bank switching via
 * a single register at $4100 (active when A8=0, A6=1, A5=0, A0=0).
 *
 * Games: Hummer Team Sachen unlicensed titles
 *
 * Register ($4100):
 *   D2:    32KB PRG bank select
 *   D1-D0: 8KB CHR bank select
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper133 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper133(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_select_ = 0;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t max_32k = prg_rom_size_ / 0x8000;
        if (max_32k == 0) max_32k = 1;
        uint32_t bank = prg_bank_select_ % max_32k;
        uint32_t base = bank * 0x8000;
        for (int i = 0; i < 8; i++) {
            uint32_t offset = base + i * 0x1000;
            config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t max_8k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x2000) : 1;
        uint32_t bank = (max_8k > 0) ? (chr_bank_select_ % max_8k) : 0;
        uint32_t base = bank * 0x2000;
        for (int i = 0; i < 8; i++) {
            uint32_t offset = base + i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        // Active at $4100 (addr & $E100 == $4100)
        if ((addr & 0xE100) == 0x4100) {
            prg_bank_select_ = (data >> 2) & 0x01;
            chr_bank_select_ = data & 0x03;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
