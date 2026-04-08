#pragma once
/*
 * mapper_120_fds_hack.hpp — iNES Mapper 120 (Tobidase Daisakusen FDS conversion hack)
 *
 * Bootleg FDS-to-cartridge conversion.
 *
 * PRG layout:
 *   $6000-$7FFF: switchable 8KB from register at $41FF
 *   $8000-$FFFF: fixed last 32KB of PRG-ROM
 *
 * CHR: 8KB fixed CHR-ROM.
 * Mirror: hardwired.
 *
 * Games: Tobidase Daisakusen (bootleg cart).
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper120 : public Mapper {
private:
    uint8_t prg_bank_ = 0;

public:
    Mapper120(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        prg_bank_ = 0;
    }

    Mirror mirror() override { return header_mirror_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);

        // $6000: switchable 8KB PRG-ROM bank (read-only — this is ROM, not RAM)
        config.prg_ram_enabled = true;
        config.prg_ram_base = const_cast<uint8_t*>(prg_rom_ + (static_cast<uint32_t>(prg_bank_ & 0x07) % n) * 0x2000);
        config.prg_ram_size = 0x2000;
        config.prg_ram_write_protected = true;

        // $8000-$FFFF: fixed last 32KB
        uint32_t fixed = (n >= 4) ? n - 4 : 0u;
        uint32_t banks[4] = { fixed, fixed + 1, fixed + 2, fixed + 3 };
        for (auto& b : banks) if (b >= n) b = n - 1;
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        // Register at $41FF in the expansion area
        if (addr == 0x41FF) {
            prg_bank_ = data & 0x07;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
