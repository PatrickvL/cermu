#pragma once
/*
 * mapper_085_vrc7.h — iNES Mapper 085 (Konami VRC7)
 *
 * VRC7 mapper with OPLL-based FM expansion audio.
 *
 * PRG: 3×8KB switchable ($8000,$A000,$C000) + 8KB fixed ($E000).
 * CHR: 8×1KB banks.
 * IRQ: VRC-style 8-bit up-counter with prescaler.
 * Audio: YM2413-compatible FM synthesizer (stub — no audio output).
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

namespace nes_system {

class Mapper085 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t prg_bank_[3] = {};
    uint8_t chr_bank_[8] = {};
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // VRC IRQ
    mapper_helpers::VRCIRQ irq_;

    // OPLL audio registers (stub — stores writes but doesn't synthesize)
    // TODO: Implement YM2413 (OPLL) FM synthesis for audio_tick()/audio_output()
    uint8_t opll_reg_select_ = 0;
    uint8_t opll_regs_[64] = {};

public:
    Mapper085(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        for (int i = 0; i < 3; i++) prg_bank_[i] = 0;
        for (int i = 0; i < 8; i++) chr_bank_[i] = 0;
        mirror_mode_ = header_mirror_;
        irq_.reset();
        opll_reg_select_ = 0;
        for (int i = 0; i < 64; i++) opll_regs_[i] = 0;
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }

    void notify_cpu_cycle() override {
        irq_.tick_cpu();
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

            // OPLL audio (stub)
            case 0x9010: opll_reg_select_ = data; return false;
            case 0x9030: opll_regs_[opll_reg_select_ & 0x3F] = data; return false;

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
