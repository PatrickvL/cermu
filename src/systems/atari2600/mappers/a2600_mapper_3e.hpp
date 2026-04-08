#pragma once
/*
 * a2600_mapper_3e.h — Atari 2600 3E (Tigervision + RAM) bank switching
 *
 * Extension of the 3F (Tigervision) scheme that adds banked RAM.
 * ROM and RAM banks are mutually exclusive in the lower 2KB window.
 *
 * Memory layout:
 *   $1000–$17FF  Switchable: ROM 2KB bank OR RAM 1KB (split-port)
 *   $1800–$1FFF  Fixed: last 2KB of ROM
 *
 * Bank selection (bus-snoop, addresses in TIA space):
 *   Write to $003F (any addr with low 6 bits = $3F):
 *     Data bus selects ROM bank into $1000–$17FF
 *   Write to $003E (any addr with low 6 bits = $3E):
 *     Data bus selects RAM bank into $1000–$17FF
 *     RAM uses split-port: $1000–$13FF = write, $1400–$17FF = read
 *
 * RAM size: 32KB maximum (up to 128 × 256-byte banks can be addressed,
 * but in practice games use far less). We allocate 32KB.
 *
 * Games: Boulder Dash (homebrew port), various homebrew.
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"
#include <cstring>

struct A2600Mapper3E : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        if (offset >= 0x0800) {
            // $1800–$1FFF: fixed last 2KB of ROM
            return rom_[rom_size_ - 0x0800 + (offset & 0x07FF)];
        }

        if (ram_selected_) {
            // $1000–$17FF: RAM mode
            if (offset >= 0x0400) {
                // $1400–$17FF: RAM read port (1KB)
                return ram_[static_cast<uint32_t>(ram_bank_) * 0x0400 + (offset & 0x03FF)];
            }
            // $1000–$13FF: RAM write port — reading returns open bus
            return 0;
        }

        // $1000–$17FF: ROM bank
        return rom_[static_cast<uint32_t>(rom_bank_) * 0x0800 + offset];
    }

    void write(uint16_t offset, uint8_t data) override {
        if (offset < 0x0400 && ram_selected_) {
            // $1000–$13FF: RAM write port
            ram_[static_cast<uint32_t>(ram_bank_) * 0x0400 + (offset & 0x03FF)] = data;
        }
    }

    void bus_snoop(uint16_t addr, uint8_t data, bool is_write) override {
        if (!is_write || (addr & 0x1000)) return;

        if ((addr & 0x003F) == 0x003F) {
            // Write to $xx3F: select ROM bank
            uint8_t num_banks = static_cast<uint8_t>(rom_size_ / 0x0800);
            rom_bank_ = data % num_banks;
            ram_selected_ = false;
        } else if ((addr & 0x003F) == 0x003E) {
            // Write to $xx3E: select RAM bank
            ram_bank_ = data & 0x1F;  // Up to 32 RAM banks (32 × 1KB = 32KB)
            ram_selected_ = true;
        }
    }

    bool needs_bus_snoop() const override { return true; }

    const char* name() const override { return "3E"; }

    void reset() override {
        rom_bank_ = 0;
        ram_bank_ = 0;
        ram_selected_ = false;
        memset(ram_, 0, sizeof(ram_));
    }

    uint8_t current_bank() const override { return ram_selected_ ? ram_bank_ : rom_bank_; }
    uint8_t bank_count() const override {
        return static_cast<uint8_t>(rom_size_ / 0x0800);
    }

private:
    uint8_t rom_bank_ = 0;
    uint8_t ram_bank_ = 0;
    bool ram_selected_ = false;
    uint8_t ram_[32768] = {};  // 32KB maximum RAM
};
