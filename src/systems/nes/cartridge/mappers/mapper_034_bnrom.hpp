#pragma once
/*
 * mapper_034_bnrom.h — iNES Mapper 034 (BNROM / NINA-001)
 *
 * Two board variants share this mapper number:
 *
 * BNROM: 32KB PRG bank switching via writes to $8000-$FFFF.
 *        No CHR banking (8KB CHR-RAM). No PRG-RAM.
 *        Games: Deadly Towers, Haunted: Halloween '85
 *
 * NINA-001: PRG 32KB + CHR 4KB×2 via $7FFD-$7FFF.
 *        Games: Impossible Mission II, Death Race
 *
 * iNES disambiguation:
 *   - If CHR banks == 0 (CHR-RAM)   → BNROM behavior
 *   - If CHR banks > 0  (CHR-ROM)   → NINA-001 behavior
 */

#include "systems/nes/cartridge/nes_mapper.hpp"
#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper034 : public Mapper {
private:
    uint8_t prg_bank_select_ = 0;
    uint8_t chr_bank_lo_ = 0;    // NINA-001: CHR $0000-$0FFF
    uint8_t chr_bank_hi_ = 0;    // NINA-001: CHR $1000-$1FFF
    bool is_nina001_ = false;

public:
    Mapper034(uint8_t /*prgBanks*/, uint8_t chrBanks)
        : is_nina001_(chrBanks > 0) {}

    void reset() override {
        prg_bank_select_ = 0;
        chr_bank_lo_ = 0;
        chr_bank_hi_ = 0;
    }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        mapper_helpers::set_prg_32k(config, prg_rom_, prg_rom_size_, prg_bank_select_);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        if (is_nina001_) {
            // NINA-001: two 4KB CHR banks
            uint32_t max_4k = (chr_mem_size_ > 0) ? static_cast<uint32_t>(chr_mem_size_ / 0x1000) : 1;
            uint32_t lo = (max_4k > 0) ? (chr_bank_lo_ % max_4k) : 0;
            uint32_t hi = (max_4k > 0) ? (chr_bank_hi_ % max_4k) : 0;
            for (int i = 0; i < 4; i++) {
                uint32_t offset = lo * 0x1000 + i * 0x0400;
                config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                config.chr_writable[i] = false;
            }
            for (int i = 0; i < 4; i++) {
                uint32_t offset = hi * 0x1000 + i * 0x0400;
                config.chr_pages[4 + i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                config.chr_writable[4 + i] = false;
            }
        } else {
            // BNROM: 8KB CHR-RAM (no switching)
            for (int i = 0; i < 8; i++) {
                uint32_t offset = i * 0x0400;
                config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
                config.chr_writable[i] = true;  // CHR-RAM
            }
        }
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (is_nina001_) {
            // NINA-001: registers at $7FFD-$7FFF only
            if (addr == 0x7FFD) {
                prg_bank_select_ = data;
                return true;
            } else if (addr == 0x7FFE) {
                chr_bank_lo_ = data;
                return true;
            } else if (addr == 0x7FFF) {
                chr_bank_hi_ = data;
                return true;
            }
            return false;  // NINA-001 ignores $8000+ writes
        }

        // BNROM: any write to $8000-$FFFF selects PRG bank
        if (addr >= 0x8000) {
            data = mapper_helpers::apply_bus_conflict(data, prg_rom_, prg_rom_size_, addr);
            prg_bank_select_ = data;
            return true;
        }
        return false;
    }
};

} // namespace nes_system
