#pragma once
/*
 * mapper_000_nrom.h — iNES Mapper 000 (NROM)
 *
 * The simplest NES mapper: no bank switching at all.
 * PRG: 16KB or 32KB fixed, CHR: 8KB fixed (ROM or RAM).
 * Games: Donkey Kong, Super Mario Bros., Excitebike, etc.
 */

#include "systems/nes/cartridge/nes_mapper.h"

namespace nes_system {

class Mapper000 : public Mapper {
private:
    uint8_t prg_banks;
    uint8_t chr_banks;

public:
    Mapper000(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks(prgBanks), chr_banks(chrBanks) {}

    void reset() override {}

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        if (prg_banks <= 1) {
            // NROM-128: 16KB mirrored at $8000 and $C000
            for (int i = 0; i < 4; i++) config.prg_pages[i] = prg_rom_ + (i * 0x1000);
            for (int i = 0; i < 4; i++) config.prg_pages[4 + i] = prg_rom_ + (i * 0x1000);
        } else {
            // NROM-256: 32KB at $8000-$FFFF
            for (int i = 0; i < 8; i++) config.prg_pages[i] = prg_rom_ + (i * 0x1000);
        }
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        for (int i = 0; i < 8; i++) {
            config.chr_pages[i] = chr_mem_ + (i * 0x0400);
            config.chr_writable[i] = chr_is_ram_;
        }
        // Mirroring is set by cartridge header (fixed for NROM)
    }

    bool register_write(uint16_t, uint8_t) override {
        return false;  // NROM has no mapper registers
    }
};

} // namespace nes_system
