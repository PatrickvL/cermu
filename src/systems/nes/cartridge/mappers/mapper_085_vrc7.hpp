#pragma once
/*
 * mapper_085_vrc7.h — iNES Mapper 085 (Konami VRC7)
 *
 * VRC7 mapper with OPLL-based FM expansion audio.
 *
 * PRG: 3×8KB switchable ($8000,$A000,$C000) + 8KB fixed ($E000).
 * CHR: 8×1KB banks.
 * IRQ: VRC-style 8-bit up-counter with prescaler.
 * Audio: 6-channel 2-operator FM synthesizer (YM2413 OPLL subset)
 *        with 15 Konami custom ROM patches + 1 user patch.
 *
 * Register map:
 *   $8000: PRG bank 0
 *   $8010: PRG bank 1
 *   $9000: PRG bank 2
 *   $9010: OPLL register select (audio)
 *   $9030: OPLL data write (audio)
 *   $A000-$D010: CHR banks 0-7
 *   $E000: mirroring + silence
 *   $E010: IRQ latch
 *   $F000: IRQ control
 *   $F010: IRQ acknowledge
 *
 * Games: Lagrange Point (only licensed VRC7 game).
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include "chip/sound/ym_fm/ym_fm.hpp"

namespace nes_system {

// VRC7 traits — stripped-down OPLL with 6 channels, no rhythm
inline constexpr YMTraits VRC7_Traits = {
    .vendor               = "Konami",
    .chip_id              = "VRC7",
    .display_name         = "Konami VRC7 (OPLL)",
    .fm_channels          = 6,
    .operators_per_channel = 2,
    .fm_algorithms        = 4,
    .has_ch3_special_mode = false,
    .has_lfo              = false,
    .has_rhythm_mode      = false,
    .has_waveform_select  = false,
    .has_rom_patches      = true,
    .has_ssg              = false,
    .ssg                  = nullptr,
    .has_adpcm_a          = false,
    .has_adpcm_b          = false,
    .has_dac              = false,
    .ladder_effect        = false,
    .pin_count            = 24,
};

class Mapper085 : public Mapper {
private:

    uint8_t prg_bank_[3] = {};
    uint8_t chr_bank_[8] = {};
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // VRC IRQ
    mapper_helpers::VRCIRQ irq_;

    // VRC7 FM expansion audio — 6-channel OPLL with Konami custom patches
    ym_fm_t<VRC7_Traits> fm_;
    uint8_t fm_reg_select_ = 0;
    uint16_t fm_divider_ = 0;

public:
    Mapper085(uint8_t /*prgBanks*/, uint8_t /*chrBanks*/) {
        fm_.set_opll_rom_patches(opll_patches::VRC7_ROM);
    }

    void reset() override {
        for (int i = 0; i < 3; i++) prg_bank_[i] = 0;
        for (int i = 0; i < 8; i++) chr_bank_[i] = 0;
        mirror_mode_ = header_mirror_;
        irq_.reset();
        fm_.reset();
        fm_.set_opll_rom_patches(opll_patches::VRC7_ROM);
        fm_reg_select_ = 0;
        fm_divider_ = 0;
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }

    void notify_cpu_cycle() override {
        irq_.tick_cpu();
    }

    // VRC7 FM expansion audio — OPLL master clock ≈ 3.58 MHz (2× CPU)
    // The OPLL internally divides by 72 for sample generation, so the
    // effective sample rate is ~49.7 kHz.  We tick the FM engine at the
    // CPU clock rate and let its internal prescaler handle the rest.
    void audio_tick() override {
        // The VRC7's OPLL runs at ~3.579545 MHz (NTSC master / 5, same as CPU×2).
        // Tick twice per CPU cycle to approximate the 2× clock relationship.
        bus_state_t dummy = 0;
        fm_.tick(dummy);
        fm_.tick(dummy);
    }

    float audio_output() const override {
        return fm_.get_sample();
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        using namespace mapper_helpers;
        uint32_t n = prg_8k_count(prg_rom_size_);
        uint32_t banks[4] = {
            static_cast<uint32_t>(prg_bank_[0]) % n,
            static_cast<uint32_t>(prg_bank_[1]) % n,
            static_cast<uint32_t>(prg_bank_[2]) % n,
            n - 1
        };
        set_prg_8k_banks(config, prg_rom_, prg_rom_size_, banks);

        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_1k_pages(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        // VRC7 uses A4 as secondary address line ($x000 vs $x010)
        uint16_t reg = (addr & 0xF000) | ((addr & 0x0010) ? 0x10 : 0x00);

        switch (reg) {
            case 0x8000: prg_bank_[0] = data & 0x3F; return true;
            case 0x8010: prg_bank_[1] = data & 0x3F; return true;
            case 0x9000: prg_bank_[2] = data & 0x3F; return true;

            // OPLL FM audio
            case 0x9010: fm_reg_select_ = data; return false;
            case 0x9030:
                fm_.write_register(fm_reg_select_, data);
                return false;

            case 0xA000: chr_bank_[0] = data; return true;
            case 0xA010: chr_bank_[1] = data; return true;
            case 0xB000: chr_bank_[2] = data; return true;
            case 0xB010: chr_bank_[3] = data; return true;
            case 0xC000: chr_bank_[4] = data; return true;
            case 0xC010: chr_bank_[5] = data; return true;
            case 0xD000: chr_bank_[6] = data; return true;
            case 0xD010: chr_bank_[7] = data; return true;

            case 0xE000:
                mirror_mode_ = mapper_helpers::mirror_from_2bit(data);
                return true;

            case 0xE010: irq_.latch = data; return false;  // latch (full byte, not nybble-split)
            case 0xF000: irq_.write(2, data); return false;  // control
            case 0xF010: irq_.write(3, data); return false;  // ack
        }
        return false;
    }
};

} // namespace nes_system
