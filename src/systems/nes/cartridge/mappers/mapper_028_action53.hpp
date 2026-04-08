#pragma once
/*
 * mapper_028_action53.h — iNES Mapper 028 (Action 53)
 *
 * Multi-discrete mapper used by the Action 53 multi-cart compilation and
 * several homebrew test ROMs. Supports multiple PRG/CHR banking modes via
 * a register-select + data-write protocol.
 *
 * Register select: write to $5000 selects which internal register
 * subsequent writes to $8000-$FFFF update.
 * Register data:   write to $8000-$FFFF updates the selected register.
 *
 * Registers:
 *   $00: CHR bank register
 *   $01: Inner bank register
 *   $80: Mode register (mirroring mode, PRG banking mode, PRG outer bank size)
 *   $81: Outer bank register
 *
 * PRG banking modes (bits 3-2 of mode register):
 *   0: 32KB switchable (inner OR outer, A14 from CPU)
 *   1: 32KB fixed (outer bank only)
 *   2: 16KB — fixed first ($8000), switchable last ($C000) (Mapper 180 style)
 *   3: 16KB — switchable first ($8000), fixed last ($C000) (UNROM style)
 *
 * References: https://www.nesdev.org/wiki/Action_53_mapper
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper028 : public Mapper {
private:

    uint8_t reg_select_ = 0x80; // Active register selection
    uint8_t reg_chr_ = 0;       // $00: CHR bank
    uint8_t reg_inner_ = 0;     // $01: Inner PRG bank
    uint8_t reg_mode_ = 0;      // $80: Mode
    uint8_t reg_outer_ = 0;     // $81: Outer PRG bank

    Mirror get_mirror_mode() const {
        switch (reg_mode_ & 0x03) {
            case 0: return Mirror::ONESCREEN_LO;
            case 1: return Mirror::ONESCREEN_HI;
            case 2: return Mirror::VERTICAL;
            case 3: return Mirror::HORIZONTAL;
        }
        return Mirror::VERTICAL;
    }

    // Compute the effective 16KB PRG bank for a given slot
    uint32_t get_prg_16k_bank(bool upper_slot) const {
        uint8_t prg_mode = (reg_mode_ >> 2) & 0x03;
        uint8_t outer_bank_size = (reg_mode_ >> 4) & 0x03;

        // Inner bank mask based on outer bank size
        // outer_bank_size: 0 → 32KB (1 inner bit), 1 → 64KB (2 bits),
        //                  2 → 128KB (4 bits), 3 → 256KB (8 bits)
        uint8_t inner_mask = (1 << (outer_bank_size + 1)) - 1;
        uint8_t outer_mask = ~inner_mask & 0xFF;

        uint8_t outer = reg_outer_ & outer_mask;
        uint8_t inner = reg_inner_ & inner_mask;

        uint8_t combined = outer | inner;

        switch (prg_mode) {
            case 0:
                // 32KB mode — inner bank provides all bits within outer slot.
                // A14 comes from CPU address (bit 0 of bank index).
                return upper_slot ? (combined | 1) : (combined & ~1);

            case 1:
                // 32KB fixed — outer bank provides all bits.
                return upper_slot ? (reg_outer_ | 1) : (reg_outer_ & ~1);

            case 2:
                // Mapper 180 style: fixed $8000, switchable $C000
                return upper_slot ? combined : outer;

            case 3:
                // UNROM style: switchable $8000, fixed $C000
                return upper_slot ? static_cast<uint8_t>(outer | inner_mask) : combined;
        }
        return 0;
    }

public:
    Mapper028(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        reg_select_ = 0x80;
        reg_chr_ = 0;
        reg_inner_ = 0;
        reg_mode_ = 0;
        reg_outer_ = 0x3F;  // Default: last outer bank
    }

    Mirror mirror() override { return get_mirror_mode(); }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t total_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (total_16k == 0) total_16k = 1;

        // Lower 16KB ($8000-$BFFF)
        uint32_t bank_lo = get_prg_16k_bank(false) % total_16k;
        uint32_t base_lo = bank_lo * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = base_lo + i * 0x1000;
            config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }

        // Upper 16KB ($C000-$FFFF)
        uint32_t bank_hi = get_prg_16k_bank(true) % total_16k;
        uint32_t base_hi = bank_hi * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = base_hi + i * 0x1000;
            config.prg_pages[4 + i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }

        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        if (chr_is_ram_) {
            // CHR-RAM: use reg_chr_ to select 8KB bank
            uint32_t max_8k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x2000) : 1;
            uint32_t bank = (max_8k > 1) ? (reg_chr_ % max_8k) : 0;
            uint32_t chr_base = bank * 0x2000;
            for (int i = 0; i < 8; i++) {
                uint32_t offset = chr_base + i * 0x0400;
                config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                config.chr_writable[i] = true;
            }
        } else {
            // CHR-ROM: simple 8KB bank select
            uint32_t max_8k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x2000) : 1;
            uint32_t bank = (max_8k > 0) ? (reg_chr_ % max_8k) : 0;
            uint32_t chr_base = bank * 0x2000;
            for (int i = 0; i < 8; i++) {
                uint32_t offset = chr_base + i * 0x0400;
                config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                config.chr_writable[i] = false;
            }
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x5000 && addr < 0x6000) {
            // Register select
            reg_select_ = data & 0x81;  // Only bits 7 and 0 are significant
            // If bit 7 is clear, this also updates the CHR register
            if (!(data & 0x80)) {
                reg_chr_ = data & 0x03;
                return true;
            }
            return false;
        }

        if (addr >= 0x8000) {
            // Write the selected register
            switch (reg_select_) {
                case 0x00: reg_chr_   = data & 0x03; return true;
                case 0x01: reg_inner_ = data & 0x0F; return true;
                case 0x80: reg_mode_  = data;        return true;
                case 0x81: reg_outer_ = data & 0x3F; return true;
            }
        }
        return false;
    }
};

} // namespace nes_system
