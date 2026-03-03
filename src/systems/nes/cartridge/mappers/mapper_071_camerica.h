#pragma once
/*
 * mapper_071_camerica.h — iNES Mapper 071 (Camerica / Codemasters)
 *
 * PRG bank switching used by Codemasters/Camerica games.
 * 16KB switchable at $8000, fixed last 16KB at $C000.
 * Some variants support single-screen mirroring via $9000.
 * Games: Fire Hawk, Micro Machines, Bee 52, Fantastic Adventures of Dizzy.
 *
 * $8000-$9FFF: Mirroring control (bit 4 on some boards)
 * $C000-$FFFF: PRG bank select (lower bits)
 */

#include "../nes_mapper.h"

namespace nes_system {

class Mapper071 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;
    uint8_t prg_bank_select_ = 0;
    Mirror mirror_mode_ = Mirror::VERTICAL;
    bool has_mirroring_control_ = false;

public:
    Mapper071(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_select_ = 0;
        mirror_mode_ = header_mirror_;
        has_mirroring_control_ = false;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t total_16k = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (total_16k == 0) total_16k = 1;

        // Switchable 16KB at $8000
        uint32_t bank = prg_bank_select_ % total_16k;
        uint32_t base = bank * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = base + i * 0x1000;
            config.prg_pages[i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }
        // Fixed last 16KB at $C000
        uint32_t last = (total_16k - 1) * 0x4000;
        for (int i = 0; i < 4; i++) {
            uint32_t offset = last + i * 0x1000;
            config.prg_pages[4 + i] = (offset < prg_rom_size_) ? prg_rom_ + offset : nullptr;
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // 8KB CHR-RAM (no switching)
        for (int i = 0; i < 8; i++) {
            uint32_t offset = i * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = true;
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr >= 0x8000 && addr <= 0x9FFF) {
            // Mirroring control (Codemasters variant)
            if (data & 0x10) {
                mirror_mode_ = Mirror::ONESCREEN_HI;
                has_mirroring_control_ = true;
            } else if (has_mirroring_control_) {
                mirror_mode_ = Mirror::ONESCREEN_LO;
            }
            return has_mirroring_control_;
        }
        if (addr >= 0xC000) {
            prg_bank_select_ = data & 0x0F;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
