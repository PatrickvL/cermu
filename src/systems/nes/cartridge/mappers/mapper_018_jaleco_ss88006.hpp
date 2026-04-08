#pragma once
/*
 * mapper_018_jaleco_ss88006.h — iNES Mapper 018 (Jaleco SS 88006)
 *
 * 3×8KB switchable PRG + 8KB fixed + 8×1KB CHR + CPU-cycle IRQ.
 * All bank registers are written as split nybbles (low then high).
 *
 * Register map (addr & $F003):
 *   $8000/$8001: PRG bank 0 lo/hi
 *   $8002/$8003: PRG bank 1 lo/hi
 *   $9000/$9001: PRG bank 2 lo/hi
 *   $A000-$B003: CHR banks 0-7 lo/hi (4 pairs)
 *   $C000-$D003: CHR banks 4-7 lo/hi (4 pairs) — wait, actually:
 *   $A000/$A001: CHR bank 0 lo/hi
 *   $A002/$A003: CHR bank 1 lo/hi
 *   $B000/$B001: CHR bank 2 lo/hi
 *   $B002/$B003: CHR bank 3 lo/hi
 *   $C000/$C001: CHR bank 4 lo/hi
 *   $C002/$C003: CHR bank 5 lo/hi
 *   $D000/$D001: CHR bank 6 lo/hi
 *   $D002/$D003: CHR bank 7 lo/hi
 *   $E000/$E001: IRQ reload lo/hi
 *   $E002/$E003: IRQ reload upper bits / control
 *   $F000: IRQ counter reload
 *   $F001: IRQ enable/disable + mask
 *   $F002: mirroring
 *
 * Games: Pizza Pop!, Plasma Ball, Ninja Jajamaru, Holy Diver, Moero Pro Yakyuu.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper018 : public Mapper {
private:

    uint8_t prg_bank_[3] = {};
    uint8_t chr_bank_[8] = {};
    Mirror mirror_mode_ = Mirror::HORIZONTAL;

    // IRQ: 16-bit countdown, configurable mask (4/8/12/16-bit width)
    uint16_t irq_reload_ = 0;
    uint16_t irq_counter_ = 0;
    uint16_t irq_mask_ = 0xFFFF;  // active bit mask
    bool irq_enabled_ = false;
    bool irq_active_ = false;

    void write_nybble_lo(uint8_t& reg, uint8_t data) {
        reg = (reg & 0xF0) | (data & 0x0F);
    }
    void write_nybble_hi(uint8_t& reg, uint8_t data) {
        reg = (reg & 0x0F) | ((data & 0x0F) << 4);
    }

public:
    Mapper018(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        for (int i = 0; i < 3; i++) prg_bank_[i] = 0;
        for (int i = 0; i < 8; i++) chr_bank_[i] = 0;
        mirror_mode_ = header_mirror_;
        irq_reload_ = 0;
        irq_counter_ = 0;
        irq_mask_ = 0xFFFF;
        irq_enabled_ = false;
        irq_active_ = false;
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_active_; }
    void irq_clear() override { irq_active_ = false; }

    void notify_cpu_cycle() override {
        if (!irq_enabled_) return;
        uint16_t masked = irq_counter_ & irq_mask_;
        if (masked == 0) {
            irq_active_ = true;
            irq_enabled_ = false;
        } else {
            irq_counter_ = (irq_counter_ & ~irq_mask_) | ((masked - 1) & irq_mask_);
        }
    }

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

        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_1k_pages(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        uint16_t reg = addr & 0xF003;
        switch (reg) {
            // PRG banks
            case 0x8000: write_nybble_lo(prg_bank_[0], data); return true;
            case 0x8001: write_nybble_hi(prg_bank_[0], data); return true;
            case 0x8002: write_nybble_lo(prg_bank_[1], data); return true;
            case 0x8003: write_nybble_hi(prg_bank_[1], data); return true;
            case 0x9000: write_nybble_lo(prg_bank_[2], data); return true;
            case 0x9001: write_nybble_hi(prg_bank_[2], data); return true;

            // CHR banks
            case 0xA000: write_nybble_lo(chr_bank_[0], data); return true;
            case 0xA001: write_nybble_hi(chr_bank_[0], data); return true;
            case 0xA002: write_nybble_lo(chr_bank_[1], data); return true;
            case 0xA003: write_nybble_hi(chr_bank_[1], data); return true;
            case 0xB000: write_nybble_lo(chr_bank_[2], data); return true;
            case 0xB001: write_nybble_hi(chr_bank_[2], data); return true;
            case 0xB002: write_nybble_lo(chr_bank_[3], data); return true;
            case 0xB003: write_nybble_hi(chr_bank_[3], data); return true;
            case 0xC000: write_nybble_lo(chr_bank_[4], data); return true;
            case 0xC001: write_nybble_hi(chr_bank_[4], data); return true;
            case 0xC002: write_nybble_lo(chr_bank_[5], data); return true;
            case 0xC003: write_nybble_hi(chr_bank_[5], data); return true;
            case 0xD000: write_nybble_lo(chr_bank_[6], data); return true;
            case 0xD001: write_nybble_hi(chr_bank_[6], data); return true;
            case 0xD002: write_nybble_lo(chr_bank_[7], data); return true;
            case 0xD003: write_nybble_hi(chr_bank_[7], data); return true;

            // IRQ reload
            case 0xE000:
                irq_reload_ = (irq_reload_ & 0xFFF0) | (data & 0x0F);
                return false;
            case 0xE001:
                irq_reload_ = (irq_reload_ & 0xFF0F) | ((data & 0x0F) << 4);
                return false;
            case 0xE002:
                irq_reload_ = (irq_reload_ & 0xF0FF) | ((data & 0x0F) << 8);
                return false;
            case 0xE003:
                irq_reload_ = (irq_reload_ & 0x0FFF) | ((data & 0x0F) << 12);
                return false;

            // IRQ control
            case 0xF000:
                irq_active_ = false;
                irq_counter_ = irq_reload_;
                return false;
            case 0xF001:
                irq_active_ = false;
                irq_enabled_ = (data & 0x01) != 0;
                // D1-D2 select counter size (D3+ unused)
                switch ((data >> 1) & 0x03) {
                    case 0: irq_mask_ = 0xFFFF; break;  // 16-bit
                    case 1: irq_mask_ = 0x0FFF; break;  // 12-bit
                    case 2: irq_mask_ = 0x00FF; break;  // 8-bit
                    case 3: irq_mask_ = 0x000F; break;  // 4-bit
                }
                return false;

            // Mirroring
            case 0xF002:
                mirror_mode_ = mapper_helpers::mirror_from_2bit(data);
                return true;

            // $9002-$9003, $F003: unused
        }
        return false;
    }
};

} // namespace nes_system
