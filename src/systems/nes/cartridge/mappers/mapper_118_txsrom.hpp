#pragma once
/*
 * mapper_118_txsrom.h — iNES Mapper 118 (TxSROM / MMC3 variant)
 *
 * Identical to standard MMC3 (Mapper 004) except nametable mirroring
 * is controlled by CHR bank D7 instead of the $A000 mirroring register.
 * This allows per-nametable-slot CIRAM page selection.
 *
 * Games: Armadillo, Pro Sport Hockey, Goal! Two
 *
 * Registers: Same as MMC3, but:
 *   $A000 mirroring register is IGNORED.
 *   Nametable page for each slot is taken from bit 7 of the CHR bank
 *   register that maps into that slot's address range.
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include <cstring>

namespace nes_system {

class Mapper118 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t target_register_ = 0;
    bool prg_bank_mode_ = false;
    bool chr_inversion_ = false;
    uint8_t registers_[8] = {};

    bool prg_ram_enabled_ = true;
    bool prg_ram_write_protect_ = false;

    // IRQ (same as MMC3)
    uint8_t irq_counter_ = 0;
    uint8_t irq_reload_value_ = 0;
    bool irq_enabled_ = false;
    bool irq_active_ = false;
    bool irq_reload_ = false;
    uint64_t a12_low_since_ = 0;
    static constexpr uint16_t A12_FILTER_DELAY = 16;

    uint32_t prg_bank_[4] = {};
    uint32_t chr_bank_[8] = {};

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

    void update_chr_banks() {
        uint32_t chr_size = chr_banks_ == 0 ? 8 : chr_banks_ * 8;
        if (!chr_inversion_) {
            chr_bank_[0] = ((registers_[0] & 0xFE) + 0) % chr_size;
            chr_bank_[1] = ((registers_[0] & 0xFE) + 1) % chr_size;
            chr_bank_[2] = ((registers_[1] & 0xFE) + 0) % chr_size;
            chr_bank_[3] = ((registers_[1] & 0xFE) + 1) % chr_size;
            chr_bank_[4] = registers_[2] % chr_size;
            chr_bank_[5] = registers_[3] % chr_size;
            chr_bank_[6] = registers_[4] % chr_size;
            chr_bank_[7] = registers_[5] % chr_size;
        } else {
            chr_bank_[0] = registers_[2] % chr_size;
            chr_bank_[1] = registers_[3] % chr_size;
            chr_bank_[2] = registers_[4] % chr_size;
            chr_bank_[3] = registers_[5] % chr_size;
            chr_bank_[4] = ((registers_[0] & 0xFE) + 0) % chr_size;
            chr_bank_[5] = ((registers_[0] & 0xFE) + 1) % chr_size;
            chr_bank_[6] = ((registers_[1] & 0xFE) + 0) % chr_size;
            chr_bank_[7] = ((registers_[1] & 0xFE) + 1) % chr_size;
        }
    }

public:
    Mapper118(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

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
        update_prg_banks();
        update_chr_banks();
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

        // TxSROM: nametable mirroring from CHR bank D7
        // R0 covers slots 0-1, R1 covers slots 2-3 (or inverted)
        // R2-R5 cover slots 4-7 (or inverted)
        if (!chr_inversion_) {
            config.nt_page[0] = (registers_[0] >> 7) & 0x01;
            config.nt_page[1] = (registers_[1] >> 7) & 0x01;
            config.nt_page[2] = (registers_[2] >> 7) & 0x01;
            config.nt_page[3] = (registers_[3] >> 7) & 0x01;
        } else {
            config.nt_page[0] = (registers_[2] >> 7) & 0x01;
            config.nt_page[1] = (registers_[3] >> 7) & 0x01;
            config.nt_page[2] = (registers_[0] >> 7) & 0x01;
            config.nt_page[3] = (registers_[1] >> 7) & 0x01;
        }
    }

    // Do NOT override mirror() — TxSROM uses nt_page[] from get_chr_bank_config()

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
            update_chr_banks();
            return true;
        } else if (addr <= 0xBFFF) {
            if (even) {
                // Mirroring register is IGNORED on TxSROM — controlled by CHR D7
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
