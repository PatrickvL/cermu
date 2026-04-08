#pragma once
/*
 * mapper_210_namco175340.h — iNES Mapper 210 (Namco 175 / Namco 340)
 *
 * Extended Namcot 108 with full 8-bit bank values.
 * Two sub-variants:
 *   Namco 175: hardwired mirroring (from header), 8KB PRG-RAM at $6000
 *   Namco 340: programmable mirroring via $E000
 * Both use 2×8KB switchable PRG + 2×8KB fixed, 8×1KB CHR.
 *
 * Register interface:
 *   $8000 (even): D2-D0 = register select (R0-R7)
 *   $8001 (odd):  D7-D0 = bank data
 *   $E000: mirroring (Namco 340 only)
 *
 * R0-R5: 1KB CHR banks (R0/R1 select 2KB, R2-R5 select 1KB),  R6-R7: 8KB PRG banks.
 * Fixed last two 8KB PRG banks at $C000/$E000.
 *
 * Games: Famista '91-'94, Splatter House SD, Dream Master, Genius Bakabon.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper210 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t target_register_ = 0;
    uint8_t registers_[8] = {};
    Mirror mirror_mode_ = Mirror::VERTICAL;
    bool hardwired_mirror_ = false;  // true for Namco 175

public:
    Mapper210(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        target_register_ = 0;
        std::memset(registers_, 0, sizeof(registers_));
        mirror_mode_ = header_mirror_;
        // Namco 175 has hardwired mirroring; Namco 340 has programmable.
        // Heuristic: if CHR-RAM, likely Namco 340; otherwise Namco 175.
        // Most emulators don't distinguish — we default to programmable.
        hardwired_mirror_ = false;
    }

    Mirror mirror() override {
        return hardwired_mirror_ ? header_mirror_ : mirror_mode_;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(registers_[6]) % n,
            static_cast<uint32_t>(registers_[7]) % n,
            (n >= 2) ? n - 2 : 0u, n - 1
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);

        // PRG-RAM at $6000
        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // R0/R1 select 2KB CHR banks ($0000/$0800), R2-R5 select 1KB ($1000-$1FFF)
        uint8_t chr[8] = {
            static_cast<uint8_t>(registers_[0] & 0xFE),       // R0 2KB even half
            static_cast<uint8_t>((registers_[0] & 0xFE) | 1), // R0 2KB odd half
            static_cast<uint8_t>(registers_[1] & 0xFE),       // R1 2KB even half
            static_cast<uint8_t>((registers_[1] & 0xFE) | 1), // R1 2KB odd half
            registers_[2], registers_[3], registers_[4], registers_[5]
        };
        mapper_helpers::set_chr_1k_pages(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        if ((addr & 0xF000) == 0xE000 && !hardwired_mirror_) {
            mirror_mode_ = mapper_helpers::mirror_from_2bit(data);
            return true;
        }

        if (!(addr & 0x0001))
            target_register_ = data & 0x07;
        else
            registers_[target_register_] = data;
        return true;
    }
};

} // namespace nes_system
