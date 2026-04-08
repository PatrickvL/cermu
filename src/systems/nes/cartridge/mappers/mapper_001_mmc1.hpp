#pragma once
/*
 * mapper_001_mmc1.h — iNES Mapper 001 (MMC1 / Nintendo SxROM)
 *
 * Serial shift-register interface controlling PRG/CHR bank switching
 * and nametable mirroring.
 * Games: Zelda, Metroid, Mega Man 2, Final Fantasy, Kid Icarus, etc.
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper001 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    // Shift register (serial writes)
    uint8_t shift_register_ = 0x10;  // bit 4 set = "empty"
    uint8_t write_count_ = 0;

    // Internal registers
    uint8_t reg_control_ = 0x0C;   // $8000-$9FFF — control
    uint8_t reg_chr_bank0_ = 0;    // $A000-$BFFF — CHR bank 0
    uint8_t reg_chr_bank1_ = 0;    // $C000-$DFFF — CHR bank 1
    uint8_t reg_prg_bank_ = 0;     // $E000-$FFFF — PRG bank

    // PRG RAM enable
    bool prg_ram_enabled_ = true;

    // Derived state
    Mirror mirror_mode_ = Mirror::HORIZONTAL;

    void update_mirroring() {
        switch (reg_control_ & 0x03) {
            case 0: mirror_mode_ = Mirror::ONESCREEN_LO; break;
            case 1: mirror_mode_ = Mirror::ONESCREEN_HI; break;
            case 2: mirror_mode_ = Mirror::VERTICAL;     break;
            case 3: mirror_mode_ = Mirror::HORIZONTAL;   break;
        }
    }

public:
    Mapper001(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    Mirror mirror() override { return mirror_mode_; }

    void reset() override {
        shift_register_ = 0x10;
        write_count_ = 0;
        reg_control_ = 0x0C;
        reg_chr_bank0_ = 0;
        reg_chr_bank1_ = 0;
        reg_prg_bank_ = 0;
        prg_ram_enabled_ = true;
        update_mirroring();
    }

    // =======================================================================
    // Phase 2 — page-pointer bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint8_t prg_mode = (reg_control_ >> 2) & 0x03;

        // SUROM/SXROM: 512KB PRG boards use CHR bank 0 bit 4 as PRG A18,
        // selecting between two 256KB halves.
        uint32_t prg_base = 0;
        if (prg_rom_size_ > 0x40000) {
            prg_base = (reg_chr_bank0_ & 0x10) ? 0x40000 : 0;
        }

        if (prg_mode <= 1) {
            // 32KB mode: ignore low bit of bank number
            uint32_t bank = (reg_prg_bank_ & 0x0E) >> 1;
            uint32_t base = prg_base + bank * 0x8000;
            for (int i = 0; i < 8; i++) {
                uint32_t offset = base + i * 0x1000;
                config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
            }
        } else if (prg_mode == 2) {
            // Fix first bank at $8000, switch second at $C000
            for (int i = 0; i < 4; i++) {
                uint32_t offset = prg_base + i * 0x1000;
                config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
            }
            uint32_t bank_base = prg_base + (reg_prg_bank_ & 0x0F) * 0x4000;
            for (int i = 0; i < 4; i++) {
                uint32_t offset = bank_base + i * 0x1000;
                config.prg_pages[4 + i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
            }
        } else {
            // Fix last bank at $C000, switch first at $8000
            uint32_t bank_base = prg_base + (reg_prg_bank_ & 0x0F) * 0x4000;
            for (int i = 0; i < 4; i++) {
                uint32_t offset = bank_base + i * 0x1000;
                config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
            }
            // Fixed last 16KB within the 256KB half
            uint32_t last_base = prg_base + 0x3C000; // last 16KB of 256KB half
            for (int i = 0; i < 4; i++) {
                uint32_t offset = last_base + i * 0x1000;
                config.prg_pages[4 + i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
            }
        }

        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = prg_ram_enabled_ && (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        if (chr_is_ram_) {
            // CHR-RAM — direct mapping
            for (int i = 0; i < 8; i++) {
                config.chr_pages[i] = chr_mem_ + (i * 0x0400);
                config.chr_writable[i] = true;
            }
        } else {
            bool chr_mode = (reg_control_ & 0x10) != 0;

            if (!chr_mode) {
                // 8KB mode
                uint32_t bank = (reg_chr_bank0_ & 0x1E) >> 1;
                uint32_t base = bank * 0x2000;
                for (int i = 0; i < 8; i++) {
                    uint32_t offset = base + i * 0x0400;
                    config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : nullptr;
                    config.chr_writable[i] = false;
                }
            } else {
                // 4KB mode
                uint32_t base0 = reg_chr_bank0_ * 0x1000;
                for (int i = 0; i < 4; i++) {
                    uint32_t offset = base0 + i * 0x0400;
                    config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : nullptr;
                    config.chr_writable[i] = false;
                }
                uint32_t base1 = reg_chr_bank1_ * 0x1000;
                for (int i = 0; i < 4; i++) {
                    uint32_t offset = base1 + i * 0x0400;
                    config.chr_pages[4 + i] = (offset < chr_mem_size_) ? chr_mem_ + offset : nullptr;
                    config.chr_writable[i + 4] = false;
                }
            }
        }

        // Nametable mirroring is set by Cartridge::update_bank_map()
        // from mapper->mirror() — no need to set nt_page here.
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        if (data & 0x80) {
            // Reset shift register
            shift_register_ = 0x10;
            write_count_ = 0;
            reg_control_ |= 0x0C;
            update_mirroring();
            return true;  // banking may have changed
        }

        shift_register_ >>= 1;
        shift_register_ |= (data & 0x01) << 4;
        write_count_++;

        if (write_count_ == 5) {
            uint8_t target = (addr >> 13) & 0x03;

            switch (target) {
                case 0:
                    reg_control_ = shift_register_ & 0x1F;
                    update_mirroring();
                    break;
                case 1:
                    reg_chr_bank0_ = shift_register_ & 0x1F;
                    break;
                case 2:
                    reg_chr_bank1_ = shift_register_ & 0x1F;
                    break;
                case 3:
                    reg_prg_bank_ = shift_register_ & 0x0F;
                    prg_ram_enabled_ = !(shift_register_ & 0x10);
                    break;
            }

            shift_register_ = 0x10;
            write_count_ = 0;
            return true;  // banking changed
        }

        return false;  // shift register not yet full
    }
};

} // namespace nes_system
