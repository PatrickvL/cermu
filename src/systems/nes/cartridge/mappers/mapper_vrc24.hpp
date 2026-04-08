#pragma once
/*
 * mapper_vrc24.h — Shared Konami VRC2/VRC4 implementation
 *
 * Template-based mapper covering all VRC2 and VRC4 variants.
 * Parameterized by a Traits struct that specifies:
 *   - Address-line wiring (a0_mask / a1_mask)
 *   - CHR bank value shift (chr_shift)
 *   - Whether the VRC4 IRQ counter is present (has_irq)
 *
 * Mapper / Traits mapping:
 *   021 (VRC4a/c) : VRC4aTraits    {A0←A1|A6, A1←A2|A7, shift=0, irq}
 *   022 (VRC2a)   : VRC2aTraits    {A0←A1,    A1←A0,    shift=1, no irq}
 *   023 (VRC2b/4e): VRC24_023Traits{A0←A0|A2, A1←A1|A3, shift=0, irq}
 *   025 (VRC2c/4b): VRC24_025Traits{A0←A1|A3, A1←A0|A2, shift=0, irq}
 *
 * Because iNES mapper numbers combine VRC2 + VRC4 sub-variants,
 * IRQ is enabled for all 021/023/025 (harmless if not written).
 *
 * VRC4 IRQ: 8-bit up-counter with latch, scanline / cycle mode.
 * Scanline mode uses A12 rising edges; cycle mode is approximate.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"

namespace nes_system {

// ============================================================================
// VRC2 / VRC4 Traits
// ============================================================================

/// Mapper 021 — Konami VRC4a / VRC4c
/// Games: Wai Wai World 2, Ganbare Goemon Gaiden 2.
struct VRC4aTraits {
    static constexpr uint16_t a0_mask   = 0x0042;  // CPU A1 | A6
    static constexpr uint16_t a1_mask   = 0x0084;  // CPU A2 | A7
    static constexpr uint8_t  chr_shift = 0;
    static constexpr bool     has_irq   = true;
};

/// Mapper 022 — Konami VRC2a
/// Games: Twinbee 3, Ganbare Goemon, etc.
struct VRC2aTraits {
    static constexpr uint16_t a0_mask   = 0x0002;  // CPU A1
    static constexpr uint16_t a1_mask   = 0x0001;  // CPU A0
    static constexpr uint8_t  chr_shift = 1;        // CHR banks >> 1
    static constexpr bool     has_irq   = false;
};

/// Mapper 023 — Konami VRC2b / VRC4e / VRC4f
/// Games: Wai Wai World, Crisis Force, Parodius Da!
struct VRC24_023Traits {
    static constexpr uint16_t a0_mask   = 0x0005;  // CPU A0 | A2
    static constexpr uint16_t a1_mask   = 0x000A;  // CPU A1 | A3
    static constexpr uint8_t  chr_shift = 0;
    static constexpr bool     has_irq   = true;
};

/// Mapper 025 — Konami VRC2c / VRC4b / VRC4d
/// Games: Gradius II, Bio Miracle Bokutte Upa.
struct VRC24_025Traits {
    static constexpr uint16_t a0_mask   = 0x000A;  // CPU A1 | A3
    static constexpr uint16_t a1_mask   = 0x0005;  // CPU A0 | A2
    static constexpr uint8_t  chr_shift = 0;
    static constexpr bool     has_irq   = true;
};

// ============================================================================
// VRC2/VRC4 Mapper Template
// ============================================================================

template<typename Traits>
class MapperVRC24 : public Mapper {
private:
    uint8_t prg_bank_0_ = 0;    // $8000 (or $C000 in swap mode)
    uint8_t prg_bank_1_ = 0;    // $A000
    bool prg_swap_mode_ = false; // VRC4: swap $8000/$C000

    uint8_t chr_reg_[8] = {};    // 8 × 1KB CHR bank registers (lo+hi nybble)
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // VRC4 IRQ (compiled out for VRC2 via if constexpr)
    mapper_helpers::VRCIRQ irq_;

    /// Remap CPU address bits to VRC internal A0/A1.
    static uint16_t decode_addr(uint16_t addr) {
        uint8_t vrc_a0 = (addr & Traits::a0_mask) ? 1 : 0;
        uint8_t vrc_a1 = (addr & Traits::a1_mask) ? 1 : 0;
        return (addr & 0xF000) | (vrc_a1 << 1) | vrc_a0;
    }

    bool write_chr_reg(uint16_t reg, uint8_t data, int base_idx) {
        switch (reg & 0x0003) {
            case 0: chr_reg_[base_idx]     = (chr_reg_[base_idx]     & 0xF0) | (data & 0x0F); return true;
            case 1: chr_reg_[base_idx]     = (chr_reg_[base_idx]     & 0x0F) | ((data & 0x0F) << 4); return true;
            case 2: chr_reg_[base_idx + 1] = (chr_reg_[base_idx + 1] & 0xF0) | (data & 0x0F); return true;
            case 3: chr_reg_[base_idx + 1] = (chr_reg_[base_idx + 1] & 0x0F) | ((data & 0x0F) << 4); return true;
            default: return false;
        }
    }

public:
    MapperVRC24(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {}

    void reset() override {
        prg_bank_0_ = 0;
        prg_bank_1_ = 0;
        prg_swap_mode_ = false;
        for (int i = 0; i < 8; i++) chr_reg_[i] = 0;
        mirror_mode_ = Mirror::VERTICAL;
        if constexpr (Traits::has_irq) irq_.reset();
    }

    Mirror mirror() override { return mirror_mode_; }

    bool irq_state() override {
        if constexpr (Traits::has_irq) return irq_.active;
        else return false;
    }

    void irq_clear() override {
        if constexpr (Traits::has_irq) irq_.active = false;
    }

    void notify_cpu_cycle() override {
        if constexpr (Traits::has_irq) irq_.tick_cpu();
    }

    void notify_a12(bool a12_high, uint64_t /*ppu_cycle*/) override {
        if constexpr (Traits::has_irq) {
            if (a12_high) irq_.clock_scanline();
        }
    }

    // =======================================================================
    // Bank configuration
    // =======================================================================

    void get_prg_bank_config(MapperBankConfig& config) const override {
        uint32_t total_8k = static_cast<uint32_t>(prg_rom_size_ / 0x2000);
        if (total_8k == 0) total_8k = 1;

        uint32_t b0 = prg_bank_0_ % total_8k;
        uint32_t b1 = prg_bank_1_ % total_8k;
        uint32_t fixed_lo = (total_8k >= 2) ? (total_8k - 2) : 0;
        uint32_t fixed_hi = total_8k - 1;

        uint32_t banks[4];
        if constexpr (Traits::has_irq) {
            if (!prg_swap_mode_) {
                banks[0] = b0; banks[1] = b1; banks[2] = fixed_lo; banks[3] = fixed_hi;
            } else {
                banks[0] = fixed_lo; banks[1] = b1; banks[2] = b0; banks[3] = fixed_hi;
            }
        } else {
            banks[0] = b0; banks[1] = b1; banks[2] = fixed_lo; banks[3] = fixed_hi;
        }

        for (int slot = 0; slot < 4; slot++) {
            uint32_t base = banks[slot] * 0x2000;
            config.prg_pages[slot * 2]     = (base < prg_rom_size_) ? prg_rom_ + base : nullptr;
            config.prg_pages[slot * 2 + 1] = (base + 0x1000 < prg_rom_size_)
                                                  ? prg_rom_ + base + 0x1000 : nullptr;
        }
        config.prg_ram_enabled = false;
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        uint32_t chr_1k = (chr_mem_size_ > 0)
                              ? static_cast<uint32_t>(chr_mem_size_ / 0x0400) : 1;
        if (chr_1k == 0) chr_1k = 1;

        for (int i = 0; i < 8; i++) {
            uint32_t bank = chr_reg_[i];
            if constexpr (Traits::chr_shift > 0) bank >>= Traits::chr_shift;
            bank %= chr_1k;
            uint32_t offset = bank * 0x0400;
            config.chr_pages[i] = (offset < chr_mem_size_) ? chr_mem_ + offset : chr_mem_;
            config.chr_writable[i] = chr_is_ram_;
        }
    }

    // =======================================================================
    // Register writes
    // =======================================================================

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        uint16_t decoded = decode_addr(addr);
        uint16_t reg = decoded & 0xF003;

        switch (reg & 0xF000) {
            case 0x8000:  // PRG bank 0
                prg_bank_0_ = data & 0x1F;
                return true;

            case 0x9000:
                if constexpr (Traits::has_irq) {
                    // VRC4: $9000/$9001 = mirroring, $9002/$9003 = PRG swap
                    if ((reg & 0x0003) <= 1) {
                        switch (data & 0x03) {
                            case 0: mirror_mode_ = Mirror::VERTICAL;     break;
                            case 1: mirror_mode_ = Mirror::HORIZONTAL;   break;
                            case 2: mirror_mode_ = Mirror::ONESCREEN_LO; break;
                            case 3: mirror_mode_ = Mirror::ONESCREEN_HI; break;
                        }
                    } else {
                        prg_swap_mode_ = (data & 0x02) != 0;
                    }
                } else {
                    // VRC2: all $9000–$9003 = mirroring
                    switch (data & 0x03) {
                        case 0: mirror_mode_ = Mirror::VERTICAL;     break;
                        case 1: mirror_mode_ = Mirror::HORIZONTAL;   break;
                        case 2: mirror_mode_ = Mirror::ONESCREEN_LO; break;
                        case 3: mirror_mode_ = Mirror::ONESCREEN_HI; break;
                    }
                }
                return true;

            case 0xA000:  // PRG bank 1
                prg_bank_1_ = data & 0x1F;
                return true;

            case 0xB000: return write_chr_reg(reg, data, 0);
            case 0xC000: return write_chr_reg(reg, data, 2);
            case 0xD000: return write_chr_reg(reg, data, 4);
            case 0xE000: return write_chr_reg(reg, data, 6);

            case 0xF000:
                if constexpr (Traits::has_irq) return irq_.write(reg & 0x03, data);
                return false;

            default:
                return false;
        }
    }
};

} // namespace nes_system
