#pragma once
/*
 * mapper_119_tqrom.h — iNES Mapper 119 (TQROM / MMC3 variant)
 *
 * Like standard MMC3 but with both CHR-ROM and CHR-RAM.
 * Bit 6 of the CHR bank register selects between ROM (0) and RAM (1).
 * 8KB CHR-RAM is available for bank values with bit 6 set.
 *
 * Games: High Speed, Pin-Bot, Rad Racer II (unreleased)
 *
 * CHR bank interpretation:
 *   D5-D0: CHR-ROM bank (when D6 = 0)
 *   D1-D0: CHR-RAM bank (when D6 = 1), addresses 8KB CHR-RAM as 1KB pages
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include <cstring>

namespace nes_system {

class Mapper119 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t target_register_ = 0;
    bool prg_bank_mode_ = false;
    bool chr_inversion_ = false;
    uint8_t registers_[8] = {};

    bool prg_ram_enabled_ = true;
    bool prg_ram_write_protect_ = false;

    Mirror mirror_mode_ = Mirror::HORIZONTAL;

    // IRQ (same as MMC3)
    uint8_t irq_counter_ = 0;
    uint8_t irq_reload_value_ = 0;
    bool irq_enabled_ = false;
    bool irq_active_ = false;
    bool irq_reload_ = false;
    uint64_t a12_low_since_ = 0;
    static constexpr uint16_t A12_FILTER_DELAY = 16;

    uint32_t prg_bank_[4] = {};

    // CHR-RAM pointer (set via set_memory_pointers — we re-use chr_mem_
    // for CHR-ROM and need a separate pointer for TQROM's on-board CHR-RAM)
    // TQROM boards have 8KB CHR-RAM on-board. We'll use chr_mem_ as CHR-ROM
    // and allocate CHR-RAM in a fixed 8KB buffer.
    uint8_t chr_ram_buf_[8192] = {};

    void update_prg_banks() {
        uint32_t last_bank = (prg_banks_ * 2) - 1;
        if (!prg_bank_mode_) {
            prg_bank_[0] = (registers_[6] & 0x3F) % (prg_banks_ * 2);
            prg_bank_[1] = (registers_[7] & 0x3F) % (prg_banks_ * 2);
            prg_bank_[2] = (last_bank - 1) % (prg_banks_ * 2);
            prg_bank_[3] = last_bank % (prg_banks_ * 2);
        } else {
            prg_bank_[0] = (last_bank - 1) % (prg_banks_ * 2);
            prg_bank_[1] = (registers_[7] & 0x3F) % (prg_banks_ * 2);
            prg_bank_[2] = (registers_[6] & 0x3F) % (prg_banks_ * 2);
            prg_bank_[3] = last_bank % (prg_banks_ * 2);
        }
    }

public:
    Mapper119(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {
        std::memset(chr_ram_buf_, 0, sizeof(chr_ram_buf_));
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_active_; }
    void irq_clear() override { irq_active_ = false; }

    void notify_a12(bool a12_high, uint64_t ppu_cycle) override {
        if (!a12_high) { a12_low_since_ = ppu_cycle; return; }
        if (ppu_cycle - a12_low_since_ < A12_FILTER_DELAY) return;

        if (irq_counter_ == 0 || irq_reload_) {
            irq_counter_ = irq_reload_value_;
            irq_reload_ = false;
        } else {
            irq_counter_--;
        }
        if (irq_counter_ == 0 && irq_enabled_) irq_active_ = true;
    }

    void reset() override {
        target_register_ = 0;
        prg_bank_mode_ = false;
        chr_inversion_ = false;
        std::memset(registers_, 0, sizeof(registers_));
        prg_ram_enabled_ = true;
        prg_ram_write_protect_ = false;
        irq_counter_ = 0;
        irq_reload_value_ = 0;
        irq_enabled_ = false;
        irq_active_ = false;
        irq_reload_ = false;
        a12_low_since_ = 0;
        mirror_mode_ = Mirror::HORIZONTAL;
        update_prg_banks();
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        for (int slot = 0; slot < 4; slot++) {
            uint32_t bank_base = prg_bank_[slot] * 0x2000;
            for (int half = 0; half < 2; half++) {
                uint32_t offset = bank_base + half * 0x1000;
                config.prg_pages[slot * 2 + half] =
                    (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
            }
        }
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = prg_ram_enabled_ && (prg_ram_ != nullptr);
        config.prg_ram_write_protected = prg_ram_write_protect_;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // Build the CHR bank assignment considering chr_inversion_
        uint8_t slot_regs[8];
        if (!chr_inversion_) {
            slot_regs[0] = registers_[0] & 0xFE;       // 2KB at $0000
            slot_regs[1] = (registers_[0] & 0xFE) | 1;
            slot_regs[2] = registers_[1] & 0xFE;       // 2KB at $0800
            slot_regs[3] = (registers_[1] & 0xFE) | 1;
            slot_regs[4] = registers_[2];               // 1KB at $1000
            slot_regs[5] = registers_[3];
            slot_regs[6] = registers_[4];
            slot_regs[7] = registers_[5];
        } else {
            slot_regs[0] = registers_[2];
            slot_regs[1] = registers_[3];
            slot_regs[2] = registers_[4];
            slot_regs[3] = registers_[5];
            slot_regs[4] = registers_[0] & 0xFE;
            slot_regs[5] = (registers_[0] & 0xFE) | 1;
            slot_regs[6] = registers_[1] & 0xFE;
            slot_regs[7] = (registers_[1] & 0xFE) | 1;
        }

        uint32_t chr_rom_1k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;

        for (int i = 0; i < 8; i++) {
            bool use_ram = (slot_regs[i] & 0x40) != 0;
            if (use_ram) {
                // CHR-RAM: D1-D0 select 1KB page within 8KB RAM
                uint32_t ram_page = slot_regs[i] & 0x07;
                uint32_t offset = (ram_page * 0x0400) % sizeof(chr_ram_buf_);
                config.chr_pages[i] = const_cast<const uint8_t*>(chr_ram_buf_ + offset);
                config.chr_writable[i] = true;
            } else {
                // CHR-ROM: D5-D0 select 1KB page
                uint32_t bank = (slot_regs[i] & 0x3F) % chr_rom_1k;
                config.chr_pages[i] = chr_mem_ + (bank * 0x0400);
                config.chr_writable[i] = false;
            }
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        bool even = !(addr & 0x0001);

        if (addr <= 0x9FFF) {
            if (even) {
                target_register_ = data & 0x07;
                prg_bank_mode_ = (data & 0x40) != 0;
                chr_inversion_ = (data & 0x80) != 0;
            } else {
                registers_[target_register_] = data;
            }
            update_prg_banks();
            return true;
        } else if (addr <= 0xBFFF) {
            if (even) {
                mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            } else {
                prg_ram_enabled_ = (data & 0x80) != 0;
                prg_ram_write_protect_ = (data & 0x40) != 0;
            }
            return true;
        } else if (addr <= 0xDFFF) {
            if (even) {
                irq_reload_value_ = data;
            } else {
                irq_counter_ = 0;
                irq_reload_ = true;
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
