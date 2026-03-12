#pragma once
/*
 * mapper_069_sunsoft_fme7.h — iNES Mapper 069 (Sunsoft FME-7 / 5A / 5B)
 *
 * Versatile mapper with command/parameter register interface.
 * 4 × 8KB switchable PRG (one can also map RAM), 8 × 1KB CHR.
 * 16-bit CPU-cycle IRQ counter.
 * FME-7 is the base; 5A/5B add Yamaha expansion audio (not emulated here).
 * Games: Batman: Return of the Joker, Gimmick!, Hebereke.
 *
 * $8000: Command register (selects target for $A000 write)
 * $A000: Parameter register (data for the selected command)
 *
 * Commands 0-7: CHR 1KB bank select
 * Command 8: PRG bank at $6000 (ROM or RAM select)
 * Commands 9-B: PRG 8KB banks at $8000/$A000/$C000
 * Command C: Mirroring
 * Command D: IRQ control
 * Command E: IRQ counter low byte
 * Command F: IRQ counter high byte
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

namespace nes_system {

class Mapper069 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t command_ = 0;
    uint8_t prg_bank_[4] = {};  // 0 = $6000, 1 = $8000, 2 = $A000, 3 = $C000
    uint8_t chr_bank_[8] = {};
    bool prg_ram_at_6000_ = false;  // Bit 6 of command 8
    bool prg_ram_enabled_ = false;  // Bit 7 of command 8

    Mirror mirror_mode_ = Mirror::VERTICAL;

    bool irq_enabled_ = false;
    bool irq_counter_enabled_ = false;
    bool irq_active_ = false;
    uint16_t irq_counter_ = 0;

public:
    Mapper069(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        command_ = 0;
        prg_bank_[0] = 0;
        prg_bank_[1] = 0;
        prg_bank_[2] = 1;
        prg_bank_[3] = static_cast<uint8_t>((prg_rom_size_ / 0x2000) > 1 ?
            (prg_rom_size_ / 0x2000) - 2 : 0);
        for (int i = 0; i < 8; i++) chr_bank_[i] = i;
        prg_ram_at_6000_ = false;
        prg_ram_enabled_ = false;
        mirror_mode_ = header_mirror_;
        irq_enabled_ = false;
        irq_counter_enabled_ = false;
        irq_active_ = false;
        irq_counter_ = 0;
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_active_; }
    void irq_clear() override { irq_active_ = false; }

    // FME-7 IRQ is a CPU-cycle counter; we approximate via A12 notifications
    void notify_a12(bool a12_high, uint64_t /*ppu_cycle*/) override {
        if (!a12_high) return;
        if (irq_counter_enabled_) {
            irq_counter_--;
            if (irq_counter_ == 0xFFFF && irq_enabled_) {
                irq_active_ = true;
            }
        }
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t total_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (total_8k == 0) total_8k = 1;

        // $8000, $A000, $C000: switchable ROM banks
        for (int slot = 0; slot < 3; slot++) {
            uint32_t b = prg_bank_[slot + 1] % total_8k;
            uint32_t base = b * 0x2000;
            config.prg_pages[slot * 2]     = prg_rom_ + base;
            config.prg_pages[slot * 2 + 1] = prg_rom_ + base + 0x1000;
        }

        // $E000-$FFFF: fixed last 8KB
        uint32_t last = (total_8k - 1) * 0x2000;
        config.prg_pages[6] = prg_rom_ + last;
        config.prg_pages[7] = prg_rom_ + last + 0x1000;

        // $6000-$7FFF: RAM or ROM depending on command 8
        if (prg_ram_at_6000_ && prg_ram_ && prg_ram_size_ > 0) {
            config.prg_ram_base = prg_ram_;
            config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
            config.prg_ram_enabled = prg_ram_enabled_;
            config.prg_ram_write_protected = !prg_ram_enabled_;
        } else {
            config.prg_ram_enabled = false;
        }
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_1k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;
        if (chr_1k == 0) chr_1k = 1;

        for (int i = 0; i < 8; i++) {
            uint32_t b = chr_bank_[i] % chr_1k;
            uint32_t offset = b * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000 && addr <= 0x9FFF) {
            command_ = data & 0x0F;
            return false;
        }

        if (addr >= 0xA000 && addr <= 0xBFFF) {
            switch (command_) {
                case 0x00: case 0x01: case 0x02: case 0x03:
                case 0x04: case 0x05: case 0x06: case 0x07:
                    chr_bank_[command_] = data;
                    return true;
                case 0x08:
                    prg_ram_at_6000_ = (data & 0x40) != 0;
                    prg_ram_enabled_ = (data & 0x80) != 0;
                    prg_bank_[0] = data & 0x3F;
                    return true;
                case 0x09: prg_bank_[1] = data & 0x3F; return true;
                case 0x0A: prg_bank_[2] = data & 0x3F; return true;
                case 0x0B: prg_bank_[3] = data & 0x3F; return true;
                case 0x0C:
                    switch (data & 0x03) {
                        case 0: mirror_mode_ = Mirror::VERTICAL;     break;
                        case 1: mirror_mode_ = Mirror::HORIZONTAL;   break;
                        case 2: mirror_mode_ = Mirror::ONESCREEN_LO; break;
                        case 3: mirror_mode_ = Mirror::ONESCREEN_HI; break;
                    }
                    return true;
                case 0x0D:
                    irq_enabled_ = (data & 0x01) != 0;
                    irq_counter_enabled_ = (data & 0x80) != 0;
                    irq_active_ = false;
                    return false;
                case 0x0E:
                    irq_counter_ = (irq_counter_ & 0xFF00) | data;
                    return false;
                case 0x0F:
                    irq_counter_ = (irq_counter_ & 0x00FF) | (static_cast<uint16_t>(data) << 8);
                    return false;
            }
        }
        return false;
    }
};

} // namespace nes_system
