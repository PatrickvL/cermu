#pragma once
/*
 * mapper_189_mmc3_tfc.h — iNES Mapper 189 (TFC-AP001 / Thunder Warrior)
 *
 * MMC3 variant where PRG banking is overridden by a single 32KB bank
 * register at $4120-$5FFF.  CHR banking and scanline IRQ work exactly
 * like standard MMC3.
 *
 *   $4120-$5FFF write: 32KB PRG bank = (D7-D4) | (D3-D0)
 *   $8000-$9FFF: CHR bank select (MMC3 style), PRG mode ignored
 *   $A000-$BFFF: mirroring / PRG-RAM protect
 *   $C000-$DFFF: IRQ reload value / reload trigger
 *   $E000-$FFFF: IRQ disable (ack) / enable
 *
 * Games: Thunder Warrior, Street Fighter II (pirate).
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper189 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    // 32KB PRG outer bank (set via $4120)
    uint8_t prg_outer_bank_ = 0;

    // Standard MMC3 CHR
    uint8_t target_register_ = 0;
    bool chr_inversion_ = false;
    uint8_t registers_[8] = {};

    Mirror mirror_mode_ = Mirror::HORIZONTAL;

    // Composable MMC3 IRQ
    mapper_helpers::MMC3IRQ irq_;

    void update_chr_banks(uint32_t chr_bank[8]) const {
        uint32_t chr_size = chr_banks_ == 0 ? 8 : chr_banks_ * 8;
        if (!chr_inversion_) {
            chr_bank[0] = ((registers_[0] & 0xFE) + 0) % chr_size;
            chr_bank[1] = ((registers_[0] & 0xFE) + 1) % chr_size;
            chr_bank[2] = ((registers_[1] & 0xFE) + 0) % chr_size;
            chr_bank[3] = ((registers_[1] & 0xFE) + 1) % chr_size;
            chr_bank[4] = registers_[2] % chr_size;
            chr_bank[5] = registers_[3] % chr_size;
            chr_bank[6] = registers_[4] % chr_size;
            chr_bank[7] = registers_[5] % chr_size;
        } else {
            chr_bank[0] = registers_[2] % chr_size;
            chr_bank[1] = registers_[3] % chr_size;
            chr_bank[2] = registers_[4] % chr_size;
            chr_bank[3] = registers_[5] % chr_size;
            chr_bank[4] = ((registers_[0] & 0xFE) + 0) % chr_size;
            chr_bank[5] = ((registers_[0] & 0xFE) + 1) % chr_size;
            chr_bank[6] = ((registers_[1] & 0xFE) + 0) % chr_size;
            chr_bank[7] = ((registers_[1] & 0xFE) + 1) % chr_size;
        }
    }

public:
    Mapper189(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_outer_bank_ = 0;
        target_register_ = 0;
        chr_inversion_ = false;
        std::memset(registers_, 0, sizeof(registers_));
        mirror_mode_ = Mirror::HORIZONTAL;
        irq_.reset();
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }

    void notify_a12(bool a12_high, uint64_t ppu_cycle) override {
        irq_.notify_a12(a12_high, ppu_cycle);
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // 32KB bank selected by $4120 register — ignores MMC3 PRG registers
        mapper_helpers::set_prg_32k(config, prg_rom_, prg_rom_size_, prg_outer_bank_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_bank[8];
        update_chr_banks(chr_bank);
        for (int i = 0; i < 8; i++) {
            uint32_t offset = chr_bank[i] * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        // $4120-$5FFF: 32KB PRG bank register
        if (addr >= 0x4120 && addr < 0x6000) {
            prg_outer_bank_ = (data >> 4) | (data & 0x0F);
            return true;
        }

        if (addr < 0x8000) return false;

        bool even = !(addr & 0x0001);

        if (addr <= 0x9FFF) {
            if (even) {
                target_register_ = data & 0x07;
                // PRG mode bit ignored (PRG is 32KB from $4120)
                chr_inversion_ = (data & 0x80) != 0;
            } else {
                registers_[target_register_] = data;
            }
            return true;  // CHR banks changed
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
