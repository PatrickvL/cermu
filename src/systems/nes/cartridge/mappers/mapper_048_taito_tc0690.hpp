#pragma once
/*
 * mapper_048_taito_tc0690.h — iNES Mapper 048 (Taito TC0690 / TC0190+PAL16R4)
 *
 * Similar layout to mapper 033 (Taito TC0190) but with A12-based IRQ.
 * 2×8KB switchable PRG + 2×8KB fixed + 2×2KB + 4×1KB CHR.
 *
 * Register map:
 *   $8000: PRG bank 0 (8KB at $8000)
 *   $8001: PRG bank 1 (8KB at $A000)
 *   $8002: CHR bank 0 (2KB at $0000)
 *   $8003: CHR bank 1 (2KB at $0800)
 *   $A000: CHR bank 2 (1KB at $1000)
 *   $A001: CHR bank 3 (1KB at $1400)
 *   $A002: CHR bank 4 (1KB at $1800)
 *   $A003: CHR bank 5 (1KB at $1C00)
 *   $C000: IRQ reload value
 *   $C001: IRQ reload + counter reset
 *   $E000: IRQ enable
 *   $E001: IRQ disable + acknowledge
 *   Note: $E000/$E001 are swapped compared to MMC3 convention.
 *
 * Games: Don Doko Don 2, Bakushou!! Jinsei Gekijou 3.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

class Mapper048 : public Mapper {
private:

    uint8_t prg_bank_[2] = {};
    uint8_t chr_bank_2k_[2] = {};
    uint8_t chr_bank_1k_[4] = {};
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // A12-based IRQ (like MMC3, but enable/disable are swapped)
    mapper_helpers::MMC3IRQ irq_;

public:
    Mapper048(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        prg_bank_[0] = 0;
        prg_bank_[1] = 1;
        for (int i = 0; i < 2; i++) chr_bank_2k_[i] = 0;
        for (int i = 0; i < 4; i++) chr_bank_1k_[i] = 0;
        mirror_mode_ = header_mirror_;
        irq_.reset();
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }
    void notify_a12(bool a12_high, uint64_t ppu_cycle) override { irq_.notify_a12(a12_high, ppu_cycle); }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(prg_bank_[0] & 0x3F) % n,
            static_cast<uint32_t>(prg_bank_[1] & 0x3F) % n,
            (n >= 2) ? n - 2 : 0u, n - 1
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t num_1k = mapper_helpers::chr_1k_count(chr_mem_size_);

        // 2×2KB + 4×1KB (no inversion — 2KB at $0000, 1KB at $1000)
        for (int i = 0; i < 2; i++) {
            uint32_t b = (static_cast<uint32_t>(chr_bank_2k_[i]) * 2) % num_1k;
            config.chr_pages[i * 2]     = chr_mem_ + b * 0x0400;
            config.chr_pages[i * 2 + 1] = chr_mem_ + ((b + 1) % num_1k) * 0x0400;
        }
        for (int i = 0; i < 4; i++) {
            uint32_t b = static_cast<uint32_t>(chr_bank_1k_[i]) % num_1k;
            config.chr_pages[4 + i] = chr_mem_ + b * 0x0400;
        }
        for (int i = 0; i < 8; i++)
            config.chr_writable[i] = chr_is_ram_;
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        switch (addr & 0xE003) {
            case 0x8000:
                prg_bank_[0] = data;
                mirror_mode_ = (data & 0x40) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                return true;
            case 0x8001: prg_bank_[1] = data; return true;
            case 0x8002: chr_bank_2k_[0] = data; return true;
            case 0x8003: chr_bank_2k_[1] = data; return true;
            case 0xA000: chr_bank_1k_[0] = data; return true;
            case 0xA001: chr_bank_1k_[1] = data; return true;
            case 0xA002: chr_bank_1k_[2] = data; return true;
            case 0xA003: chr_bank_1k_[3] = data; return true;

            // IRQ — note: $C000/$C001 reload, $E000/$E001 enable/disable (swapped vs MMC3)
            case 0xC000:
                irq_.reload_value = data;
                return false;
            case 0xC001:
                irq_.counter = 0;
                irq_.reload_flag = true;
                return false;
            case 0xE000:
                irq_.enabled = true;
                return false;
            case 0xE001:
                irq_.enabled = false;
                irq_.active = false;
                return false;
        }
        return false;
    }
};

} // namespace nes_system
