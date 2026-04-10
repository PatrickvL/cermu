#pragma once
/*
 * mapper_004_mmc3.h — iNES Mapper 004 (MMC3 / TxROM)
 *
 * Advanced PRG/CHR bank switching with a scanline-counting IRQ.
 * 8 bank registers (R0-R7) select 2×2KB + 4×1KB CHR and 2×8KB PRG windows.
 * PPU A12 rising edges drive the IRQ counter (via notify_a12()).
 * Games: Super Mario Bros. 2/3, Kirby's Adventure, Mega Man 3-6, etc.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>  // std::memset

namespace nes_system {

class Mapper004 : public Mapper {
private:

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

    // A12 IRQ counter (composable)
    mapper_helpers::MMC3IRQ irq_;

    // Derived banks for fast lookup
    uint32_t prg_bank_[4] = {};  // 4 × 8KB PRG banks
    uint32_t chr_bank_[8] = {};  // 8 × 1KB CHR banks

    void update_prg_banks() {
        uint32_t total_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (total_8k == 0) total_8k = 1;
        uint32_t last_bank = total_8k - 1;

        if (!prg_bank_mode_) {
            prg_bank_[0] = (registers_[6] & 0x3F) % total_8k;
            prg_bank_[1] = (registers_[7] & 0x3F) % total_8k;
            prg_bank_[2] = (total_8k >= 2) ? total_8k - 2 : 0;
            prg_bank_[3] = last_bank;
        } else {
            prg_bank_[0] = (total_8k >= 2) ? total_8k - 2 : 0;
            prg_bank_[1] = (registers_[7] & 0x3F) % total_8k;
            prg_bank_[2] = (registers_[6] & 0x3F) % total_8k;
            prg_bank_[3] = last_bank;
        }
    }

    void update_chr_banks() {
        uint32_t chr_1k = static_cast<uint32_t>(chr_mem_size_ / 0x0400);
        if (chr_1k == 0) chr_1k = 1;

        if (!chr_inversion_) {
            chr_bank_[0] = ((registers_[0] & 0xFE) + 0) % chr_1k;
            chr_bank_[1] = ((registers_[0] & 0xFE) + 1) % chr_1k;
            chr_bank_[2] = ((registers_[1] & 0xFE) + 0) % chr_1k;
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
            chr_bank_[4] = ((registers_[0] & 0xFE) + 0) % chr_1k;
            chr_bank_[5] = ((registers_[0] & 0xFE) + 1) % chr_1k;
            chr_bank_[6] = ((registers_[1] & 0xFE) + 0) % chr_1k;
            chr_bank_[7] = ((registers_[1] & 0xFE) + 1) % chr_1k;
        }
    }

public:
    Mapper004(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    Mirror mirror() override { return mirror_mode_; }

    bool irq_state() override { return irq_.active; }

    void irq_clear() override { irq_.active = false; }

    void notify_a12(bool a12_high, uint64_t ppu_cycle) override {
        // Submapper 4 = MMC3A / Rev A (Crystalis) variant IRQ behavior
        irq_.notify_a12(a12_high, ppu_cycle, submapper_ == 4);
    }

    void reset() override {
        target_register_ = 0;
        prg_bank_mode_ = false;
        chr_inversion_ = false;
        std::memset(registers_, 0, sizeof(registers_));
        prg_ram_enabled_ = true;
        prg_ram_write_protect_ = false;
        irq_.reset();
        mirror_mode_ = Mirror::HORIZONTAL;
        update_prg_banks();
        update_chr_banks();
    }

    // =======================================================================
    // Phase 2 — page-pointer bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // 4 × 8KB PRG banks → 8 × 4KB page pointers
        const uint32_t prg_mask = static_cast<uint32_t>(prg_rom_size_) - 1;
        for (int slot = 0; slot < 4; slot++) {
            uint32_t bank_base = prg_bank_[slot] * 0x2000;
            for (int half = 0; half < 2; half++) {
                uint32_t offset = (bank_base + half * 0x1000) & prg_mask;
                config.prg_pages[slot * 2 + half] = prg_rom_ + offset;
            }
        }

        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = prg_ram_enabled_ && (prg_ram_ != nullptr);
        config.prg_ram_write_protected = prg_ram_write_protect_;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // 8 × 1KB CHR banks → 8 × 1KB page pointers
        const uint32_t chr_mask = static_cast<uint32_t>(chr_mem_size_) - 1;
        for (int i = 0; i < 8; i++) {
            uint32_t offset = (chr_bank_[i] * 0x0400) & chr_mask;
            if (chr_is_ram_) {
                config.chr_pages[i] = chr_mem_ + offset;
                config.chr_writable[i] = true;
            } else {
                config.chr_pages[i] = chr_mem_ + offset;
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
        } else {
            irq_.write(addr, data);
            return false;
        }
    }
};

} // namespace nes_system
