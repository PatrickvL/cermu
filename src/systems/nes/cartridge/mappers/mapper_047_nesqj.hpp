#pragma once
/*
 * mapper_047_nesqj.h — iNES Mapper 047 (NES-QJ — MMC3 multicart)
 *
 * Standard MMC3 wrapped in a 2-game outer bank register.
 * Writing to $6000-$7FFF selects the outer PRG/CHR bank (bit 0).
 *
 * Block 0: first 128K PRG + first 128K CHR
 * Block 1: next 128K PRG + next 128K CHR
 *
 * Games: Super Spike V'Ball + Nintendo World Cup (2-in-1 cart).
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper047 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    // Inner MMC3 state
    uint8_t target_register_ = 0;
    bool prg_bank_mode_ = false;
    bool chr_inversion_ = false;
    uint8_t registers_[8] = {};
    Mirror mirror_mode_ = Mirror::HORIZONTAL;

    // Outer bank
    uint8_t outer_bank_ = 0;

    // IRQ (reuses MMC3IRQ composable)
    mapper_helpers::MMC3IRQ irq_;

    // Derived banks
    uint32_t prg_bank_[4] = {};
    uint32_t chr_bank_[8] = {};

    void update_banks() {
        // Outer bank shift: each block is 128KB PRG (16×8KB) + 128KB CHR (128×1KB)
        uint32_t prg_outer = (outer_bank_ & 0x01) * 16;
        uint32_t chr_outer = (outer_bank_ & 0x01) * 128;

        uint32_t total_prg_8k = mapper_helpers::prg_8k_count(prg_rom_size_);
        uint32_t total_chr_1k = mapper_helpers::chr_1k_count(chr_mem_size_);
        uint32_t last_in_block = (prg_outer + 15 < total_prg_8k) ? prg_outer + 15 : total_prg_8k - 1;
        uint32_t second_last = (last_in_block > 0) ? last_in_block - 1 : 0;

        uint32_t r6 = prg_outer + ((registers_[6] & 0x0F) % 16);
        uint32_t r7 = prg_outer + ((registers_[7] & 0x0F) % 16);
        if (r6 >= total_prg_8k) r6 = total_prg_8k - 1;
        if (r7 >= total_prg_8k) r7 = total_prg_8k - 1;

        if (!prg_bank_mode_) {
            prg_bank_[0] = r6; prg_bank_[1] = r7;
            prg_bank_[2] = second_last; prg_bank_[3] = last_in_block;
        } else {
            prg_bank_[0] = second_last; prg_bank_[1] = r7;
            prg_bank_[2] = r6; prg_bank_[3] = last_in_block;
        }

        auto chr_in_block = [&](uint8_t val) -> uint32_t {
            return (chr_outer + (val % 128)) % total_chr_1k;
        };

        if (!chr_inversion_) {
            chr_bank_[0] = chr_in_block(registers_[0] & 0xFE);
            chr_bank_[1] = chr_in_block((registers_[0] & 0xFE) + 1);
            chr_bank_[2] = chr_in_block(registers_[1] & 0xFE);
            chr_bank_[3] = chr_in_block((registers_[1] & 0xFE) + 1);
            for (int i = 0; i < 4; i++)
                chr_bank_[4 + i] = chr_in_block(registers_[2 + i]);
        } else {
            for (int i = 0; i < 4; i++)
                chr_bank_[i] = chr_in_block(registers_[2 + i]);
            chr_bank_[4] = chr_in_block(registers_[0] & 0xFE);
            chr_bank_[5] = chr_in_block((registers_[0] & 0xFE) + 1);
            chr_bank_[6] = chr_in_block(registers_[1] & 0xFE);
            chr_bank_[7] = chr_in_block((registers_[1] & 0xFE) + 1);
        }
    }

public:
    Mapper047(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        target_register_ = 0;
        prg_bank_mode_ = false;
        chr_inversion_ = false;
        std::memset(registers_, 0, sizeof(registers_));
        mirror_mode_ = Mirror::HORIZONTAL;
        outer_bank_ = 0;
        irq_.reset();
        update_banks();
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }
    void notify_a12(bool a12_high, uint64_t ppu_cycle) override { irq_.notify_a12(a12_high, ppu_cycle); }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_8k_banks(config, prg_rom_, prg_rom_size_, prg_bank_);
        config.prg_ram_enabled = false;
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
        // Outer bank register at $6000-$7FFF
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            outer_bank_ = data & 0x01;
            update_banks();
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
                return true;
            }
            return false;
        } else {
            return irq_.write(addr, data);
        }
    }
};

} // namespace nes_system
