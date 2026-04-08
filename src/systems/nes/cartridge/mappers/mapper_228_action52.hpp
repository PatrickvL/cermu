#pragma once
/*
 * mapper_228_action52.h — iNES Mapper 228 (Action 52 / Cheetahmen II)
 *
 * Address+data based multicart mapper.
 * PRG banks from address, CHR banks from address + data.
 *
 * Games: Action 52, Cheetahmen II
 *
 * Write to $8000-$FFFF:
 *   Address:
 *     A12-A11: chip select (PRG ROM bank group)
 *     A10-A6:  PRG bank within chip
 *     A5:      PRG mode (0 = 32KB, 1 = 16KB)
 *     A0:      Mirroring (0 = vertical, 1 = horizontal)
 *   Data:
 *     D3-D2:  CHR bank high 2 bits
 *     (D1-D0 are unused)
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper228 : public Mapper {
private:

    uint16_t latch_addr_ = 0;
    uint8_t latch_data_ = 0;

public:
    Mapper228(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        latch_addr_ = 0;
        latch_data_ = 0;
    }

    Mirror mirror() override {
        return (latch_addr_ & 0x0001) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t num_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (num_16k == 0) num_16k = 1;

        uint32_t chip = (latch_addr_ >> 11) & 0x03;
        uint32_t prg_inner = (latch_addr_ >> 6) & 0x1F;
        bool mode_16k = (latch_addr_ & 0x20) != 0;

        // Chip select provides an offset into the full PRG ROM
        uint32_t chip_offset;
        switch (chip) {
            case 0: chip_offset = 0;  break;
            case 1: chip_offset = 16; break;  // 256KB
            case 2: chip_offset = 32; break;  // 512KB
            case 3:
            default: chip_offset = 32; break; // Chip 3 mirrors chip 2
        }

        uint32_t base_bank = chip_offset + prg_inner;

        uint32_t lo_bank, hi_bank;
        if (mode_16k) {
            lo_bank = base_bank % num_16k;
            hi_bank = lo_bank;
        } else {
            lo_bank = (base_bank & 0xFE) % num_16k;
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

        // CHR bank: D3-D2 as high bits, A3-A1 as low bits → 5-bit bank select
        uint32_t chr_bank = ((latch_data_ & 0x0C) << 1) | ((latch_addr_ >> 1) & 0x07);
        if (max_8k > 0) chr_bank %= max_8k;

        uint32_t base = chr_bank * 0x2000;
        for (int i = 0; i < 8; i++) {
            uint32_t offset = base + i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            latch_addr_ = addr;
            latch_data_ = data;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
