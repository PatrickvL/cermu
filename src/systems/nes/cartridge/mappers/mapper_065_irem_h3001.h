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

#include "systems/nes/cartridge/nes_mapper.h"

namespace nes_system {

class Mapper065 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t prg_bank_[3] = {};
    uint8_t chr_bank_[8] = {};

    Mirror mirror_mode_ = Mirror::VERTICAL;

    // IRQ timer
    bool irq_enabled_ = false;
    bool irq_active_ = false;
    uint16_t irq_counter_ = 0;
    uint16_t irq_reload_ = 0;

public:
    Mapper065(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_[0] = 0;
        prg_bank_[1] = 1;
        prg_bank_[2] = static_cast<uint8_t>((prg_rom_size_ / 0x2000) > 1 ?
            (prg_rom_size_ / 0x2000) - 2 : 0);
        for (int i = 0; i < 8; i++) chr_bank_[i] = i;
        mirror_mode_ = header_mirror_;
        irq_enabled_ = false;
        irq_active_ = false;
        irq_counter_ = 0;
        irq_reload_ = 0;
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_active_; }
    void irq_clear() override { irq_active_ = false; }

    // A12 is repurposed as a "tick" for the CPU-cycle-based IRQ counter.
    // In a real integration the system would call this once per CPU cycle
    // when IRQ is enabled, or the bus tick could drive it. For now we rely
    // on the PPU A12 callback as a rough approximation.
    void notify_a12(bool a12_high, uint64_t /*ppu_cycle*/) override {
        if (!irq_enabled_ || !a12_high) return;
        if (irq_counter_ > 0) {
            irq_counter_--;
            if (irq_counter_ == 0) {
                irq_active_ = true;
            }
        }
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t total_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (total_8k == 0) total_8k = 1;

        for (int slot = 0; slot < 3; slot++) {
            uint32_t b = prg_bank_[slot] % total_8k;
            uint32_t base = b * 0x2000;
            config.prg_pages[slot * 2]     = prg_rom_ + base;
            config.prg_pages[slot * 2 + 1] = prg_rom_ + base + 0x1000;
        }
        // $E000-$FFFF: fixed last 8KB
        uint32_t last = (total_8k - 1) * 0x2000;
        config.prg_pages[6] = prg_rom_ + last;
        config.prg_pages[7] = prg_rom_ + last + 0x1000;

        config.prg_ram_enabled = false;
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
        if (addr < 0x8000) return false;

        switch (addr) {
            case 0x8000: prg_bank_[0] = data; return true;
            case 0xA000: prg_bank_[1] = data; return true;
            case 0xC000: prg_bank_[2] = data; return true;

            case 0x9001:
                mirror_mode_ = (data & 0x80) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                return true;

            case 0x9003:
                irq_enabled_ = (data & 0x80) != 0;
                irq_active_ = false;
                return false;
            case 0x9004:
                irq_counter_ = irq_reload_;
                irq_active_ = false;
                return false;
            case 0x9005:
                irq_reload_ = (irq_reload_ & 0x00FF) | (static_cast<uint16_t>(data) << 8);
                return false;
            case 0x9006:
                irq_reload_ = (irq_reload_ & 0xFF00) | data;
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
