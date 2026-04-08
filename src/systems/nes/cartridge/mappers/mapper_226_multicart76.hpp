#pragma once
/*
 * mapper_226_multicart76.h — iNES Mapper 226 (76-in-1 / Super 42-in-1)
 *
 * Two-register multicart: $8000 and $8001 combine to select PRG bank.
 *
 * Games: 76-in-1, Super 42-in-1 multicart
 *
 * Register $8000:
 *   D7:    Reset bit (unused in emulation)
 *   D6:    Mirroring (0 = vertical, 1 = horizontal)
 *   D5:    PRG mode (0 = 32KB, 1 = 16KB)
 *   D4-D1: PRG bank low bits
 *   D0:    PRG bank high bit from chip select
 *
 * Register $8001:
 *   D0:    Upper PRG chip select bit
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper226 : public Mapper {
private:

    uint8_t reg0_ = 0;
    uint8_t reg1_ = 0;

public:
    Mapper226(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        reg0_ = 0;
        reg1_ = 0;
    }

    Mirror mirror() override {
        return (reg0_ & 0x40) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (num_16k == 0) num_16k = 1;

        // PRG bank: combine chip select bits with bank bits
        uint32_t prg_bank = ((reg0_ >> 1) & 0x0F) | ((reg0_ & 0x01) << 4) | ((reg1_ & 0x01) << 5);
        bool mode_16k = (reg0_ & 0x20) != 0;

        uint32_t lo_bank, hi_bank;
        if (mode_16k) {
            lo_bank = prg_bank % num_16k;
            hi_bank = lo_bank;
        } else {
            lo_bank = (prg_bank & 0xFE) % num_16k;
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
        if (addr >= 0x8000) {
            if (!(addr & 0x0001)) {
                reg0_ = data;
            } else {
                reg1_ = data;
            }
            return true;
        }
        return false;
    }
};

} // namespace nes_system
