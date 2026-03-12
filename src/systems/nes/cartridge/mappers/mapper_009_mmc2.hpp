#pragma once
/*
 * mapper_009_mmc2.h — iNES Mapper 009 (MMC2 / PxROM)
 *
 * Nintendo MMC2 — famous for Punch-Out!!
 * 8KB switchable PRG at $8000, three fixed 8KB banks at $A000-$FFFF.
 * Two pairs of 4KB CHR banks with automatic latch-triggered switching
 * based on PPU tile fetches ($FD/$FE tiles).
 *
 * The CHR auto-switching mechanism makes this mapper unique:
 * reading certain tile indices from pattern tables triggers a latch
 * that swaps the active CHR bank.
 *
 * Games: Mike Tyson's Punch-Out!!, Punch-Out!! (all versions)
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper009 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;

    // Two CHR bank registers per 4KB page, switched by tile latch
    uint8_t chr_bank_0_fd_ = 0;   // $0000-$0FFF when latch0 = $FD
    uint8_t chr_bank_0_fe_ = 0;   // $0000-$0FFF when latch0 = $FE
    uint8_t chr_bank_1_fd_ = 0;   // $1000-$1FFF when latch1 = $FD
    uint8_t chr_bank_1_fe_ = 0;   // $1000-$1FFF when latch1 = $FE

    bool latch_0_ = true;   // false = $FD selected, true = $FE selected
    bool latch_1_ = true;   // false = $FD selected, true = $FE selected

    Mirror mirror_mode_ = Mirror::VERTICAL;

public:
    Mapper009(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_0_fd_ = 0;
        chr_bank_0_fe_ = 0;
        chr_bank_1_fd_ = 0;
        chr_bank_1_fe_ = 0;
        latch_0_ = true;
        latch_1_ = true;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t total_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (total_8k == 0) total_8k = 1;

        // $8000-$9FFF: switchable 8KB
        uint32_t b0 = prg_bank_select_ % total_8k;
        config.prg_pages[0] = prg_rom_ + b0 * 0x2000;
        config.prg_pages[1] = prg_rom_ + b0 * 0x2000 + 0x1000;

        // $A000-$FFFF: fixed to last three 8KB banks
        for (int slot = 1; slot < 4; slot++) {
            uint32_t b = (total_8k >= (uint32_t)(4 - slot)) ? (total_8k - (4 - slot)) : 0;
            config.prg_pages[slot * 2]     = prg_rom_ + b * 0x2000;
            config.prg_pages[slot * 2 + 1] = prg_rom_ + b * 0x2000 + 0x1000;
        }
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_4k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x1000) : 1;
        if (chr_4k == 0) chr_4k = 1;

        // $0000-$0FFF: selected by latch_0_
        uint32_t bank_lo = latch_0_ ? chr_bank_0_fe_ : chr_bank_0_fd_;
        bank_lo %= chr_4k;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = bank_lo * 0x1000 + i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }

        // $1000-$1FFF: selected by latch_1_
        uint32_t bank_hi = latch_1_ ? chr_bank_1_fe_ : chr_bank_1_fd_;
        bank_hi %= chr_4k;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = bank_hi * 0x1000 + i * 0x0400;
            config.chr_pages[4 + i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[4 + i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xF000) {
            case 0xA000:
                prg_bank_select_ = data & 0x0F;
                return true;
            case 0xB000:
                chr_bank_0_fd_ = data & 0x1F;
                return true;
            case 0xC000:
                chr_bank_0_fe_ = data & 0x1F;
                return true;
            case 0xD000:
                chr_bank_1_fd_ = data & 0x1F;
                return true;
            case 0xE000:
                chr_bank_1_fe_ = data & 0x1F;
                return true;
            case 0xF000:
                mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                return true;
        }
        return false;
    }

    // ----------------------------------------------------------------
    // PPU bus read hook — called by ppu_memory_tick on every
    // rendering read.  Detects pattern-table address ranges that
    // trigger the MMC2 CHR latch mechanism ($FD/$FE tile indices).
    //
    // Returns true if a latch switched, signalling the caller to
    // rebuild the bank map so subsequent reads use the new CHR page.
    // ----------------------------------------------------------------
    bool ppu_bus_read(uint16_t addr) override {
        const bool old_l0 = latch_0_;
        const bool old_l1 = latch_1_;
        chr_read_hook(addr);
        return (latch_0_ != old_l0) || (latch_1_ != old_l1);
    }

private:
    /// Internal latch update — shared between ppu_bus_read and any
    /// future direct callers.
    void chr_read_hook(uint16_t addr) {
        // Latches trigger on specific tile pattern addresses
        if (addr >= 0x0FD8 && addr <= 0x0FDF) {
            latch_0_ = false;  // $FD
        } else if (addr >= 0x0FE8 && addr <= 0x0FEF) {
            latch_0_ = true;   // $FE
        } else if (addr >= 0x1FD8 && addr <= 0x1FDF) {
            latch_1_ = false;  // $FD
        } else if (addr >= 0x1FE8 && addr <= 0x1FEF) {
            latch_1_ = true;   // $FE
        }
    }
};

} // namespace nes_system
