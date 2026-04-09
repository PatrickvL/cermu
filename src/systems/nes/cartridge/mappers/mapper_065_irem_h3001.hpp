#pragma once
/*
 * mapper_065_irem_h3001.h — iNES Mapper 065 (Irem H3001)
 *
 * 8KB switchable PRG banks (3 switchable + 1 fixed) + 8×1KB CHR banks.
 * Includes a programmable IRQ timer (16-bit countdown).
 * Games: Kaiketsu Yanchamaru 3, Spartan X 2, Daiku no Gen-san.
 *
 * PRG layout: $8000 (R0), $A000 (R1), $C000 (R2), $E000 (fixed last).
 * CHR: 8 individual 1KB registers at $B000-$B007.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper065 : public Mapper {
private:
    uint8_t prg_bank_[3] = {};
    uint8_t chr_bank_[8] = {};

    Mirror mirror_mode_ = Mirror::VERTICAL;

    // IRQ timer — 16-bit CPU-cycle countdown, fires at zero
    mapper_helpers::CPUCycleIRQ<mapper_helpers::IRQFireCondition::ON_ZERO> irq_;

public:
    Mapper065(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        prg_bank_[0] = 0;
        prg_bank_[1] = 1;
        prg_bank_[2] = static_cast<uint8_t>((prg_rom_size_ / 0x2000) > 1 ?
            (prg_rom_size_ / 0x2000) - 2 : 0);
        for (int i = 0; i < 8; i++) chr_bank_[i] = i;
        mirror_mode_ = header_mirror_;
        irq_.reset();
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }

    // H3001 IRQ is a 16-bit CPU-cycle countdown counter.
    void notify_cpu_cycle() override { irq_.tick(); }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(prg_bank_[0]) % n,
            static_cast<uint32_t>(prg_bank_[1]) % n,
            static_cast<uint32_t>(prg_bank_[2]) % n,
            n - 1
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_1k_pages(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr) {
            case 0x8000: prg_bank_[0] = data; return true;
            case 0xA000: prg_bank_[1] = data; return true;
            case 0xC000: prg_bank_[2] = data; return true;

            case 0x9001:
                mirror_mode_ = (data & 0x80) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                return true;

            case 0x9003:
                irq_.enabled = (data & 0x80) != 0;
                irq_.active = false;
                return false;
            case 0x9004:
                irq_.counter = irq_.reload;
                irq_.active = false;
                return false;
            case 0x9005:
                irq_.reload = (irq_.reload & 0x00FF) | (static_cast<uint16_t>(data) << 8);
                return false;
            case 0x9006:
                irq_.reload = (irq_.reload & 0xFF00) | data;
                return false;

            case 0xB000: chr_bank_[0] = data; return true;
            case 0xB001: chr_bank_[1] = data; return true;
            case 0xB002: chr_bank_[2] = data; return true;
            case 0xB003: chr_bank_[3] = data; return true;
            case 0xB004: chr_bank_[4] = data; return true;
            case 0xB005: chr_bank_[5] = data; return true;
            case 0xB006: chr_bank_[6] = data; return true;
            case 0xB007: chr_bank_[7] = data; return true;
        }
        return false;
    }
};

} // namespace nes_system
