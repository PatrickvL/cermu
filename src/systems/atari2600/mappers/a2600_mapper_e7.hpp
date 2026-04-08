#pragma once
/*
 * a2600_mapper_e7.h — Atari 2600 E7 (M-Network) bank switching
 *
 * 16KB ROM (8 × 2KB slices) + 2KB RAM (4 × 256-byte banks from a
 * 1KB RAM chip, confusingly called "2KB" in some docs).
 *
 * Memory layout:
 *   $1000–$17FF  Switchable 2KB ROM slice (select slice 0–6 via hotspots)
 *                Slice 7 selects 1KB RAM with read/write ports instead
 *   $1800–$19FF  256-byte RAM (bank-selectable, 4 banks)
 *                $1800–$18FF = write port, $1900–$19FF = read port
 *   $1A00–$1FFF  Fixed to last 1.5KB of ROM (slice 7 upper portion)
 *
 * Hotspots (triggered by read or write):
 *   $1FE0–$1FE6  Select ROM slice 0–6 into $1000–$17FF
 *   $1FE7         Select RAM mode for $1000–$17FF (1KB RAM, split ports)
 *   $1FE8–$1FEB  Select RAM bank 0–3 for $1800–$19FF
 *
 * When $1000–$17FF is in RAM mode (slice 7):
 *   $1000–$13FF = write port (512 bytes → lower 1KB RAM bank)
 *   $1400–$17FF = read port  (512 bytes → lower 1KB RAM bank)
 *
 * Games: Burgertime, He-Man, Masters of the Universe,
 *        Bump 'n' Jump, Super Challenge Football.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include <cstring>

struct A2600MapperE7 : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        check_hotspot(offset);

        if (offset < 0x0800) {
            // $1000–$17FF: switchable ROM slice or RAM
            if (ram_mode_) {
                // RAM mode: $1400–$17FF is read port (1KB)
                if (offset >= 0x0400)
                    return ram_1k_[offset & 0x03FF];
                // $1000–$13FF write port — read returns open bus / stale
                return rom_[7 * 0x0800 + offset];
            }
            return rom_[static_cast<uint32_t>(rom_slice_) * 0x0800 + offset];
        }

        if (offset < 0x0A00) {
            // $1800–$19FF: 256-byte banked RAM
            if (offset >= 0x0900)
                return ram_256_[static_cast<uint32_t>(ram_bank_) * 0x100 + (offset & 0xFF)];
            // $1800–$18FF write port — read returns stale
            return 0;
        }

        // $1A00–$1FFF: fixed to last 1.5KB of ROM
        // Offset within the fixed area: 0xA00–0xFFF
        // This maps to ROM slice 7 at offsets $200–$7FF (the upper 1.5KB)
        return rom_[7 * 0x0800 + (offset - 0x0800)];
    }

    void write(uint16_t offset, uint8_t data) override {
        check_hotspot(offset);

        if (offset < 0x0800 && ram_mode_) {
            // RAM mode: $1000–$13FF is write port (1KB)
            if (offset < 0x0400)
                ram_1k_[offset & 0x03FF] = data;
            return;
        }

        if (offset >= 0x0800 && offset < 0x0900) {
            // $1800–$18FF: 256-byte banked RAM write port
            ram_256_[static_cast<uint32_t>(ram_bank_) * 0x100 + (offset & 0xFF)] = data;
        }
    }

    const char* name() const override { return "E7"; }

    void reset() override {
        rom_slice_ = 0;
        ram_mode_ = false;
        ram_bank_ = 0;
        memset(ram_1k_, 0, sizeof(ram_1k_));
        memset(ram_256_, 0, sizeof(ram_256_));
    }

    uint8_t current_bank() const override { return rom_slice_; }
    uint8_t bank_count() const override { return 8; }

private:
    uint8_t rom_slice_ = 0;   // Which 2KB ROM slice at $1000–$17FF (0–6)
    bool ram_mode_ = false;    // true when slice 7 (RAM) is selected
    uint8_t ram_bank_ = 0;    // Which 256B RAM bank at $1800–$19FF (0–3)
    uint8_t ram_1k_[1024] = {};  // 1KB RAM for $1000–$17FF in RAM mode
    uint8_t ram_256_[1024] = {}; // 4 × 256B RAM banks for $1800–$19FF

    inline void check_hotspot(uint16_t offset) {
        if (offset >= 0x0FE0 && offset <= 0x0FEB) {
            uint8_t idx = static_cast<uint8_t>(offset - 0x0FE0);
            if (idx <= 6) {
                // $1FE0–$1FE6: select ROM slice 0–6
                rom_slice_ = idx;
                ram_mode_ = false;
            } else if (idx == 7) {
                // $1FE7: select RAM mode
                ram_mode_ = true;
            } else {
                // $1FE8–$1FEB: select RAM bank 0–3
                ram_bank_ = static_cast<uint8_t>(idx - 8);
            }
        }
    }
};
