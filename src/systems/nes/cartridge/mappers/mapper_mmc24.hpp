#pragma once
/*
 * mapper_mmc24.h — Shared MMC2/MMC4 template (mappers 009/010)
 *
 * Both mappers use identical CHR auto-latch switching ($FD/$FE tile
 * fetches trigger bank swaps) and the same register layout.
 * The only difference is PRG banking:
 *   MMC2 (009): 8KB switchable at $8000, three fixed 8KB at $A000-$FFFF
 *   MMC4 (010): 16KB switchable at $8000, fixed 16KB at $C000
 *
 * Template parameter:
 *   Is8kPRG — true = MMC2 (8KB PRG switch), false = MMC4 (16KB)
 *
 * Games:
 *   MMC2: Mike Tyson's Punch-Out!!, Punch-Out!!
 *   MMC4: Fire Emblem, Fire Emblem Gaiden
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

template<bool Is8kPRG>
class MapperMMC24 : public Mapper {
private:
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
    MapperMMC24(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

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

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        if constexpr (Is8kPRG) {
            // MMC2: 8KB switchable at $8000, three fixed 8KB at $A000-$FFFF
            uint32_t total_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
            if (total_8k == 0) total_8k = 1;

            uint32_t b0 = prg_bank_select_ % total_8k;
            config.prg_pages[0] = prg_rom_ + b0 * 0x2000;
            config.prg_pages[1] = prg_rom_ + b0 * 0x2000 + 0x1000;

            for (int slot = 1; slot < 4; slot++) {
                uint32_t b = (total_8k >= (uint32_t)(4 - slot))
                                 ? (total_8k - (4 - slot)) : 0;
                config.prg_pages[slot * 2]     = prg_rom_ + b * 0x2000;
                config.prg_pages[slot * 2 + 1] = prg_rom_ + b * 0x2000 + 0x1000;
            }
        } else {
            // MMC4: 16KB switchable at $8000, fixed 16KB at $C000
            mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_,
                                           prg_bank_select_);
        }
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_4k = (chr_mem_size_ > 0)
                              ? static_cast<uint32_t>(chr_mem_size_ / 0x1000) : 1;
        if (chr_4k == 0) chr_4k = 1;

        // $0000-$0FFF: selected by latch_0_
        uint32_t bank_lo = (latch_0_ ? chr_bank_0_fe_ : chr_bank_0_fd_) % chr_4k;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = bank_lo * 0x1000 + i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_)
                                      ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }

        // $1000-$1FFF: selected by latch_1_
        uint32_t bank_hi = (latch_1_ ? chr_bank_1_fe_ : chr_bank_1_fd_) % chr_4k;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = bank_hi * 0x1000 + i * 0x0400;
            config.chr_pages[4 + i] = (offset < chr_mem_size_)
                                          ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[4 + i] = chr_is_ram_;
        }
    }

    // =======================================================================
    // Register writes ($A000-$F000)
    // =======================================================================

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xF000) {
            case 0xA000: prg_bank_select_ = data & 0x0F; return true;
            case 0xB000: chr_bank_0_fd_   = data & 0x1F; return true;
            case 0xC000: chr_bank_0_fe_   = data & 0x1F; return true;
            case 0xD000: chr_bank_1_fd_   = data & 0x1F; return true;
            case 0xE000: chr_bank_1_fe_   = data & 0x1F; return true;
            case 0xF000:
                mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL
                                             : Mirror::VERTICAL;
                return true;
        }
        return false;
    }

    // =======================================================================
    // PPU bus read — CHR latch switching
    // =======================================================================
    //
    // Detects pattern-table address ranges that trigger the MMC2/4 CHR
    // latch mechanism ($FD/$FE tile indices).  Returns true if a latch
    // switched, signalling the caller to rebuild the bank map.

    bool ppu_bus_read(uint16_t addr) override {
        const bool old_l0 = latch_0_;
        const bool old_l1 = latch_1_;

        if      (addr >= 0x0FD8 && addr <= 0x0FDF) latch_0_ = false; // $FD
        else if (addr >= 0x0FE8 && addr <= 0x0FEF) latch_0_ = true;  // $FE
        else if (addr >= 0x1FD8 && addr <= 0x1FDF) latch_1_ = false; // $FD
        else if (addr >= 0x1FE8 && addr <= 0x1FEF) latch_1_ = true;  // $FE

        return (latch_0_ != old_l0) || (latch_1_ != old_l1);
    }
};

// Mapper 009 — MMC2 (PxROM): 8KB switchable PRG
using Mapper009 = MapperMMC24<true>;

// Mapper 010 — MMC4 (FxROM): 16KB switchable PRG
using Mapper010 = MapperMMC24<false>;

} // namespace nes_system
