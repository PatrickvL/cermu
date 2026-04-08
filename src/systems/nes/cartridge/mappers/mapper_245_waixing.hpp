#pragma once
/*
 * mapper_245_waixing.h — iNES Mapper 245 (Waixing MMC3 variant)
 *
 * MMC3 variant for Chinese pirate boards with large PRG-ROM.
 * R0 bit 0 (from the CHR bank register) provides an extra PRG address
 * bit (typically A18), allowing up to 512KB PRG.  CHR is always RAM.
 *
 * All other behavior (PRG/CHR banking pattern, mirroring, IRQ) is
 * standard MMC3.
 *
 * Games: Yong Zhe Dou E Long (DQ7), Sheng Huo Lie Zhuan (Final Fantasy).
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper245 : public Mapper {
private:

    uint8_t target_register_ = 0;
    bool prg_bank_mode_ = false;
    bool chr_inversion_ = false;
    uint8_t registers_[8] = {};

    Mirror mirror_mode_ = Mirror::HORIZONTAL;

    // Composable MMC3 IRQ
    mapper_helpers::MMC3IRQ irq_;

    // Extra PRG bit from R0.D0 (latched when R0 is written)
    uint8_t prg_extra_bit_ = 0;

    void compute_prg_banks(uint32_t bank[4]) const {
        uint32_t total_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (total_8k == 0) total_8k = 1;

        // R0.D0 provides bit 5 of the 8KB bank number (A18 in address space)
        uint32_t extra = static_cast<uint32_t>(prg_extra_bit_) << 5;

        uint32_t b6 = (extra | (registers_[6] & 0x1F)) % total_8k;
        uint32_t b7 = (extra | (registers_[7] & 0x1F)) % total_8k;
        uint32_t fixed_lo = (extra | 0x1E) % total_8k;
        uint32_t fixed_hi = (extra | 0x1F) % total_8k;

        if (!prg_bank_mode_) {
            bank[0] = b6; bank[1] = b7; bank[2] = fixed_lo; bank[3] = fixed_hi;
        } else {
            bank[0] = fixed_lo; bank[1] = b7; bank[2] = b6; bank[3] = fixed_hi;
        }
    }

public:
    Mapper245(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        target_register_ = 0;
        prg_bank_mode_ = false;
        chr_inversion_ = false;
        std::memset(registers_, 0, sizeof(registers_));
        mirror_mode_ = Mirror::HORIZONTAL;
        irq_.reset();
        prg_extra_bit_ = 0;
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }

    void notify_a12(bool a12_high, uint64_t ppu_cycle) override {
        irq_.notify_a12(a12_high, ppu_cycle);
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t bank[4];
        compute_prg_banks(bank);
        for (int slot = 0; slot < 4; slot++) {
            uint32_t base = bank[slot] * 0x2000;
            for (int h = 0; h < 2; h++) {
                uint32_t off = base + h * 0x1000;
                config.prg_pages[slot * 2 + h] =
                    (off < prg_rom_size_) ? prg_rom_ + off : nullptr;
            }
        }
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // CHR is always RAM on mapper 245 boards — 8KB fixed
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, true);
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
                // Latch R0.D0 as extra PRG bit whenever R0 is written
                if (target_register_ == 0) {
                    prg_extra_bit_ = data & 0x01;
                }
            }
            return true;
        } else if (addr <= 0xBFFF) {
            if (even) {
                mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            }
            return true;
        } else {
            return irq_.write(addr, data);
        }
    }
};

} // namespace nes_system
