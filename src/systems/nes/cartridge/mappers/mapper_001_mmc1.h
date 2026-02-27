#pragma once
/*
 * mapper_001_mmc1.h — iNES Mapper 001 (MMC1 / Nintendo SxROM)
 *
 * Serial shift-register interface controlling PRG/CHR bank switching
 * and nametable mirroring.
 * Games: Zelda, Metroid, Mega Man 2, Final Fantasy, Kid Icarus, etc.
 */

#include "../nes_mapper.h"

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

    bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            mapped_addr = 0xFFFFFFFF;  // sentinel for PRG RAM
            return true;
        }

        if (addr >= 0x8000) {
            uint8_t prg_mode = (reg_control_ >> 2) & 0x03;

            if (prg_mode <= 1) {
                // 32KB mode: ignore low bit of bank number
                uint32_t bank = (reg_prg_bank_ & 0x0E) >> 1;
                mapped_addr = bank * 0x8000 + (addr & 0x7FFF);
            } else if (prg_mode == 2) {
                // Fix first bank at $8000, switch second at $C000
                if (addr < 0xC000) {
                    mapped_addr = addr & 0x3FFF;
                } else {
                    mapped_addr = (reg_prg_bank_ & 0x0F) * 0x4000 + (addr & 0x3FFF);
                }
            } else {
                // Fix last bank at $C000, switch first at $8000
                if (addr < 0xC000) {
                    mapped_addr = (reg_prg_bank_ & 0x0F) * 0x4000 + (addr & 0x3FFF);
                } else {
                    mapped_addr = (prg_banks_ - 1) * 0x4000 + (addr & 0x3FFF);
                }
            }
            return true;
        }
        return false;
    }

    bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override {
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            mapped_addr = 0xFFFFFFFF;  // sentinel for PRG RAM
            return true;
        }

        if (addr >= 0x8000) {
            if (data & 0x80) {
                // Reset shift register
                shift_register_ = 0x10;
                write_count_ = 0;
                reg_control_ |= 0x0C;  // Reset to PRG bank mode 3
                update_mirroring();
            } else {
                shift_register_ >>= 1;
                shift_register_ |= (data & 0x01) << 4;
                write_count_++;

                if (write_count_ == 5) {
                    uint8_t target = (addr >> 13) & 0x03;  // Which register

                    switch (target) {
                        case 0: // $8000-$9FFF — Control
                            reg_control_ = shift_register_ & 0x1F;
                            update_mirroring();
                            break;
                        case 1: // $A000-$BFFF — CHR bank 0
                            reg_chr_bank0_ = shift_register_ & 0x1F;
                            break;
                        case 2: // $C000-$DFFF — CHR bank 1
                            reg_chr_bank1_ = shift_register_ & 0x1F;
                            break;
                        case 3: // $E000-$FFFF — PRG bank
                            reg_prg_bank_ = shift_register_ & 0x0F;
                            prg_ram_enabled_ = !(shift_register_ & 0x10);
                            break;
                    }

                    shift_register_ = 0x10;
                    write_count_ = 0;
                }
            }
            mapped_addr = 0;
            return false;  // Don't write to PRG ROM
        }
        return false;
    }

    bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF) {
            if (chr_banks_ == 0) {
                // CHR RAM — direct mapping
                mapped_addr = addr;
                return true;
            }

            bool chr_mode = (reg_control_ & 0x10) != 0;

            if (!chr_mode) {
                // 8KB mode
                uint32_t bank = (reg_chr_bank0_ & 0x1E) >> 1;
                mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
            } else {
                // 4KB mode
                if (addr < 0x1000) {
                    mapped_addr = reg_chr_bank0_ * 0x1000 + (addr & 0x0FFF);
                } else {
                    mapped_addr = reg_chr_bank1_ * 0x1000 + (addr & 0x0FFF);
                }
            }
            return true;
        }
        return false;
    }

    bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr <= 0x1FFF && chr_banks_ == 0) {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

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
};

} // namespace nes_system
