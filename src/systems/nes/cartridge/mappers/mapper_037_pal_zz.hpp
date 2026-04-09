#pragma once
/*
 * mapper_037_pal_zz.hpp — iNES Mapper 037 (PAL-ZZ — MMC3 multicart)
 *
 * 3-game MMC3 multicart used exclusively by the PAL "Super Mario Bros +
 * Tetris + Nintendo World Cup" cart.
 *
 * Writing to $6000-$7FFF selects the active block (bits 0-2).
 *
 * Block → PRG mapping:
 *   0-2: page & 0x07                  (banks 0-7,  first  64KB)
 *     3: page & 0x07 | 0x08           (banks 8-15, second 64KB)
 *   4-6: page & 0x0F | 0x10           (banks 16-31, last 128KB)
 *     7: page & 0x07 | 0x20           (banks 32-39, wraps to 0-7)
 *
 * Block → CHR mapping:
 *   0-3: no offset                    (first  128KB CHR)
 *   4-7: page | 0x80                  (second 128KB CHR)
 *
 * Games (as originally configured):
 *   Blocks 0-2: Super Mario Bros      (64KB PRG, 128KB CHR)
 *   Block    3: Tetris                 (64KB PRG, 128KB CHR)
 *   Blocks 4-7: Nintendo World Cup     (128KB PRG, 128KB CHR)
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper037 : public Mapper {
private:

    // Inner MMC3 state
    uint8_t target_register_ = 0;
    bool prg_bank_mode_ = false;
    bool chr_inversion_ = false;
    uint8_t registers_[8] = {};
    Mirror mirror_mode_ = Mirror::HORIZONTAL;

    // Outer block (3-bit)
    uint8_t selected_block_ = 0;

    // PRG-RAM protect (MMC3 $A001 register)
    bool wram_enabled_ = false;

    // IRQ (reuses MMC3IRQ composable)
    mapper_helpers::MMC3IRQ irq_;

    // Derived banks
    uint32_t prg_bank_[4] = {};
    uint32_t chr_bank_[8] = {};

    // Apply block-dependent PRG masking (matches Mesen's SelectPrgPage)
    uint32_t apply_prg_block(uint32_t page) const {
        switch (selected_block_) {
            case 0: case 1: case 2:
                return page & 0x07;
            case 3:
                return (page & 0x07) | 0x08;
            case 7:
                return (page & 0x07) | 0x20;
            default: // 4, 5, 6
                return (page & 0x0F) | 0x10;
        }
    }

    // Apply block-dependent CHR offsetting
    uint32_t apply_chr_block(uint32_t page) const {
        return (selected_block_ >= 4) ? (page | 0x80) : page;
    }

    void update_banks() {
        uint32_t total_prg_8k = mapper_helpers::prg_8k_count(prg_rom_size_);
        uint32_t total_chr_1k = mapper_helpers::chr_1k_count(chr_mem_size_);

        // Fixed banks: MMC3 always maps last two banks of the ROM,
        // but mapper 37's block masking constrains them.
        uint32_t last = apply_prg_block(total_prg_8k - 1) % total_prg_8k;
        uint32_t second_last = apply_prg_block(total_prg_8k - 2) % total_prg_8k;

        uint32_t r6 = apply_prg_block(registers_[6]) % total_prg_8k;
        uint32_t r7 = apply_prg_block(registers_[7]) % total_prg_8k;

        if (!prg_bank_mode_) {
            prg_bank_[0] = r6;          prg_bank_[1] = r7;
            prg_bank_[2] = second_last; prg_bank_[3] = last;
        } else {
            prg_bank_[0] = second_last; prg_bank_[1] = r7;
            prg_bank_[2] = r6;          prg_bank_[3] = last;
        }

        auto chr_page = [&](uint8_t val) -> uint32_t {
            return apply_chr_block(val) % total_chr_1k;
        };

        if (!chr_inversion_) {
            chr_bank_[0] = chr_page(registers_[0] & 0xFE);
            chr_bank_[1] = chr_page((registers_[0] & 0xFE) + 1);
            chr_bank_[2] = chr_page(registers_[1] & 0xFE);
            chr_bank_[3] = chr_page((registers_[1] & 0xFE) + 1);
            for (int i = 0; i < 4; i++)
                chr_bank_[4 + i] = chr_page(registers_[2 + i]);
        } else {
            for (int i = 0; i < 4; i++)
                chr_bank_[i] = chr_page(registers_[2 + i]);
            chr_bank_[4] = chr_page(registers_[0] & 0xFE);
            chr_bank_[5] = chr_page((registers_[0] & 0xFE) + 1);
            chr_bank_[6] = chr_page(registers_[1] & 0xFE);
            chr_bank_[7] = chr_page((registers_[1] & 0xFE) + 1);
        }
    }

public:
    Mapper037(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        target_register_ = 0;
        prg_bank_mode_ = false;
        chr_inversion_ = false;
        std::memset(registers_, 0, sizeof(registers_));
        mirror_mode_ = Mirror::HORIZONTAL;
        selected_block_ = 0;
        wram_enabled_ = false;
        irq_.reset();
        update_banks();
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }
    void notify_a12(bool a12_high, uint64_t ppu_cycle) override { irq_.notify_a12(a12_high, ppu_cycle); }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_8k_banks(config, prg_rom_, prg_rom_size_, prg_bank_);
        // Write-protect $6000-$7FFF so block register writes
        // reach register_write() instead of going to PRG-RAM.
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = 8192;
        config.prg_ram_enabled = (prg_ram_ != nullptr);
        config.prg_ram_write_protected = true;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t total_chr_1k = mapper_helpers::chr_1k_count(chr_mem_size_);
        for (int i = 0; i < 8; i++) {
            uint32_t offset = (chr_bank_[i] % total_chr_1k) * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        // Block select register at $6000-$7FFF (guarded by WRAM enable)
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            if (wram_enabled_) {
                selected_block_ = data & 0x07;
                update_banks();
            }
            return true;
        }

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
            update_banks();
            return true;
        } else if (addr <= 0xBFFF) {
            if (even) {
                mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            } else {
                // $A001: WRAM enable/protect (bit 7 = enable)
                wram_enabled_ = (data & 0x80) != 0;
            }
            return true;
        } else {
            return irq_.write(addr, data);
        }
    }
};

} // namespace nes_system
