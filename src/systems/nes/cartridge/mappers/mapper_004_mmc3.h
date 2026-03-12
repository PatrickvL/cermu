#pragma once
/*
 * mapper_004_mmc3.h — iNES Mapper 004 (MMC3 / TxROM)
 *
 * Advanced PRG/CHR bank switching with a scanline-counting IRQ.
 * 8 bank registers (R0-R7) select 2×2KB + 4×1KB CHR and 2×8KB PRG windows.
 * PPU A12 rising edges drive the IRQ counter (via notify_a12()).
 * Games: Super Mario Bros. 2/3, Kirby's Adventure, Mega Man 3-6, etc.
 */

#include "systems/nes/cartridge/nes_mapper.h"
#include <cstring>  // std::memset

namespace nes_system {

class Mapper004 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    // Bank registers
    uint8_t target_register_ = 0;    // R0-R7 selection
    bool prg_bank_mode_ = false;     // false = $8000 swappable, true = $C000 swappable
    bool chr_inversion_ = false;     // false = 2KB banks at $0000, true = 2KB banks at $1000
    uint8_t registers_[8] = {};      // R0-R7 bank values

    // PRG RAM protect
    bool prg_ram_enabled_ = true;
    bool prg_ram_write_protect_ = false;

    // Mirroring
    Mirror mirror_mode_ = Mirror::HORIZONTAL;

    // IRQ
    uint8_t irq_counter_ = 0;
    uint8_t irq_reload_value_ = 0;
    bool irq_enabled_ = false;
    bool irq_active_ = false;
    bool irq_reload_ = false;

    // A12 filter state — the MMC3's input monitoring circuit has an
    // RC delay that requires A12 to have been low for >= ~16 PPU dots
    // before a subsequent rising edge clocks the IRQ counter.  This
    // prevents spurious counts from brief A12 dips during BG fetches
    // when the BG pattern table is at $1xxx.
    uint64_t a12_low_since_ = 0;
    static constexpr uint16_t A12_FILTER_DELAY = 16;

    // Derived banks for fast lookup
    uint32_t prg_bank_[4] = {};  // 4 × 8KB PRG banks
    uint32_t chr_bank_[8] = {};  // 8 × 1KB CHR banks

    void update_prg_banks() {
        uint32_t last_bank = (prg_banks_ * 2) - 1;  // Total 8KB banks - 1

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
        uint32_t chr_size = chr_banks_ == 0 ? 8 : chr_banks_ * 8; // in 1KB units

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
    Mapper004(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    Mirror mirror() override { return mirror_mode_; }

    bool irq_state() override { return irq_active_; }

    void irq_clear() override { irq_active_ = false; }

    // A12 transition notification — receives raw A12 signal changes from
    // the PPU with the current PPU dot timestamp.  Implements the hardware
    // filter: only clock the IRQ counter on a rising edge where A12 was
    // low for at least A12_FILTER_DELAY PPU dots beforehand.
    void notify_a12(bool a12_high, uint64_t ppu_cycle) override {
        if (!a12_high) {
            // Falling edge — record when A12 went low
            a12_low_since_ = ppu_cycle;
            return;
        }

        // Rising edge — apply filter
        if (ppu_cycle - a12_low_since_ < A12_FILTER_DELAY) {
            return;  // A12 was low too briefly; spurious transition
        }

        // Qualified rising edge — clock the IRQ counter
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
        update_chr_banks();
    }

    // =======================================================================
    // Phase 2 — page-pointer bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // 4 × 8KB PRG banks → 8 × 4KB page pointers
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
        // 8 × 1KB CHR banks → 8 × 1KB page pointers
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

        // Nametable mirroring is set by Cartridge::update_bank_map()
        // from mapper->mirror() — no need to set nt_page here.
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
            update_chr_banks();
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
            return false;  // IRQ config doesn't change banking
        } else {
            if (even) {
                irq_enabled_ = false;
                irq_active_ = false;
            } else {
                irq_enabled_ = true;
            }
            return false;  // IRQ enable doesn't change banking
        }
    }
};

} // namespace nes_system
