#pragma once
/*
 * mapper_096_oeka_kids.hpp — iNES Mapper 096 (Oeka Kids tablet)
 *
 * UNROM variant with special 32KB CHR-RAM banking:
 *   $8000-$FFFF writes: D0-D1 = 32KB PRG bank, D2-D3 = 256KB CHR-RAM outer bank
 *   PPU A8 latch (from reads in $2xxx range) selects which 4KB CHR-RAM inner bank
 *   within each 32KB outer bank.
 *
 * CHR-RAM layout: 4 outer banks × 8 inner 4KB pages = 256KB CHR-RAM total.
 * Inner page selected by PPU A8 rising edge on BG fetches (nametable reads).
 *
 * PRG: switchable 32KB + fixed last 32KB.
 *
 * Games: Oeka Kids: Anpanman no Hiragana Daisuki, Oeka Kids: Anpanman to Oekaki Shiyou!!
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper096 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t prg_bank_ = 0;
    uint8_t chr_outer_ = 0;      // D2-D3 of write: selects 32KB CHR-RAM block
    uint8_t chr_latch_ = 0;      // PPU A8 latch: selects 4KB inner page

public:
    Mapper096(uint8_t prgBanks, uint8_t /*chrBanks*/)
        : prg_banks_(prgBanks) {}

    void reset() override {
        prg_bank_ = 0;
        chr_outer_ = 0;
        chr_latch_ = 0;
    }

    Mirror mirror() override { return Mirror::FOUR_SCREEN; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        // 32KB switchable at $8000 + 32KB fixed at last
        uint32_t base = (static_cast<uint32_t>(prg_bank_ & 0x03) * 4) % n;
        uint32_t fixed = (n >= 4) ? n - 4 : 0u;
        uint32_t banks[4] = { base, base + 1, base + 2, base + 3 };
        // Clamp
        for (auto& b : banks) if (b >= n) b = n - 1;
        // Actually this is 32KB switchable at $8000-$FFFF (no fixed bank)
        // But some games have PRG > 32KB, so:
        // Original hardware: 128KB PRG, D0-D1 select 32KB page
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // 256KB CHR-RAM: outer bank (32KB) + inner latch (4KB page within)
        // The hardware has 8 4KB pages per outer bank
        // Page layout: outer_bank * 32KB + latch * 4KB for the NT-read-selected page,
        // and the remaining 4KB pages come from a fixed arrangement.
        //
        // Actually: $0000-$0FFF and $1000-$1FFF each 4KB.
        // Pages 0-3 of each 32KB block are the 4 possible "latched" pages.
        // The latch selects which 4KB page appears at a specific slot.
        //
        // Simplified model: entire 8KB CHR comes from outer*32KB + latch*4KB offsets.
        uint32_t outer_offset = static_cast<uint32_t>(chr_outer_) * 0x8000; // 32KB
        uint32_t latch_offset = static_cast<uint32_t>(chr_latch_) * 0x1000; // 4KB

        // $0000-$0FFF: latched 4KB page
        config.chr_pages[0] = chr_mem_ + outer_offset + latch_offset;
        config.chr_pages[1] = chr_mem_ + outer_offset + latch_offset + 0x0400;
        config.chr_pages[2] = chr_mem_ + outer_offset + latch_offset + 0x0800;
        config.chr_pages[3] = chr_mem_ + outer_offset + latch_offset + 0x0C00;

        // $1000-$1FFF: fixed at page 0 of outer bank (or same latched page)
        // Actually the Oeka Kids hardware maps both halves from the same latch.
        config.chr_pages[4] = chr_mem_ + outer_offset + latch_offset;
        config.chr_pages[5] = chr_mem_ + outer_offset + latch_offset + 0x0400;
        config.chr_pages[6] = chr_mem_ + outer_offset + latch_offset + 0x0800;
        config.chr_pages[7] = chr_mem_ + outer_offset + latch_offset + 0x0C00;

        for (int i = 0; i < 8; i++)
            config.chr_writable[i] = true; // CHR-RAM
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;
        prg_bank_ = data & 0x03;
        chr_outer_ = (data >> 2) & 0x03;
        return true;
    }

    // PPU bus intercept: latch PPU A8 on nametable reads ($2000-$2FFF)
    bool ppu_bus_intercept(uint16_t ppu_addr, uint8_t& /*data*/) override {
        if ((ppu_addr & 0x3000) == 0x2000) {
            chr_latch_ = (ppu_addr >> 8) & 0x03;
        }
        return false; // don't override data
    }
};

} // namespace nes_system
