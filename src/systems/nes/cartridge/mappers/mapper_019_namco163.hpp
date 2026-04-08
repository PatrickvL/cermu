#pragma once
/*
 * mapper_019_namco163.h — iNES Mapper 019 (Namco 163 / Namco 129)
 *
 * Complex banking + wavetable expansion audio.
 *
 * PRG: 3×8KB switchable ($8000,$A000,$C000) + 8KB fixed ($E000).
 *      PRG-RAM at $6000-$7FFF.
 * CHR: 8×1KB banks. Banks $E0-$FF map to internal CIRAM (nametable RAM)
 *      when CHR value >= $E0, enabling on-cart nametable mapping.
 * IRQ: 15-bit CPU-cycle up-counter, fires when bit 15 set.
 *
 * Expansion audio: 8-channel wavetable synthesizer with shared 128-byte
 * internal RAM.  Audio is complex — stubbed for initial mapper support.
 *
 * Register map:
 *   $4800: data port (read/write internal RAM, auto-increment)
 *   $5000-$5FFF: IRQ counter low / high
 *   $8000-$BFFF: CHR bank select (R0-R7)
 *   $C000-$C7FF: CHR bank for NT slot 0
 *   $C800-$CFFF: CHR bank for NT slot 1
 *   $D000-$D7FF: CHR bank for NT slot 2
 *   $D800-$DFFF: CHR bank for NT slot 3
 *   $E000: PRG bank 0 + sound enable
 *   $E800: PRG bank 1 + CHR-ROM/RAM mode
 *   $F000: PRG bank 2
 *   $F800: write protect + auto-increment
 *
 * Games: Rolling Thunder, King of Kings, Youkai Douchuki, Megami Tensei II,
 *        Sangokushi, Final Lap, Famista '90, Splatter House, etc.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper019 : public Mapper {
private:

    // PRG bank registers
    uint8_t prg_bank_[3] = {};

    // CHR bank registers (8 pattern + 4 nametable)
    uint8_t chr_bank_[12] = {};  // [0-7] = pattern, [8-11] = nametable

    // Internal 128-byte RAM (shared between audio wavetable + scratch)
    // TODO: Implement Namco 163 wavetable audio synthesis for audio_tick()/audio_output()
    uint8_t internal_ram_[128] = {};
    uint8_t ram_addr_ = 0;
    bool auto_increment_ = false;

    // CHR-ROM/RAM mode flags (from $E800 write)
    bool chr_lo_ram_ = false;  // true = banks 0-3 use CHR-RAM
    bool chr_hi_ram_ = false;  // true = banks 4-7 use CHR-RAM

    // IRQ: 15-bit up-counter
    uint16_t irq_counter_ = 0;
    bool irq_enabled_ = false;
    bool irq_active_ = false;

    // Sound enable
    bool sound_enabled_ = false;

    // Nametable CIRAM pointer (set by set_ciram)
    uint8_t* ciram_ = nullptr;

    bool is_ciram_bank(uint8_t bank) const {
        return bank >= 0xE0;
    }

public:
    Mapper019(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        std::memset(prg_bank_, 0, sizeof(prg_bank_));
        std::memset(chr_bank_, 0, sizeof(chr_bank_));
        std::memset(internal_ram_, 0, sizeof(internal_ram_));
        ram_addr_ = 0;
        auto_increment_ = false;
        chr_lo_ram_ = false;
        chr_hi_ram_ = false;
        irq_counter_ = 0;
        irq_enabled_ = false;
        irq_active_ = false;
        sound_enabled_ = false;
    }

    void set_ciram(uint8_t* ciram) override { ciram_ = ciram; }

    Mirror mirror() override {
        // Mirroring controlled by NT bank registers — handled in get_chr_bank_config
        return header_mirror_;
    }

    bool irq_state() override { return irq_active_; }
    void irq_clear() override { irq_active_ = false; }

    void notify_cpu_cycle() override {
        if (!irq_enabled_) return;
        if (irq_counter_ < 0x7FFF) {
            irq_counter_++;
            if (irq_counter_ >= 0x7FFF) {
                irq_active_ = true;
            }
        }
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(prg_bank_[0] & 0x3F) % n,
            static_cast<uint32_t>(prg_bank_[1] & 0x3F) % n,
            static_cast<uint32_t>(prg_bank_[2] & 0x3F) % n,
            n - 1
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);

        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t num_1k = mapper_helpers::chr_1k_count(chr_mem_size_);

        // Pattern table: $0000-$1FFF (8 × 1KB)
        for (int i = 0; i < 8; i++) {
            uint32_t bank = chr_bank_[i];
            if (is_ciram_bank(static_cast<uint8_t>(bank)) && ciram_) {
                config.chr_pages[i] = ciram_ + ((bank & 0x01) * 0x0400);
                config.chr_writable[i] = true;
            } else {
                uint32_t offset = (bank % num_1k) * 0x0400;
                config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                // Lower half uses chr_lo_ram_, upper half uses chr_hi_ram_
                config.chr_writable[i] = (i < 4) ? chr_lo_ram_ : chr_hi_ram_;
            }
        }

        // Nametable mappings from chr_bank_[8-11]
        for (int i = 0; i < 4; i++) {
            uint8_t bank = chr_bank_[8 + i];
            if (is_ciram_bank(bank)) {
                config.nt_page[i] = bank & 0x01;
                config.nt_ptr[i] = nullptr;
            } else if (chr_mem_size_ > 0) {
                // Map CHR-ROM into nametable slot
                uint32_t offset = (bank % num_1k) * 0x0400;
                config.nt_ptr[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                config.nt_page[i] = 0;
            }
        }
    }

    uint8_t expansion_read(uint16_t addr, bool& handled) override {
        if (addr == 0x4800) {
            handled = true;
            uint8_t val = internal_ram_[ram_addr_ & 0x7F];
            if (auto_increment_) ram_addr_ = (ram_addr_ + 1) & 0x7F;
            return val;
        }
        if ((addr & 0xF800) == 0x5000) {
            handled = true;
            return static_cast<uint8_t>(irq_counter_ & 0xFF);
        }
        if ((addr & 0xF800) == 0x5800) {
            handled = true;
            return static_cast<uint8_t>((irq_counter_ >> 8) & 0x7F) |
                   (irq_enabled_ ? 0x80 : 0x00);
        }
        handled = false;
        return 0;
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        // Internal RAM data port
        if (addr == 0x4800) {
            internal_ram_[ram_addr_ & 0x7F] = data;
            if (auto_increment_) ram_addr_ = (ram_addr_ + 1) & 0x7F;
            return false;
        }

        // IRQ registers
        if ((addr & 0xF800) == 0x5000) {
            irq_counter_ = (irq_counter_ & 0xFF00) | data;
            irq_active_ = false;
            return false;
        }
        if ((addr & 0xF800) == 0x5800) {
            irq_counter_ = (irq_counter_ & 0x00FF) | (static_cast<uint16_t>(data & 0x7F) << 8);
            irq_enabled_ = (data & 0x80) != 0;
            irq_active_ = false;
            return false;
        }

        if (addr < 0x8000) return false;

        // CHR bank registers
        uint16_t reg = addr & 0xF800;
        switch (reg) {
            case 0x8000: chr_bank_[0]  = data; return true;
            case 0x8800: chr_bank_[1]  = data; return true;
            case 0x9000: chr_bank_[2]  = data; return true;
            case 0x9800: chr_bank_[3]  = data; return true;
            case 0xA000: chr_bank_[4]  = data; return true;
            case 0xA800: chr_bank_[5]  = data; return true;
            case 0xB000: chr_bank_[6]  = data; return true;
            case 0xB800: chr_bank_[7]  = data; return true;
            case 0xC000: chr_bank_[8]  = data; return true;
            case 0xC800: chr_bank_[9]  = data; return true;
            case 0xD000: chr_bank_[10] = data; return true;
            case 0xD800: chr_bank_[11] = data; return true;

            case 0xE000:
                prg_bank_[0] = data & 0x3F;
                sound_enabled_ = !(data & 0x40);
                return true;
            case 0xE800:
                prg_bank_[1] = data & 0x3F;
                chr_lo_ram_ = (data & 0x40) != 0;
                chr_hi_ram_ = (data & 0x80) != 0;
                return true;
            case 0xF000:
                prg_bank_[2] = data & 0x3F;
                return true;
            case 0xF800:
                ram_addr_ = data & 0x7F;
                auto_increment_ = (data & 0x80) != 0;
                return false;  // no banking change
        }
        return false;
    }
};

} // namespace nes_system
