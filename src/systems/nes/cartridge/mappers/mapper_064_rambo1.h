#pragma once
/*
 * mapper_064_rambo1.h — iNES Mapper 064 (Tengen RAMBO-1)
 *
 * Tengen's MMC3-like mapper with additional features:
 * - 1KB CHR granularity in both halves (8 individually selectable 1KB banks)
 * - Optional 8KB/16KB PRG bank modes (like MMC3) plus an extra third mode
 * - Scanline counter with cycle-based IRQ (different from MMC3)
 * Games: Klax, Skull & Crossbones, Shinobi, Rolling Thunder.
 *
 * Register interface mirrors MMC3 ($8000-$FFFF even/odd pairs).
 */

#include "../nes_mapper.h"
#include <cstring>

namespace nes_system {

class Mapper064 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t target_register_ = 0;
    bool prg_bank_mode_ = false;     // Bit 6 of $8000
    bool chr_1k_mode_ = false;       // Bit 5 of $8000 — RAMBO-1 extension
    bool chr_inversion_ = false;     // Bit 7 of $8000
    uint8_t registers_[9] = {};      // R0-R8 (R8 is RAMBO-1 extension)

    Mirror mirror_mode_ = Mirror::HORIZONTAL;

    // IRQ
    uint8_t irq_counter_ = 0;
    uint8_t irq_reload_value_ = 0;
    bool irq_enabled_ = false;
    bool irq_active_ = false;
    bool irq_reload_ = false;
    bool irq_mode_ = false;         // false = scanline (A12), true = cycle

    uint64_t a12_low_since_ = 0;
    static constexpr uint16_t A12_FILTER_DELAY = 16;

    uint32_t prg_bank_[4] = {};
    uint32_t chr_bank_[8] = {};

    void update_prg_banks() {
        uint32_t total_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (total_8k == 0) total_8k = 1;
        uint32_t last = total_8k - 1;

        if (!prg_bank_mode_) {
            prg_bank_[0] = registers_[6] % total_8k;
            prg_bank_[1] = registers_[7] % total_8k;
            prg_bank_[2] = (last > 0) ? (last - 1) : 0;
            prg_bank_[3] = last;
        } else {
            prg_bank_[0] = (last > 0) ? (last - 1) : 0;
            prg_bank_[1] = registers_[7] % total_8k;
            prg_bank_[2] = registers_[6] % total_8k;
            prg_bank_[3] = last;
        }
    }

    void update_chr_banks() {
        uint32_t chr_1k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;
        if (chr_1k == 0) chr_1k = 1;

        if (chr_1k_mode_) {
            // RAMBO-1 extended: all 8 banks are independently selectable 1KB
            if (!chr_inversion_) {
                chr_bank_[0] = registers_[0] % chr_1k;
                chr_bank_[1] = registers_[8] % chr_1k;  // R8 extension
                chr_bank_[2] = registers_[1] % chr_1k;
                chr_bank_[3] = registers_[3] % chr_1k;
                chr_bank_[4] = registers_[2] % chr_1k;
                chr_bank_[5] = registers_[4] % chr_1k;
                chr_bank_[6] = registers_[5] % chr_1k;
                chr_bank_[7] = registers_[3] % chr_1k;
            } else {
                chr_bank_[0] = registers_[2] % chr_1k;
                chr_bank_[1] = registers_[4] % chr_1k;
                chr_bank_[2] = registers_[5] % chr_1k;
                chr_bank_[3] = registers_[3] % chr_1k;
                chr_bank_[4] = registers_[0] % chr_1k;
                chr_bank_[5] = registers_[8] % chr_1k;
                chr_bank_[6] = registers_[1] % chr_1k;
                chr_bank_[7] = registers_[3] % chr_1k;
            }
        } else {
            // Standard MMC3-like 2KB+1KB arrangement
            if (!chr_inversion_) {
                chr_bank_[0] = (registers_[0] & 0xFE) % chr_1k;
                chr_bank_[1] = ((registers_[0] & 0xFE) + 1) % chr_1k;
                chr_bank_[2] = (registers_[1] & 0xFE) % chr_1k;
                chr_bank_[3] = ((registers_[1] & 0xFE) + 1) % chr_1k;
                chr_bank_[4] = registers_[2] % chr_1k;
                chr_bank_[5] = registers_[3] % chr_1k;
                chr_bank_[6] = registers_[4] % chr_1k;
                chr_bank_[7] = registers_[5] % chr_1k;
            } else {
                chr_bank_[0] = registers_[2] % chr_1k;
                chr_bank_[1] = registers_[3] % chr_1k;
                chr_bank_[2] = registers_[4] % chr_1k;
                chr_bank_[3] = registers_[5] % chr_1k;
                chr_bank_[4] = (registers_[0] & 0xFE) % chr_1k;
                chr_bank_[5] = ((registers_[0] & 0xFE) + 1) % chr_1k;
                chr_bank_[6] = (registers_[1] & 0xFE) % chr_1k;
                chr_bank_[7] = ((registers_[1] & 0xFE) + 1) % chr_1k;
            }
        }
    }

public:
    Mapper064(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        target_register_ = 0;
        prg_bank_mode_ = false;
        chr_1k_mode_ = false;
        chr_inversion_ = false;
        std::memset(registers_, 0, sizeof(registers_));
        mirror_mode_ = Mirror::HORIZONTAL;
        irq_counter_ = 0;
        irq_reload_value_ = 0;
        irq_enabled_ = false;
        irq_active_ = false;
        irq_reload_ = false;
        irq_mode_ = false;
        a12_low_since_ = 0;
        update_prg_banks();
        update_chr_banks();
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_active_; }
    void irq_clear() override { irq_active_ = false; }

    void notify_a12(bool a12_high, uint64_t ppu_cycle) override {
        if (irq_mode_) return;  // Cycle mode — not driven by A12

        if (!a12_high) {
            a12_low_since_ = ppu_cycle;
            return;
        }
        if (ppu_cycle - a12_low_since_ < A12_FILTER_DELAY) return;

        if (irq_counter_ == 0 || irq_reload_) {
            irq_counter_ = irq_reload_value_;
            irq_reload_ = false;
        } else {
            irq_counter_--;
        }
        if (irq_counter_ == 0 && irq_enabled_) {
            irq_active_ = true;
        }
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        for (int slot = 0; slot < 4; slot++) {
            uint32_t base = prg_bank_[slot] * 0x2000;
            config.prg_pages[slot * 2]     = (base < prg_rom_size_) ? prg_rom_ + base : nullptr;
            config.prg_pages[slot * 2 + 1] = (base + 0x1000 < prg_rom_size_) ? prg_rom_ + base + 0x1000 : nullptr;
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        for (int i = 0; i < 8; i++) {
            uint32_t offset = chr_bank_[i] * 0x0400;
            if (chr_is_ram_) {
                config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                config.chr_writable[i] = true;
            } else {
                config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : nullptr;
                config.chr_writable[i] = false;
            }
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;
        bool even = !(addr & 1);

        if (addr <= 0x9FFF) {
            if (even) {
                target_register_ = data & 0x0F;  // R0-R8 (up to 9)
                prg_bank_mode_ = (data & 0x40) != 0;
                chr_1k_mode_   = (data & 0x20) != 0;
                chr_inversion_ = (data & 0x80) != 0;
            } else {
                if (target_register_ < 9)
                    registers_[target_register_] = data;
            }
            update_prg_banks();
            update_chr_banks();
            return true;
        } else if (addr <= 0xBFFF) {
            if (even) {
                mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            }
            return true;
        } else if (addr <= 0xDFFF) {
            if (even) {
                irq_reload_value_ = data;
            } else {
                irq_counter_ = 0;
                irq_reload_ = true;
                irq_mode_ = (data & 0x01) != 0;
            }
            return false;
        } else {
            if (even) {
                irq_enabled_ = false;
                irq_active_ = false;
            } else {
                irq_enabled_ = true;
            }
            return false;
        }
    }
};

} // namespace nes_system
