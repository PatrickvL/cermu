#pragma once
/*
 * mapper_232_camerica_bf9096.h — iNES Mapper 232 (Camerica BF9096)
 *
 * Camerica Quattro series mapper. Two register banks control a
 * combined 32KB PRG window: an outer 256KB block select and an
 * inner 32KB bank select within that block.
 * No CHR banking (8KB CHR-RAM). No PRG-RAM.
 * Games: Quattro Adventure, Quattro Sports, Quattro Arcade.
 *
 * $8000-$BFFF: Block select (D4-D3 → outer block)
 * $C000-$FFFF: Page select (D1-D0 → inner bank within block)
 */

#include "systems/nes/cartridge/nes_mapper.h"

namespace nes_system {

class Mapper232 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t outer_block_ = 0;    // 2-bit block select (0-3)
    uint8_t inner_bank_ = 3;     // 2-bit bank within block (0-3)

public:
    Mapper232(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        outer_block_ = 0;
        inner_bank_ = 3;  // Default to last page of first block
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t total_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (total_16k == 0) total_16k = 1;

        // Each block has 4 × 16KB banks. Outer selects block, inner selects bank.
        uint32_t block_base = outer_block_ * 4;

        // $8000-$BFFF: selected inner bank within block
        uint32_t bank_lo = (block_base + inner_bank_) % total_16k;
        uint32_t base_lo = bank_lo * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = base_lo + i * 0x1000;
            config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }

        // $C000-$FFFF: fixed to last bank of the same block
        uint32_t bank_hi = (block_base + 3) % total_16k;
        uint32_t base_hi = bank_hi * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = base_hi + i * 0x1000;
            config.prg_pages[4 + i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        for (int i = 0; i < 8; i++) {
            uint32_t offset = i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = true;  // CHR-RAM
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000 && addr <= 0xBFFF) {
            outer_block_ = (data >> 3) & 0x03;
            return true;
        }
        if (addr >= 0xC000) {
            inner_bank_ = data & 0x03;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
