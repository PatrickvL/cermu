#pragma once
/*
 * mapper_067_sunsoft3.h — iNES Mapper 067 (Sunsoft-3)
 *
 * 16KB switchable + 16KB fixed PRG, 4×2KB CHR banks.
 * Includes an IRQ counter written in two stages (toggle latch).
 * Games: Fantasy Zone II, Spy vs Spy (Japan).
 *
 * $8800: CHR bank 0 (2KB at $0000)
 * $9800: CHR bank 1 (2KB at $0800)
 * $A800: CHR bank 2 (2KB at $1000)
 * $B800: CHR bank 3 (2KB at $1800)
 * $C800: IRQ counter load (high/low alternating)
 * $D800: IRQ enable + counter reset
 * $E800: Mirroring
 * $F800: PRG bank select (16KB at $8000)
 */

#include "../nes_mapper.h"
#include "mapper_helpers.h"

namespace nes_system {

class Mapper067 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_[4] = {};

    Mirror mirror_mode_ = Mirror::VERTICAL;

    // IRQ
    bool irq_enabled_ = false;
    bool irq_active_ = false;
    uint16_t irq_counter_ = 0;
    bool irq_latch_toggle_ = false;  // Alternates high/low byte writes

public:
    Mapper067(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        for (int i = 0; i < 4; i++) chr_bank_[i] = i;
        mirror_mode_ = header_mirror_;
        irq_enabled_ = false;
        irq_active_ = false;
        irq_counter_ = 0;
        irq_latch_toggle_ = false;
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_active_; }
    void irq_clear() override { irq_active_ = false; }

    void notify_a12(bool a12_high, uint64_t /*ppu_cycle*/) override {
        if (!irq_enabled_ || !a12_high) return;
        if (irq_counter_ > 0) {
            irq_counter_--;
            if (irq_counter_ == 0) {
                irq_active_ = true;
            }
        }
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_2k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0800) : 1;
        if (chr_2k == 0) chr_2k = 1;

        for (int slot = 0; slot < 4; slot++) {
            uint32_t b = chr_bank_[slot] % chr_2k;
            for (int i = 0; i < 2; i++) {
                uint32_t offset = b * 0x0800 + i * 0x0400;
                config.chr_pages[slot * 2 + i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                config.chr_writable[slot * 2 + i] = chr_is_ram_;
            }
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xF800) {
            case 0x8800: chr_bank_[0] = data; return true;
            case 0x9800: chr_bank_[1] = data; return true;
            case 0xA800: chr_bank_[2] = data; return true;
            case 0xB800: chr_bank_[3] = data; return true;

            case 0xC800:
                if (!irq_latch_toggle_) {
                    irq_counter_ = (irq_counter_ & 0x00FF) | (static_cast<uint16_t>(data) << 8);
                } else {
                    irq_counter_ = (irq_counter_ & 0xFF00) | data;
                }
                irq_latch_toggle_ = !irq_latch_toggle_;
                return false;

            case 0xD800:
                irq_latch_toggle_ = false;
                irq_enabled_ = (data & 0x10) != 0;
                irq_active_ = false;
                return false;

            case 0xE800:
                switch (data & 0x03) {
                    case 0: mirror_mode_ = Mirror::VERTICAL;     break;
                    case 1: mirror_mode_ = Mirror::HORIZONTAL;   break;
                    case 2: mirror_mode_ = Mirror::ONESCREEN_LO; break;
                    case 3: mirror_mode_ = Mirror::ONESCREEN_HI; break;
                }
                return true;

            case 0xF800:
                prg_bank_select_ = data;
                return true;
        }
        return false;
    }
};

} // namespace nes_system
