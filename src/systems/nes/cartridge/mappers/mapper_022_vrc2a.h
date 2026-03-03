#pragma once
/*
 * mapper_022_vrc2a.h — iNES Mapper 022 (Konami VRC2a)
 *
 * Konami VRC2 variant with shifted CHR address lines.
 * PRG: 2 × 8KB switchable windows + 2 × 8KB fixed.
 * CHR: 8 × 1KB independently switchable.
 * Mirroring controlled by register at $9000.
 *
 * Address line mapping for VRC2a (mapper 022):
 *   A0 → VRC pin A1, A1 → VRC pin A0  (i.e. address bits are swapped)
 *
 * Key registers:
 *   $8000-$8003: PRG Select 0 (8KB at $8000)
 *   $9000-$9003: Mirroring
 *   $A000-$A003: PRG Select 1 (8KB at $A000)
 *   $B000-$E003: CHR Select 0-7 (lo/hi nybble pairs)
 *
 * Games: Twinbee 3, Ganbare Goemon, etc.
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper022 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t prg_bank_0_ = 0;   // $8000-$9FFF
    uint8_t prg_bank_1_ = 0;   // $A000-$BFFF

    // CHR 1KB bank registers (low and high nybbles combined)
    uint8_t chr_reg_[8] = {};

    Mirror mirror_mode_ = Mirror::VERTICAL;

    // VRC2a swaps A0/A1, so we apply the swap when decoding the address
    static uint16_t decode_addr(uint16_t addr) {
        // Swap bit 0 and bit 1
        uint16_t b0 = (addr >> 0) & 1;
        uint16_t b1 = (addr >> 1) & 1;
        return (addr & ~0x0003u) | (b0 << 1) | (b1 << 0);
    }

public:
    Mapper022(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_0_ = 0;
        prg_bank_1_ = 0;
        for (int i = 0; i < 8; i++) chr_reg_[i] = 0;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override { return mirror_mode_; }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t total_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (total_8k == 0) total_8k = 1;

        // $8000-$9FFF: switchable
        uint32_t b0 = prg_bank_0_ % total_8k;
        config.prg_pages[0] = prg_rom_ + b0 * 0x2000;
        config.prg_pages[1] = prg_rom_ + b0 * 0x2000 + 0x1000;

        // $A000-$BFFF: switchable
        uint32_t b1 = prg_bank_1_ % total_8k;
        config.prg_pages[2] = prg_rom_ + b1 * 0x2000;
        config.prg_pages[3] = prg_rom_ + b1 * 0x2000 + 0x1000;

        // $C000-$DFFF: fixed second-to-last 8KB
        uint32_t b2 = (total_8k >= 2) ? (total_8k - 2) : 0;
        config.prg_pages[4] = prg_rom_ + b2 * 0x2000;
        config.prg_pages[5] = prg_rom_ + b2 * 0x2000 + 0x1000;

        // $E000-$FFFF: fixed last 8KB
        uint32_t b3 = total_8k - 1;
        config.prg_pages[6] = prg_rom_ + b3 * 0x2000;
        config.prg_pages[7] = prg_rom_ + b3 * 0x2000 + 0x1000;

        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // VRC2a: CHR values are right-shifted by 1 (low bit ignored)
        uint32_t chr_1k_count = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;

        for (int i = 0; i < 8; i++) {
            // VRC2a shifts register value right by 1
            uint32_t bank = (chr_reg_[i] >> 1) % chr_1k_count;
            uint32_t offset = bank * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        // Apply VRC2a address line swap
        uint16_t decoded = decode_addr(addr);
        uint16_t reg = decoded & 0xF003;

        switch (reg) {
            // PRG bank 0
            case 0x8000: case 0x8001: case 0x8002: case 0x8003:
                prg_bank_0_ = data & 0x1F;
                return true;

            // Mirroring
            case 0x9000: case 0x9001: case 0x9002: case 0x9003:
                switch (data & 0x03) {
                    case 0: mirror_mode_ = Mirror::VERTICAL;     break;
                    case 1: mirror_mode_ = Mirror::HORIZONTAL;   break;
                    case 2: mirror_mode_ = Mirror::ONESCREEN_LO; break;
                    case 3: mirror_mode_ = Mirror::ONESCREEN_HI; break;
                }
                return true;

            // PRG bank 1
            case 0xA000: case 0xA001: case 0xA002: case 0xA003:
                prg_bank_1_ = data & 0x1F;
                return true;

            // CHR bank registers — low nybble at even, high nybble at odd
            case 0xB000: chr_reg_[0] = (chr_reg_[0] & 0xF0) | (data & 0x0F); return true;
            case 0xB001: chr_reg_[0] = (chr_reg_[0] & 0x0F) | ((data & 0x0F) << 4); return true;
            case 0xB002: chr_reg_[1] = (chr_reg_[1] & 0xF0) | (data & 0x0F); return true;
            case 0xB003: chr_reg_[1] = (chr_reg_[1] & 0x0F) | ((data & 0x0F) << 4); return true;
            case 0xC000: chr_reg_[2] = (chr_reg_[2] & 0xF0) | (data & 0x0F); return true;
            case 0xC001: chr_reg_[2] = (chr_reg_[2] & 0x0F) | ((data & 0x0F) << 4); return true;
            case 0xC002: chr_reg_[3] = (chr_reg_[3] & 0xF0) | (data & 0x0F); return true;
            case 0xC003: chr_reg_[3] = (chr_reg_[3] & 0x0F) | ((data & 0x0F) << 4); return true;
            case 0xD000: chr_reg_[4] = (chr_reg_[4] & 0xF0) | (data & 0x0F); return true;
            case 0xD001: chr_reg_[4] = (chr_reg_[4] & 0x0F) | ((data & 0x0F) << 4); return true;
            case 0xD002: chr_reg_[5] = (chr_reg_[5] & 0xF0) | (data & 0x0F); return true;
            case 0xD003: chr_reg_[5] = (chr_reg_[5] & 0x0F) | ((data & 0x0F) << 4); return true;
            case 0xE000: chr_reg_[6] = (chr_reg_[6] & 0xF0) | (data & 0x0F); return true;
            case 0xE001: chr_reg_[6] = (chr_reg_[6] & 0x0F) | ((data & 0x0F) << 4); return true;
            case 0xE002: chr_reg_[7] = (chr_reg_[7] & 0xF0) | (data & 0x0F); return true;
            case 0xE003: chr_reg_[7] = (chr_reg_[7] & 0x0F) | ((data & 0x0F) << 4); return true;

            default: return false;
        }
    }
};

} // namespace nes_system
