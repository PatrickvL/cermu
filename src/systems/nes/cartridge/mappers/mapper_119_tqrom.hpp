#pragma once
/*
 * mapper_119_tqrom.h — iNES Mapper 119 (TQROM / MMC3 variant)
 *
 * Like standard MMC3 but with both CHR-ROM and CHR-RAM.
 * Bit 6 of the CHR bank register selects between ROM (0) and RAM (1).
 * 8KB CHR-RAM is available for bank values with bit 6 set.
 *
 * Games: High Speed, Pin-Bot, Rad Racer II (unreleased)
 *
 * CHR bank interpretation:
 *   D5-D0: CHR-ROM bank (when D6 = 0)
 *   D1-D0: CHR-RAM bank (when D6 = 1), addresses 8KB CHR-RAM as 1KB pages
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper119 : public Mapper {
private:

    uint8_t target_register_ = 0;
    bool prg_bank_mode_ = false;
    bool chr_inversion_ = false;
    uint8_t registers_[8] = {};

    bool prg_ram_enabled_ = true;
    bool prg_ram_write_protect_ = false;

    Mirror mirror_mode_ = Mirror::HORIZONTAL;

    mapper_helpers::MMC3IRQ irq_;

    uint32_t prg_bank_[4] = {};

    // TQROM boards have 8KB CHR-RAM alongside CHR-ROM.  The
    // CHR-RAM lives at chr_mem_ + original_chr_rom_size_, appended
    // by Cartridge via extra_chr_ram_size().
    static constexpr uint32_t CHR_RAM_SIZE = 8192;
    uint32_t original_chr_rom_size_ = 0;

    void update_prg_banks() {
        uint32_t total_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (total_8k == 0) total_8k = 1;
        uint32_t last_bank = total_8k - 1;
        if (!prg_bank_mode_) {
            prg_bank_[0] = (registers_[6] & 0x3F) % total_8k;
            prg_bank_[1] = (registers_[7] & 0x3F) % total_8k;
            prg_bank_[2] = (total_8k >= 2) ? total_8k - 2 : 0;
            prg_bank_[3] = last_bank;
        } else {
            prg_bank_[0] = (total_8k >= 2) ? total_8k - 2 : 0;
            prg_bank_[1] = (registers_[7] & 0x3F) % total_8k;
            prg_bank_[2] = (registers_[6] & 0x3F) % total_8k;
            prg_bank_[3] = last_bank;
        }
    }

public:
    Mapper119(uint8_t /*prgBanks*/, uint8_t chrBanks)
        : original_chr_rom_size_(chrBanks * 8192u) {}

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }
    void notify_a12(bool a12_high, uint64_t ppu_cycle) override { irq_.notify_a12(a12_high, ppu_cycle); }

    uint32_t extra_chr_ram_size() const override { return CHR_RAM_SIZE; }

    void reset() override {
        target_register_ = 0;
        prg_bank_mode_ = false;
        chr_inversion_ = false;
        std::memset(registers_, 0, sizeof(registers_));
        prg_ram_enabled_ = true;
        prg_ram_write_protect_ = false;
        irq_.reset();
        mirror_mode_ = Mirror::HORIZONTAL;
        update_prg_banks();
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        for (int slot = 0; slot < 4; slot++) {
            uint32_t bank_base = prg_bank_[slot] * 0x2000;
            for (int half = 0; half < 2; half++) {
                uint32_t offset = bank_base + half * 0x1000;
                config.prg_pages[slot * 2 + half] =
                    (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
            }
        }
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = prg_ram_enabled_ && (prg_ram_ != nullptr);
        config.prg_ram_write_protected = prg_ram_write_protect_;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // Build the CHR bank assignment considering chr_inversion_
        uint8_t slot_regs[8];
        if (!chr_inversion_) {
            slot_regs[0] = registers_[0] & 0xFE;       // 2KB at $0000
            slot_regs[1] = (registers_[0] & 0xFE) | 1;
            slot_regs[2] = registers_[1] & 0xFE;       // 2KB at $0800
            slot_regs[3] = (registers_[1] & 0xFE) | 1;
            slot_regs[4] = registers_[2];               // 1KB at $1000
            slot_regs[5] = registers_[3];
            slot_regs[6] = registers_[4];
            slot_regs[7] = registers_[5];
        } else {
            slot_regs[0] = registers_[2];
            slot_regs[1] = registers_[3];
            slot_regs[2] = registers_[4];
            slot_regs[3] = registers_[5];
            slot_regs[4] = registers_[0] & 0xFE;
            slot_regs[5] = (registers_[0] & 0xFE) | 1;
            slot_regs[6] = registers_[1] & 0xFE;
            slot_regs[7] = (registers_[1] & 0xFE) | 1;
        }

        uint32_t chr_rom_1k = (original_chr_rom_size_ > 0) ? static_cast<uint32_t>(original_chr_rom_size_ / 0x0400) : 1;

        for (int i = 0; i < 8; i++) {
            bool use_ram = (slot_regs[i] & 0x40) != 0;
            if (use_ram) {
                // CHR-RAM: D2-D0 select 1KB page within 8KB RAM
                // CHR-RAM is appended after CHR-ROM in chr_mem_.
                uint32_t ram_page = slot_regs[i] & 0x07;
                uint32_t offset = original_chr_rom_size_ + (ram_page * 0x0400) % CHR_RAM_SIZE;
                config.chr_pages[i] = chr_mem_ + offset;
                config.chr_writable[i] = true;
            } else {
                // CHR-ROM: D5-D0 select 1KB page
                uint32_t bank = (slot_regs[i] & 0x3F) % chr_rom_1k;
                config.chr_pages[i] = chr_mem_ + (bank * 0x0400);
                config.chr_writable[i] = false;
            }
        }
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
            }
            update_prg_banks();
            return true;
        } else if (addr <= 0xBFFF) {
            if (even) {
                mirror_mode_ = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            } else {
                prg_ram_enabled_ = (data & 0x80) != 0;
                prg_ram_write_protect_ = (data & 0x40) != 0;
            }
            return true;
        } else {
            irq_.write(addr, data);
            return false;
        }
    }
};

} // namespace nes_system
