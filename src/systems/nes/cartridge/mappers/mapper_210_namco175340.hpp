#pragma once
/*
 * mapper_210_namco175340.h — iNES Mapper 210 (Namco 175 / Namco 340)
 *
 * Direct-mapped register variant of the Namco 163 family.
 * Two sub-variants:
 *   Namco 175: hardwired mirroring (from header), PRG-RAM write protect via $C000
 *   Namco 340: programmable mirroring via $E000 bits 7-6
 *
 * Register interface (addr & $F800):
 *   $8000/$8800/$9000/$9800: 1KB CHR bank select (banks 0-3)
 *   $A000/$A800/$B000/$B800: 1KB CHR bank select (banks 4-7)
 *   $C000: PRG-RAM write protect (Namco 175 only, bit 0 = enable)
 *   $E000: 8KB PRG bank 0 ($8000-$9FFF), bits 5-0 = bank
 *          Namco 340: bits 7-6 = mirroring
 *   $E800: 8KB PRG bank 1 ($A000-$BFFF), bits 5-0 = bank
 *   $F000: 8KB PRG bank 2 ($C000-$DFFF), bits 5-0 = bank
 *   PRG bank 3 ($E000-$FFFF) is always the last bank.
 *
 * Games: Famista '91-'94, Splatter House SD, Dream Master, Genius Bakabon.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper210 : public Mapper {
private:

    uint8_t chr_banks_[8] = {};     // 8 × 1KB CHR bank registers
    uint8_t prg_banks_[3] = {};     // 3 × 8KB PRG bank registers (4th is fixed)
    uint8_t write_protect_ = 0;     // PRG-RAM write protect (Namco 175)
    Mirror mirror_mode_ = Mirror::VERTICAL;
    bool hardwired_mirror_ = false; // true for Namco 175

public:
    Mapper210(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        std::memset(chr_banks_, 0, sizeof(chr_banks_));
        std::memset(prg_banks_, 0, sizeof(prg_banks_));
        write_protect_ = 0;
        mirror_mode_ = header_mirror_;
        // NES 2.0 submapper: 1 = Namco 175 (hardwired), 2 = Namco 340 (programmable)
        // Fallback: CHR-RAM → likely Namco 340; CHR-ROM → likely Namco 175
        if (submapper_ == 1)
            hardwired_mirror_ = true;
        else if (submapper_ == 2)
            hardwired_mirror_ = false;
        else
            hardwired_mirror_ = !chr_is_ram_;
    }

    Mirror mirror() override {
        return hardwired_mirror_ ? header_mirror_ : mirror_mode_;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(prg_banks_[0] & 0x3F) % n,
            static_cast<uint32_t>(prg_banks_[1] & 0x3F) % n,
            static_cast<uint32_t>(prg_banks_[2] & 0x3F) % n,
            n - 1  // $E000-$FFFF always last bank
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);

        // PRG-RAM at $6000
        set_prg_ram(config, prg_ram_, prg_ram_size_);
        // Namco 175: write protect controlled by $C000 bit 0
        if (hardwired_mirror_) {
            config.prg_ram_write_protected = !(write_protect_ & 0x01);
        }
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint8_t chr[8];
        for (int i = 0; i < 8; i++)
            chr[i] = chr_banks_[i];
        mapper_helpers::set_chr_1k_pages(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xF800) {
            // CHR bank select: 8 × 1KB
            case 0x8000: chr_banks_[0] = data; return true;
            case 0x8800: chr_banks_[1] = data; return true;
            case 0x9000: chr_banks_[2] = data; return true;
            case 0x9800: chr_banks_[3] = data; return true;
            case 0xA000: chr_banks_[4] = data; return true;
            case 0xA800: chr_banks_[5] = data; return true;
            case 0xB000: chr_banks_[6] = data; return true;
            case 0xB800: chr_banks_[7] = data; return true;

            // PRG-RAM write protect (Namco 175 only)
            case 0xC000:
                if (hardwired_mirror_) {
                    write_protect_ = data;
                    return true;
                }
                return false;

            // PRG bank select: 3 × 8KB
            case 0xE000:
                prg_banks_[0] = data & 0x3F;
                if (!hardwired_mirror_) {
                    // Namco 340: bits 7-6 control mirroring
                    // 0=one-screen A, 1=vertical, 2=horizontal, 3=one-screen B
                    switch ((data >> 6) & 0x03) {
                        case 0: mirror_mode_ = Mirror::ONESCREEN_LO; break;
                        case 1: mirror_mode_ = Mirror::VERTICAL;     break;
                        case 2: mirror_mode_ = Mirror::HORIZONTAL;   break;
                        case 3: mirror_mode_ = Mirror::ONESCREEN_HI; break;
                    }
                }
                return true;
            case 0xE800:
                prg_banks_[1] = data & 0x3F;
                return true;
            case 0xF000:
                prg_banks_[2] = data & 0x3F;
                return true;

            default:
                return false;
        }
    }
};

} // namespace nes_system
