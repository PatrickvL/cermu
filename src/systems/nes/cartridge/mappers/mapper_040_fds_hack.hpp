#pragma once
/*
 * mapper_040_fds_hack.hpp — iNES Mapper 040 (SMB2J FDS conversion hack)
 *
 * Bootleg FDS-to-cartridge conversion of Super Mario Bros. 2 (Japan).
 *
 * PRG layout (hardcoded):
 *   $6000-$7FFF: switchable 8KB bank (register at $8000-$9FFF)
 *   $8000-$9FFF: fixed bank 6
 *   $A000-$BFFF: fixed bank 4
 *   $C000-$DFFF: fixed bank 5
 *   $E000-$FFFF: fixed bank 7 (vectors)
 *
 * IRQ: 12-bit CPU-cycle countdown, writes to $A000-$BFFF disable+ack,
 *      writes to $C000-$DFFF enable and reload counter to 4096.
 *
 * CHR: 8KB fixed CHR-ROM (no banking).
 *
 * Games: Super Mario Bros. 2 (J) bootleg cart.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper040 : public Mapper {
private:
    uint8_t prg_bank_ = 0;   // switchable bank at $6000
    uint16_t irq_counter_ = 0;
    bool irq_enabled_ = false;
    bool irq_active_ = false;

public:
    Mapper040(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        prg_bank_ = 0;
        irq_counter_ = 0;
        irq_enabled_ = false;
        irq_active_ = false;
    }

    Mirror mirror() override { return header_mirror_; }
    bool irq_state() override { return irq_active_; }
    void irq_clear() override { irq_active_ = false; }

    void notify_cpu_cycle() override {
        if (!irq_enabled_) return;
        if (irq_counter_ > 0) {
            irq_counter_--;
        }
        if (irq_counter_ == 0) {
            irq_active_ = true;
            irq_enabled_ = false;
        }
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        // $6000: switchable bank (exposed as PRG-RAM window, but we put PRG-ROM there)
        config.prg_ram_enabled = true;
        config.prg_ram_base = const_cast<uint8_t*>(prg_rom_ + (static_cast<uint32_t>(prg_bank_ & 0x07) % n) * 0x2000);
        config.prg_ram_size = 0x2000;

        // $8000: bank 6, $A000: bank 4, $C000: bank 5, $E000: bank 7
        uint32_t banks[4] = {
            6u % n, 4u % n, 5u % n, 7u % n
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // Fixed 8KB CHR
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000 && addr <= 0x9FFF) {
            // Switchable 8KB bank at $6000
            prg_bank_ = data & 0x07;
            return true;
        }
        if (addr >= 0xA000 && addr <= 0xBFFF) {
            // IRQ disable + acknowledge
            irq_enabled_ = false;
            irq_active_ = false;
            return false;
        }
        if (addr >= 0xC000 && addr <= 0xDFFF) {
            // IRQ enable + reload counter to 4096
            irq_enabled_ = true;
            irq_counter_ = 4096;
            return false;
        }
        return false;
    }
};

} // namespace nes_system
