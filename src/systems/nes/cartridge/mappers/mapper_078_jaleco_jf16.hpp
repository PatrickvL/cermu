#pragma once
/*
 * mapper_078_jaleco_jf16.h — iNES Mapper 078 (Jaleco JF-16 / Irem IF-12)
 *
 * 16KB PRG + 8KB CHR with mirroring quirk.
 *   $8000-$FFFF write: D2-D0 = 16KB PRG bank, D7-D4 = 8KB CHR bank.
 *   Fixed last 16KB PRG at $C000.
 *
 * Mirroring: submapper 1 = 1-screen (Cosmo Carrier, Holy Diver);
 *            submapper 3 = H/V from D3 (Uchuusen).
 * iNES header doesn't reliably convey submapper, so we detect by
 * checking the initial mirroring flag and use D3 as toggle:
 *   D3=0 → one-screen LO or horizontal,  D3=1 → one-screen HI or vertical.
 *
 * Games: Cosmo Carrier, Holy Diver, Uchuusen - Cosmo Carrier.
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper078 : public Mapper {
private:
    uint8_t prg_bank_ = 0;
    uint8_t chr_bank_ = 0;
    Mirror mirror_mode_ = Mirror::ONESCREEN_LO;
    bool onescreen_mode_ = true; // submapper 1 default; submapper 3 uses H/V

public:
    Mapper078(uint8_t prgBanks, uint8_t chrBanks) { (void)prgBanks; (void)chrBanks; }

    void reset() override {
        prg_bank_ = 0;
        chr_bank_ = 0;
        // Detect submapper from header mirroring:
        // H or V → submapper 3 (Uchuusen): D3 toggles H/V
        // otherwise → submapper 1: D3 toggles ONESCREEN_LO/HI
        onescreen_mode_ = (header_mirror_ != Mirror::HORIZONTAL &&
                           header_mirror_ != Mirror::VERTICAL);
        mirror_mode_ = header_mirror_;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_16k_lo(config, prg_rom_, prg_rom_size_, prg_bank_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_8k(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;
        prg_bank_ = data & 0x07;
        chr_bank_ = (data >> 4) & 0x0F;
        mirror_mode_ = (data & 0x08)
            ? (onescreen_mode_ ? Mirror::ONESCREEN_HI : Mirror::VERTICAL)
            : (onescreen_mode_ ? Mirror::ONESCREEN_LO : Mirror::HORIZONTAL);
        return true;
    }
};

} // namespace nes_system
