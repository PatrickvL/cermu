#pragma once
/*
 * a2600_mapper_3f.h — Atari 2600 3F (Tigervision) bank switching
 *
 * Supports ROMs from 8KB up to 512KB. ROM is divided into 2KB banks.
 * The 4KB cartridge window is split:
 *
 *   $1000–$17FF  Switchable 2KB bank (selected by writing to TIA space)
 *   $1800–$1FFF  Fixed to last 2KB bank
 *
 * Bank selection: any write where (addr & 0x3F) == 0x3F selects the
 * bank from the data bus value. The canonical address is $003F (a TIA
 * location), but any mirrored address with low 6 bits == 0x3F works.
 * Since the write target is in TIA space (A12=0), this mapper needs
 * bus snooping to detect the bank-select writes.
 *
 * Games: Miner 2049er, Springer, Espial, Polaris, River Patrol.
 */

#include "systems/atari2600/mappers/a2600_mapper.h"

struct A2600Mapper3F : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        if (offset < 0x0800) {
            // Switchable 2KB bank
            return rom_[static_cast<uint32_t>(bank_) * 0x0800 + offset];
        } else {
            // Fixed: last 2KB bank
            return rom_[rom_size_ - 0x0800 + (offset & 0x07FF)];
        }
    }

    void bus_snoop(uint16_t addr, uint8_t data, bool is_write) override {
        // Bank select: write to any address where low 6 bits == 0x3F
        // and address is NOT in cartridge space (A12=0)
        if (is_write && !(addr & 0x1000) && (addr & 0x003F) == 0x003F) {
            uint8_t num_banks = static_cast<uint8_t>(rom_size_ / 0x0800);
            bank_ = data % num_banks;
        }
    }

    bool needs_bus_snoop() const override { return true; }

    const char* name() const override { return "3F"; }
    void reset() override { bank_ = 0; }
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override {
        return static_cast<uint8_t>(rom_size_ / 0x0800);
    }

private:
    uint8_t bank_ = 0;
};
