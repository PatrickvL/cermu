#pragma once
/*
 * mapper_005_mmc5.h — iNES Mapper 005 (MMC5 / ExROM)
 *
 * Nintendo's most complex first-party mapper. Partial implementation covering
 * the features exercised by common test ROMs (mmc5test, mmc5exram):
 *
 *   - PRG banking modes 0-3 (four granularities from 32KB to 8KB)
 *   - CHR banking modes 0-3 (1KB, 2KB, 4KB, 8KB sprite/BG split)
 *   - 1KB ExRAM ($5C00-$5FFF) with mode selection
 *   - Nametable mapping ($5105) — any slot to CIRAM page 0/1 or ExRAM/fill
 *   - Fill-mode tile/attribute ($5106/$5107)
 *   - PRG-RAM banking at $6000-$7FFF
 *   - 8×8 hardware multiplier ($5205/$5206)
 *   - Scanline IRQ counter ($5203/$5204)
 *   - Vertical split mode (stub)
 *
 * Games: Castlevania III, Laser Invasion, Uncharted Waters, etc.
 *
 * Reference: https://www.nesdev.org/wiki/MMC5
 */

#include "../nes_mapper.h"
#include <cstring>

namespace nes_system {

class Mapper005 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    // -----------------------------------------------------------------------
    // PRG banking
    // -----------------------------------------------------------------------
    uint8_t prg_mode_ = 3;             // $5100: PRG banking mode (0-3)
    uint8_t prg_bank_[5] = {};         // $5113-$5117: PRG bank registers
    bool prg_ram_protect_1_ = false;   // $5102 = $02
    bool prg_ram_protect_2_ = false;   // $5103 = $01

    // -----------------------------------------------------------------------
    // CHR banking
    // -----------------------------------------------------------------------
    uint8_t chr_mode_ = 0;             // $5101: CHR banking mode (0-3)
    uint16_t chr_bank_[12] = {};       // $5120-$512B: CHR bank registers
    bool chr_upper_set_ = false;       // Tracks last set (sprite vs BG)

    // -----------------------------------------------------------------------
    // Nametable mapping
    // -----------------------------------------------------------------------
    uint8_t nt_mapping_ = 0;           // $5105: nametable control
    uint8_t fill_tile_ = 0;            // $5106: fill-mode tile
    uint8_t fill_attr_ = 0;            // $5107: fill-mode attribute (bits 1-0)

    // -----------------------------------------------------------------------
    // ExRAM
    // -----------------------------------------------------------------------
    uint8_t exram_mode_ = 0;           // $5104: ExRAM mode (0-3)
    uint8_t exram_[1024] = {};         // 1KB internal extended RAM

    // -----------------------------------------------------------------------
    // IRQ
    // -----------------------------------------------------------------------
    uint8_t irq_scanline_ = 0;        // $5203: target scanline
    bool irq_enabled_ = false;         // $5204 bit 7
    bool irq_pending_ = false;         // IRQ pending flag
    bool in_frame_ = false;            // Whether PPU is rendering
    uint8_t scanline_counter_ = 0;     // Current scanline count

    // -----------------------------------------------------------------------
    // Multiplier
    // -----------------------------------------------------------------------
    uint8_t multiplicand_ = 0xFF;      // $5205
    uint8_t multiplier_ = 0xFF;        // $5206
    uint16_t product_ = 0;             // multiplicand_ × multiplier_

    // -----------------------------------------------------------------------
    // Mirroring
    // -----------------------------------------------------------------------
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    bool prg_ram_writable() const {
        return prg_ram_protect_1_ && prg_ram_protect_2_;
    }

    /// Resolve a PRG bank register value to a ROM/RAM pointer.
    /// bit 7 of the bank value: 0 = RAM, 1 = ROM.
    const uint8_t* resolve_prg_page(uint8_t bank_val, uint32_t page_size,
                                     bool& is_ram_out) const {
        is_ram_out = !(bank_val & 0x80);
        if (is_ram_out) {
            // PRG-RAM bank (ignore bit 7)
            if (!prg_ram_ || prg_ram_size_ == 0) return nullptr;
            uint32_t offset = (bank_val & 0x07) * page_size;
            return (offset < prg_ram_size_) ? prg_ram_ + offset : prg_ram_;
        } else {
            // PRG-ROM bank
            uint32_t total_pages = static_cast<uint32_t>(prg_rom_size_ / page_size);
            if (total_pages == 0) return nullptr;
            uint32_t page = (bank_val & 0x7F) % total_pages;
            return prg_rom_ + page * page_size;
        }
    }

    uint8_t* resolve_prg_page_write(uint8_t bank_val, uint32_t page_size) const {
        if (bank_val & 0x80) return nullptr;  // ROM — not writable
        if (!prg_ram_ || prg_ram_size_ == 0) return nullptr;
        if (!prg_ram_writable()) return nullptr;
        uint32_t offset = (bank_val & 0x07) * page_size;
        return (offset < prg_ram_size_) ? prg_ram_ + offset : prg_ram_;
    }

public:
    Mapper005(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_mode_ = 3;
        chr_mode_ = 0;
        std::memset(prg_bank_, 0, sizeof(prg_bank_));
        prg_bank_[4] = 0xFF;  // Last 8KB bank defaults to last page (ROM)
        std::memset(chr_bank_, 0, sizeof(chr_bank_));
        chr_upper_set_ = false;
        nt_mapping_ = 0;
        fill_tile_ = 0;
        fill_attr_ = 0;
        exram_mode_ = 0;
        std::memset(exram_, 0, sizeof(exram_));
        prg_ram_protect_1_ = false;
        prg_ram_protect_2_ = false;
        irq_scanline_ = 0;
        irq_enabled_ = false;
        irq_pending_ = false;
        in_frame_ = false;
        scanline_counter_ = 0;
        multiplicand_ = 0xFF;
        multiplier_ = 0xFF;
        product_ = 0;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override { return mirror_mode_; }

    bool irq_state() override { return irq_pending_ && irq_enabled_; }

    void irq_clear() override { irq_pending_ = false; }

    // =======================================================================
    // Scanline notification — called by PPU at each visible scanline
    // =======================================================================

    void notify_a12(bool a12_high, uint64_t ppu_cycle) override {
        // MMC5 doesn't use A12 directly; it uses an internal scanline
        // detector. We repurpose this callback for scanline counting.
        // The PPU should call this once per scanline with a12_high=true.
        if (!a12_high) return;

        if (!in_frame_) {
            in_frame_ = true;
            scanline_counter_ = 0;
        }

        scanline_counter_++;

        if (scanline_counter_ == irq_scanline_) {
            irq_pending_ = true;
        }

        // End of visible frame (after 240 scanlines)
        if (scanline_counter_ >= 240) {
            in_frame_ = false;
        }
    }

    // =======================================================================
    // Bank configuration — PRG
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        bool dummy_ram;

        switch (prg_mode_) {
            case 0: {
                // Mode 0: One 32KB bank at $8000-$FFFF
                // $5117 selects 32KB page (bits 6-2 used, bit 7 = ROM always)
                uint8_t bank_val = prg_bank_[4] | 0x80;  // Force ROM
                uint32_t total_32k = static_cast<uint32_t>(prg_rom_size_ / 0x8000);
                if (total_32k == 0) total_32k = 1;
                uint32_t page = ((bank_val & 0x7F) >> 2) % total_32k;
                uint32_t base = page * 0x8000;
                for (int i = 0; i < 8; i++) {
                    uint32_t offset = base + i * 0x1000;
                    config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
                }
                break;
            }
            case 1: {
                // Mode 1: Two 16KB banks — $5115 at $8000, $5117 at $C000
                // $5115: ROM or RAM
                const uint8_t* p0 = resolve_prg_page(prg_bank_[2], 0x4000, dummy_ram);
                const uint8_t* p1 = resolve_prg_page(prg_bank_[4] | 0x80, 0x4000, dummy_ram);
                for (int i = 0; i < 4; i++) {
                    config.prg_pages[i] = p0 ? p0 + i * 0x1000 : nullptr;
                    config.prg_pages[4 + i] = p1 ? p1 + i * 0x1000 : nullptr;
                }
                // Write pages for RAM banks
                uint8_t* w0 = resolve_prg_page_write(prg_bank_[2], 0x4000);
                for (int i = 0; i < 4; i++) {
                    config.prg_write_pages[i] = w0 ? w0 + i * 0x1000 : nullptr;
                    config.prg_write_pages[4 + i] = nullptr;  // $C000 is ROM
                }
                break;
            }
            case 2: {
                // Mode 2: 16KB + 8KB + 8KB
                // $5115 at $8000-$BFFF (16KB), $5116 at $C000, $5117 at $E000
                const uint8_t* p0 = resolve_prg_page(prg_bank_[2], 0x4000, dummy_ram);
                const uint8_t* p1 = resolve_prg_page(prg_bank_[3], 0x2000, dummy_ram);
                const uint8_t* p2 = resolve_prg_page(prg_bank_[4] | 0x80, 0x2000, dummy_ram);
                for (int i = 0; i < 4; i++)
                    config.prg_pages[i] = p0 ? p0 + i * 0x1000 : nullptr;
                config.prg_pages[4] = p1;
                config.prg_pages[5] = p1 ? p1 + 0x1000 : nullptr;
                config.prg_pages[6] = p2;
                config.prg_pages[7] = p2 ? p2 + 0x1000 : nullptr;

                uint8_t* w0 = resolve_prg_page_write(prg_bank_[2], 0x4000);
                uint8_t* w1 = resolve_prg_page_write(prg_bank_[3], 0x2000);
                for (int i = 0; i < 4; i++)
                    config.prg_write_pages[i] = w0 ? w0 + i * 0x1000 : nullptr;
                config.prg_write_pages[4] = w1;
                config.prg_write_pages[5] = w1 ? w1 + 0x1000 : nullptr;
                config.prg_write_pages[6] = nullptr;
                config.prg_write_pages[7] = nullptr;
                break;
            }
            case 3: {
                // Mode 3: Four 8KB banks
                // $5114 at $8000, $5115 at $A000, $5116 at $C000, $5117 at $E000
                for (int slot = 0; slot < 4; slot++) {
                    uint8_t bv = (slot == 3) ? (prg_bank_[slot + 1] | 0x80) : prg_bank_[slot + 1];
                    const uint8_t* p = resolve_prg_page(bv, 0x2000, dummy_ram);
                    config.prg_pages[slot * 2]     = p;
                    config.prg_pages[slot * 2 + 1] = p ? p + 0x1000 : nullptr;

                    uint8_t* w = (slot < 3) ? resolve_prg_page_write(prg_bank_[slot + 1], 0x2000) : nullptr;
                    config.prg_write_pages[slot * 2]     = w;
                    config.prg_write_pages[slot * 2 + 1] = w ? w + 0x1000 : nullptr;
                }
                break;
            }
        }

        // PRG-RAM at $6000-$7FFF from $5113
        if (prg_ram_ && prg_ram_size_ > 0) {
            uint32_t ram_bank = (prg_bank_[0] & 0x07);
            uint32_t offset = ram_bank * 0x2000;
            if (offset + 0x2000 <= prg_ram_size_) {
                config.prg_ram_base = prg_ram_ + offset;
            } else {
                config.prg_ram_base = prg_ram_;
            }
            config.prg_ram_size = 0x2000;
            config.prg_ram_enabled = true;
            config.prg_ram_write_protected = !prg_ram_writable();
        }

        // Expansion area at $5000-$5FFF is handled by register_write/read
        // No direct page mapping for expansion space
    }

    // =======================================================================
    // Bank configuration — CHR
    // =======================================================================

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_1k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;
        if (chr_1k == 0) chr_1k = 1;

        // Use the 'sprite' registers ($5120-$5127) — always the lower 8 slots
        // BG registers ($5128-$512B) are used during rendering based on
        // sprite-height mode; we simplify to sprite set here.
        switch (chr_mode_) {
            case 0: {
                // 8KB mode — $5127 selects 8KB page
                uint32_t base = (chr_bank_[7] * 8) % chr_1k;
                for (int i = 0; i < 8; i++) {
                    uint32_t b = (base + i) % chr_1k;
                    config.chr_pages[i] = chr_mem_ + b * 0x0400;
                    config.chr_writable[i] = chr_is_ram_;
                }
                break;
            }
            case 1: {
                // 4KB mode — $5123 at $0000, $5127 at $1000
                uint32_t lo = (chr_bank_[3] * 4) % chr_1k;
                uint32_t hi = (chr_bank_[7] * 4) % chr_1k;
                for (int i = 0; i < 4; i++) {
                    uint32_t b_lo = (lo + i) % chr_1k;
                    uint32_t b_hi = (hi + i) % chr_1k;
                    config.chr_pages[i]     = chr_mem_ + b_lo * 0x0400;
                    config.chr_pages[4 + i] = chr_mem_ + b_hi * 0x0400;
                    config.chr_writable[i] = chr_is_ram_;
                    config.chr_writable[4 + i] = chr_is_ram_;
                }
                break;
            }
            case 2: {
                // 2KB mode — $5121/$5123 at $0000/$0800, $5125/$5127 at $1000/$1800
                for (int pair = 0; pair < 4; pair++) {
                    int reg = pair * 2 + 1;  // registers 1,3,5,7
                    uint32_t base = (chr_bank_[reg] * 2) % chr_1k;
                    int slot = pair * 2;
                    config.chr_pages[slot]     = chr_mem_ + ((base) % chr_1k) * 0x0400;
                    config.chr_pages[slot + 1] = chr_mem_ + ((base + 1) % chr_1k) * 0x0400;
                    config.chr_writable[slot] = chr_is_ram_;
                    config.chr_writable[slot + 1] = chr_is_ram_;
                }
                break;
            }
            case 3: {
                // 1KB mode — each register maps 1KB
                for (int i = 0; i < 8; i++) {
                    uint32_t b = chr_bank_[i] % chr_1k;
                    config.chr_pages[i] = chr_mem_ + b * 0x0400;
                    config.chr_writable[i] = chr_is_ram_;
                }
                break;
            }
        }

        // Nametable mapping — decode $5105
        for (int slot = 0; slot < 4; slot++) {
            uint8_t src = (nt_mapping_ >> (slot * 2)) & 0x03;
            config.nt_page[slot] = src;  // 0=CIRAM-0, 1=CIRAM-1, 2=ExRAM, 3=fill
        }
    }

    // =======================================================================
    // Register read — $5000-$5FFF expansion area
    // =======================================================================

    /// CPU read in $5000-$5FFF range. Must be called by the bus for
    /// expansion area reads. Returns the byte and sets `handled` to true
    /// if the address was serviced.
    uint8_t expansion_read(uint16_t addr, bool& handled) const {
        handled = true;

        if (addr == 0x5204) {
            // IRQ status — bit 7 = pending, bit 6 = in-frame
            uint8_t val = 0;
            if (irq_pending_) val |= 0x80;
            if (in_frame_)    val |= 0x40;
            return val;
        }

        if (addr == 0x5205) {
            return static_cast<uint8_t>(product_ & 0xFF);
        }
        if (addr == 0x5206) {
            return static_cast<uint8_t>((product_ >> 8) & 0xFF);
        }

        // ExRAM read ($5C00-$5FFF)
        if (addr >= 0x5C00 && addr <= 0x5FFF) {
            if (exram_mode_ >= 2) {  // Modes 2 & 3: readable
                return exram_[addr - 0x5C00];
            }
            return 0;  // Modes 0 & 1: returns open bus (0 as fallback)
        }

        handled = false;
        return 0;
    }

    // =======================================================================
    // Register write — combines $5000-$5FFF and $8000-$FFFF
    // =======================================================================

    bool register_write(uint16_t addr, uint8_t data) override {
        // MMC5 internal registers $5000-$5FFF
        if (addr >= 0x5000 && addr < 0x6000) {
            return write_expansion(addr, data);
        }

        // No mapper register writes at $8000-$FFFF for MMC5
        // (PRG banking is controlled entirely via $5100-$5117)
        return false;
    }

private:
    bool write_expansion(uint16_t addr, uint8_t data) {
        switch (addr) {
            // --- Sound registers $5000-$5015 (stub — not implemented) ---

            // --- PRG mode ---
            case 0x5100:
                prg_mode_ = data & 0x03;
                return true;

            // --- CHR mode ---
            case 0x5101:
                chr_mode_ = data & 0x03;
                return true;

            // --- PRG-RAM protect ---
            case 0x5102:
                prg_ram_protect_1_ = (data & 0x03) == 0x02;
                return false;  // Doesn't change banking
            case 0x5103:
                prg_ram_protect_2_ = (data & 0x03) == 0x01;
                return false;

            // --- ExRAM mode ---
            case 0x5104:
                exram_mode_ = data & 0x03;
                return false;

            // --- Nametable mapping ---
            case 0x5105:
                nt_mapping_ = data;
                // Derive a Mirror mode for the base class
                // This is a simplification; real MMC5 NT mapping is per-slot
                if (data == 0x50) mirror_mode_ = Mirror::VERTICAL;
                else if (data == 0x44) mirror_mode_ = Mirror::HORIZONTAL;
                else if (data == 0x00) mirror_mode_ = Mirror::ONESCREEN_LO;
                else if (data == 0x55) mirror_mode_ = Mirror::ONESCREEN_HI;
                else mirror_mode_ = Mirror::FOUR_SCREEN;  // Custom mapping
                return true;

            // --- Fill-mode tile & attribute ---
            case 0x5106:
                fill_tile_ = data;
                return false;
            case 0x5107:
                fill_attr_ = data & 0x03;
                return false;

            // --- PRG bank registers ---
            case 0x5113: prg_bank_[0] = data & 0x07; return true;   // $6000-$7FFF RAM
            case 0x5114: prg_bank_[1] = data;         return true;   // $8000-$9FFF (mode 3)
            case 0x5115: prg_bank_[2] = data;         return true;   // $A000-$BFFF / $8000-$BFFF
            case 0x5116: prg_bank_[3] = data;         return true;   // $C000-$DFFF
            case 0x5117: prg_bank_[4] = data;         return true;   // $E000-$FFFF (always ROM)

            // --- CHR bank registers (sprite set $5120-$5127) ---
            case 0x5120: chr_bank_[0]  = data; chr_upper_set_ = false; return true;
            case 0x5121: chr_bank_[1]  = data; chr_upper_set_ = false; return true;
            case 0x5122: chr_bank_[2]  = data; chr_upper_set_ = false; return true;
            case 0x5123: chr_bank_[3]  = data; chr_upper_set_ = false; return true;
            case 0x5124: chr_bank_[4]  = data; chr_upper_set_ = false; return true;
            case 0x5125: chr_bank_[5]  = data; chr_upper_set_ = false; return true;
            case 0x5126: chr_bank_[6]  = data; chr_upper_set_ = false; return true;
            case 0x5127: chr_bank_[7]  = data; chr_upper_set_ = false; return true;

            // --- CHR bank registers (BG set $5128-$512B) ---
            case 0x5128: chr_bank_[8]  = data; chr_upper_set_ = true; return true;
            case 0x5129: chr_bank_[9]  = data; chr_upper_set_ = true; return true;
            case 0x512A: chr_bank_[10] = data; chr_upper_set_ = true; return true;
            case 0x512B: chr_bank_[11] = data; chr_upper_set_ = true; return true;

            // --- Vertical split (stub) ---
            case 0x5200: return false;  // Split mode control
            case 0x5201: return false;  // Split scroll
            case 0x5202: return false;  // Split bank

            // --- IRQ ---
            case 0x5203:
                irq_scanline_ = data;
                return false;
            case 0x5204:
                irq_enabled_ = (data & 0x80) != 0;
                return false;

            // --- Multiplier ---
            case 0x5205:
                multiplicand_ = data;
                product_ = static_cast<uint16_t>(multiplicand_) * multiplier_;
                return false;
            case 0x5206:
                multiplier_ = data;
                product_ = static_cast<uint16_t>(multiplicand_) * multiplier_;
                return false;

            default:
                break;
        }

        // ExRAM write ($5C00-$5FFF)
        if (addr >= 0x5C00 && addr <= 0x5FFF) {
            if (exram_mode_ <= 1) {
                // Modes 0 & 1: writable during rendering (simplified: always writable)
                exram_[addr - 0x5C00] = data;
            } else if (exram_mode_ == 2) {
                // Mode 2: writable anytime
                exram_[addr - 0x5C00] = data;
            }
            // Mode 3: read-only
            return false;
        }

        return false;
    }
};

} // namespace nes_system
