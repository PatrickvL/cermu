#pragma once
/*
 * mapper_073_vrc3.h — iNES Mapper 073 (Konami VRC3)
 *
 * 16KB switchable PRG at $8000 + 16KB fixed at $C000.
 * No CHR banking (uses CHR-RAM).
 * 16-bit IRQ counter with optional 8-bit mode.
 *
 * Games: Salamander (J)
 *
 * Registers:
 *   $8000: D3-D0 = IRQ latch bits 0-3
 *   $9000: D3-D0 = IRQ latch bits 4-7
 *   $A000: D3-D0 = IRQ latch bits 8-11
 *   $B000: D3-D0 = IRQ latch bits 12-15
 *   $C000: D2 = IRQ mode (0=16-bit, 1=8-bit), D1 = IRQ enable after ack, D0 = IRQ enable
 *   $D000: IRQ acknowledge (write any value)
 *   $F000: D3-D0 = 16KB PRG bank at $8000
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper073 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t prg_bank_select_ = 0;

    // IRQ
    uint16_t irq_latch_ = 0;
    uint16_t irq_counter_ = 0;
    bool irq_enabled_ = false;
    bool irq_enable_after_ack_ = false;
    bool irq_mode_8bit_ = false;
    bool irq_active_ = false;
    uint16_t irq_prescaler_ = 0;

    static constexpr uint16_t IRQ_PRESCALER_RELOAD = 341;  // ~CPU cycles per scanline

public:
    Mapper073(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        irq_latch_ = 0;
        irq_counter_ = 0;
        irq_enabled_ = false;
        irq_enable_after_ack_ = false;
        irq_mode_8bit_ = false;
        irq_active_ = false;
        irq_prescaler_ = 0;
    }

    bool irq_state() override { return irq_active_; }
    void irq_clear() override { irq_active_ = false; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_select_);
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, true);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xF000) {
            case 0x8000:
                irq_latch_ = (irq_latch_ & 0xFFF0) | (data & 0x0F);
                return false;
            case 0x9000:
                irq_latch_ = (irq_latch_ & 0xFF0F) | ((data & 0x0F) << 4);
                return false;
            case 0xA000:
                irq_latch_ = (irq_latch_ & 0xF0FF) | ((data & 0x0F) << 8);
                return false;
            case 0xB000:
                irq_latch_ = (irq_latch_ & 0x0FFF) | ((data & 0x0F) << 12);
                return false;

            case 0xC000:
                irq_active_ = false;
                irq_mode_8bit_ = (data & 0x04) != 0;
                irq_enable_after_ack_ = (data & 0x02) != 0;
                irq_enabled_ = (data & 0x01) != 0;
                if (irq_enabled_) {
                    irq_counter_ = irq_latch_;
                    irq_prescaler_ = IRQ_PRESCALER_RELOAD;
                }
                return false;

            case 0xD000:
                irq_active_ = false;
                irq_enabled_ = irq_enable_after_ack_;
                return false;

            case 0xF000:
                prg_bank_select_ = data & 0x07;
                return true;

            default:
                return false;
        }
    }
};

} // namespace nes_system
