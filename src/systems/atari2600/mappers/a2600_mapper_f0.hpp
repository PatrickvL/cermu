#pragma once
/*
 * a2600_mapper_f0.h — Atari 2600 F0 (Megaboy) bank switching (64KB)
 *
 * Sixteen 4KB banks with a sequential auto-incrementing bank counter.
 * Accessing hotspot $1FF0 increments the bank counter (wraps 0–15).
 * The read at $1FF0 also returns the current bank number.
 *
 * Unlike F4/EF which use multiple hotspots, F0 uses a single hotspot —
 * the program must increment through banks to reach the desired one.
 *
 * Games: Megaboy (the only known commercial use).
 */

#include "systems/atari2600/mappers/a2600_mapper.hpp"

struct A2600MapperF0 : public A2600Mapper {
    uint8_t read(uint16_t offset) override {
        if (offset == 0x0FF0) {
            bank_ = (bank_ + 1) & 0x0F;
            return bank_;
        }
        return rom_[static_cast<uint32_t>(bank_) * 0x1000 + offset];
    }

    void write(uint16_t offset, uint8_t data) override {
        (void)data;
        if (offset == 0x0FF0)
            bank_ = (bank_ + 1) & 0x0F;
    }

    const char* name() const override { return "F0"; }
    void reset() override { bank_ = 15; }  // Start in last bank
    uint8_t current_bank() const override { return bank_; }
    uint8_t bank_count() const override { return 16; }

private:
    uint8_t bank_ = 15;
};
