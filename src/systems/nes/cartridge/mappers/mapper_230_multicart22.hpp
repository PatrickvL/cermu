#pragma once
/*
 * mapper_230_multicart22.h — iNES Mapper 230 (22-in-1 multicart)
 *
 * Reset-switch multicart that toggles between two modes:
 *   Mode 0 (Contra 1): 16KB switchable at $8000, fixed last at $C000.
 *     $8000-$FFFF write: D2-D0 = 16KB PRG bank at $8000.
 *   Mode 1 (Multicart): 16KB at $8000 and $C000 from outer bank.
 *     $8000-$FFFF write: D4-D0 = 16KB PRG bank (mapped to both halves),
 *     D5 = mirroring (0=V, 1=H).
 *
 * The mode toggles on each reset (power-on starts in mode 0).
 * Contra mode uses the first 128KB of PRG; multicart mode uses the rest.
 *
 * Games: 22-in-1 (Contra + multicart).
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper230 : public Mapper {
private:
    uint8_t prg_bank_ = 0;
    bool mode_ = true;            // toggled on reset; starts true so first reset → false (Contra)
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // Contra uses first 8 × 16KB (128KB), multicart uses the rest.
    static constexpr uint32_t CONTRA_SIZE = 0x20000;  // 128KB

public:
    Mapper230(uint8_t prgBanks, uint8_t chrBanks) { (void)prgBanks; (void)chrBanks; }

    void reset() override {
        mode_ = !mode_;   // Toggle mode on each reset
        prg_bank_ = 0;
        mirror_mode_ = Mirror::VERTICAL;
    }

    Mirror mirror() override { return mirror_mode_; }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        if (!mode_) {
            // Contra mode: first 128KB, 16KB switchable at $8000 + fixed last at $C000
            uint32_t contra_16k = CONTRA_SIZE / 0x4000;
            if (contra_16k == 0) contra_16k = 1;
            uint32_t bank = prg_bank_ % contra_16k;
            uint32_t lo = bank * 0x4000;
            uint32_t hi = (contra_16k - 1) * 0x4000;
            for (int i = 0; i < 4; i++) {
                config.prg_pages[i]     = prg_rom_ + lo + i * 0x1000;
                config.prg_pages[4 + i] = prg_rom_ + hi + i * 0x1000;
            }
        } else {
            // Multicart mode: 16KB bank mapped to both $8000 and $C000
            const uint8_t* rom_base = prg_rom_ + CONTRA_SIZE;
            size_t rom_avail = (prg_rom_size_ > CONTRA_SIZE) ? prg_rom_size_ - CONTRA_SIZE : 0;
            uint32_t num_16k = rom_avail > 0 ? static_cast<uint32_t>(rom_avail / 0x4000) : 1;
            if (num_16k == 0) num_16k = 1;
            uint32_t bank = prg_bank_ % num_16k;
            uint32_t base = bank * 0x4000;
            for (int i = 0; i < 4; i++) {
                const uint8_t* page = rom_base + base + i * 0x1000;
                config.prg_pages[i]     = page;
                config.prg_pages[4 + i] = page;  // Same 16KB mapped to both halves
            }
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        // CHR-RAM, fixed 8KB
        mapper_helpers::set_chr_8k_fixed(config, chr_mem_, chr_mem_size_, chr_is_ram_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        if (!mode_) {
            // Contra mode: D2-D0 = PRG bank
            prg_bank_ = data & 0x07;
        } else {
            // Multicart mode: D4-D0 = PRG bank, D5 = mirror
            prg_bank_ = data & 0x1F;
            mirror_mode_ = (data & 0x20) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
        }
        return true;
    }
};

} // namespace nes_system
