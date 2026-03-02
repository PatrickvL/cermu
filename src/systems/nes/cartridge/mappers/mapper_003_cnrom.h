#pragma once
/*
 * mapper_003_cnrom.h — iNES Mapper 003 (CNROM)
 *
 * Simple CHR bank switching: writes to $8000-$FFFF select an 8KB CHR bank.
 * PRG is 16KB or 32KB fixed (same as NROM).
 * Games: Galaxian, Gradius, Arkista's Ring, etc.
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper003 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t chr_bank_select_ = 0;

public:
    Mapper003(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override { chr_bank_select_ = 0; }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // Fixed PRG — same as NROM
        if (prg_banks_ <= 1) {
            for (int i = 0; i < 4; i++) config.prg_pages[i] = prg_rom_ + (i * 0x1000);
            for (int i = 0; i < 4; i++) config.prg_pages[4 + i] = prg_rom_ + (i * 0x1000);
        } else {
            for (int i = 0; i < 8; i++) config.prg_pages[i] = prg_rom_ + (i * 0x1000);
        }
        // CNROM has no PRG-RAM on real hardware
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // Switchable 8KB CHR bank
        uint32_t chr_base = chr_bank_select_ * 0x2000;
        for (int i = 0; i < 8; i++) {
            uint32_t offset = chr_base + i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : nullptr;
            config.chr_writable[i] = false;  // CNROM uses CHR-ROM
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            chr_bank_select_ = data & 0x03;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
