#pragma once
/*
 * mapper_030_unrom512.h — iNES Mapper 030 (UNROM 512)
 *
 * Self-flashable homebrew mapper with expanded UNROM banking.
 * 16KB switchable PRG at $8000 + fixed last 16KB at $C000 (up to 512KB).
 * 4 × 8KB CHR-RAM banks (32KB total), software-selected.
 * Single-screen mirroring controlled by register bit.
 *
 * Popular with homebrew: Battle Kid, Study Hall, many NESdev compo entries.
 *
 * Register ($8000-$FFFF):
 *   D4-D0: PRG 16KB bank select (0-31)
 *   D6-D5: CHR-RAM 8KB bank select (0-3)
 *   D7:    Mirror select (0 = 1-screen low, 1 = 1-screen high)
 *
 * Self-flashing behavior is not emulated (writes to PRG-ROM space are
 * ignored); the mapper operates as a read-only UNROM variant with
 * expanded banking.
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cstring>

namespace nes_system {

class Mapper030 : public Mapper {
private:
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_select_ = 0;
    Mirror mirror_mode_ = Mirror::ONESCREEN_LO;

    // 32KB on-board CHR-RAM (4 × 8KB banks).
    // UNROM 512 boards have 32KB CHR-RAM regardless of iNES header.
    static constexpr size_t CHR_RAM_SIZE = 32 * 1024;
    uint8_t chr_ram_buf_[CHR_RAM_SIZE] = {};

public:
    Mapper030(uint8_t prgBanks, uint8_t chrBanks) {
        (void)prgBanks; (void)chrBanks;
        std::memset(chr_ram_buf_, 0, sizeof(chr_ram_buf_));
    }

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_select_ = 0;
        mirror_mode_ = Mirror::ONESCREEN_LO;
    }

    Mirror mirror() override {
        // Submappers 1/2: fixed mirroring (no software control)
        if (submapper_ == 1) return Mirror::HORIZONTAL;
        if (submapper_ == 2) return Mirror::VERTICAL;
        return mirror_mode_;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k(config,
                                   const_cast<const uint8_t*>(chr_ram_buf_), CHR_RAM_SIZE,
                                   true, chr_bank_select_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000) {
            prg_bank_select_ = data & 0x1F;
            chr_bank_select_ = (data >> 5) & 0x03;
            mirror_mode_ = (data & 0x80) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
