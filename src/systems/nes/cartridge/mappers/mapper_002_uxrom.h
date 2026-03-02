#pragma once
/*
 * mapper_002_uxrom.h — iNES Mapper 002 (UxROM)
 *
 * Simple PRG bank switching: switchable 16KB at $8000, fixed last 16KB at $C000.
 * CHR 8KB ROM/RAM with no switching.
 * Games: Castlevania, Contra, Metal Gear, Mega Man, etc.
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper002 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;

public:
    Mapper002(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override { prg_bank_select_ = 0; }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // Switchable 16KB at $8000
        uint32_t bank_base = prg_bank_select_ * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = bank_base + i * 0x1000;
            config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }
        // Fixed last 16KB at $C000
        uint32_t last_base = (prg_banks_ - 1) * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = last_base + i * 0x1000;
            config.prg_pages[4 + i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }
        // UxROM has no PRG-RAM on real hardware
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // 8KB CHR direct (ROM or RAM)
        for (int i = 0; i < 8; i++) {
            config.chr_pages[i] = chr_mem_ + (i * 0x0400);
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            prg_bank_select_ = data & 0x0F;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
