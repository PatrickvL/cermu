#pragma once
/*
 * mapper_vrc6.h — iNES Mappers 024/026 (Konami VRC6a/VRC6b)
 *
 * VRC6 mapper with expansion audio (2 pulse + 1 sawtooth channels).
 *
 * PRG: 16KB switchable ($8000) + 8KB switchable ($C000) + 8KB fixed ($E000).
 * CHR: 8×1KB banks.
 * IRQ: VRC-style 8-bit up-counter with latch/prescaler.
 * Audio: 2 pulse channels (variable duty) + 1 sawtooth accumulator.
 *
 * Mapper 024 (VRC6a): A0=A0, A1=A1 (Akumajou Densetsu / Castlevania III JP)
 * Mapper 026 (VRC6b): A0=A1, A1=A0 (Esper Dream 2, Madara)
 *
 * Register map (after address line remapping):
 *   $8000-$8003: PRG bank 0 (16KB at $8000)
 *   $9000-$9002: Pulse 1 audio registers
 *   $A000-$A002: Pulse 2 audio registers
 *   $B000-$B002: Sawtooth audio registers
 *   $B003: mirroring control
 *   $C000-$C003: PRG bank 1 (8KB at $C000)
 *   $D000-$D003: CHR banks 0-3 (1KB each)
 *   $E000-$E003: CHR banks 4-7 (1KB each)
 *   $F000-$F002: IRQ latch/control/ack
 *
 * Games: Akumajou Densetsu, Esper Dream 2, Madara.
 */

#include "systems/nes/cartridge/mappers/mapper_helpers.hpp"
#include <cmath>

namespace nes_system {

// VRC6a: A0=A0, A1=A1; VRC6b: A0↔A1 swapped
struct VRC6aTraits {
    static uint16_t remap(uint16_t addr) { return addr; }
};
struct VRC6bTraits {
    static uint16_t remap(uint16_t addr) {
        // Swap A0 and A1
        uint16_t a0 = (addr >> 0) & 1;
        uint16_t a1 = (addr >> 1) & 1;
        return (addr & 0xFFFC) | (a0 << 1) | (a1 << 0);
    }
};

template<typename Traits>
class MapperVRC6 : public Mapper {
private:
    uint8_t prg_banks_;
    uint8_t chr_banks_;

    uint8_t prg_bank_16k_ = 0;   // $8000 (16KB)
    uint8_t prg_bank_8k_ = 0;    // $C000 (8KB)
    uint8_t chr_bank_[8] = {};
    Mirror mirror_mode_ = Mirror::VERTICAL;

    // VRC IRQ
    mapper_helpers::VRCIRQ irq_;

    // ========================================================================
    // Expansion Audio: 2 pulse + 1 sawtooth
    // ========================================================================

    // Pulse channel state
    struct Pulse {
        uint8_t volume = 0;      // 4-bit volume
        uint8_t duty = 0;        // 3-bit duty cycle (0-7)
        bool mode = false;       // true = direct volume (ignore duty)
        bool enabled = false;
        uint16_t period = 0;     // 12-bit timer period
        uint16_t timer = 0;      // current countdown
        uint8_t step = 0;        // 16-step phase counter

        void write(uint8_t reg, uint8_t data) {
            switch (reg) {
                case 0:
                    volume = data & 0x0F;
                    duty = (data >> 4) & 0x07;
                    mode = (data & 0x80) != 0;
                    break;
                case 1:
                    period = (period & 0x0F00) | data;
                    break;
                case 2:
                    period = (period & 0x00FF) | ((data & 0x0F) << 8);
                    enabled = (data & 0x80) != 0;
                    break;
            }
        }

        void tick() {
            if (!enabled) return;
            if (timer == 0) {
                timer = period;
                step = (step + 1) & 0x0F;
            } else {
                timer--;
            }
        }

        float output() const {
            if (!enabled) return 0.0f;
            if (mode) return static_cast<float>(volume) / 15.0f;
            // Duty cycle: output high when step <= duty
            return (step <= duty) ? static_cast<float>(volume) / 15.0f : 0.0f;
        }
    };

    // Sawtooth channel state
    struct Sawtooth {
        uint8_t rate = 0;        // 6-bit accumulator rate
        bool enabled = false;
        uint16_t period = 0;     // 12-bit timer period
        uint16_t timer = 0;
        uint8_t accumulator = 0; // 8-bit accumulator
        uint8_t step = 0;        // counts 0-13 (7 additions, reset on 14th)

        void write(uint8_t reg, uint8_t data) {
            switch (reg) {
                case 0: rate = data & 0x3F; break;
                case 1: period = (period & 0x0F00) | data; break;
                case 2:
                    period = (period & 0x00FF) | ((data & 0x0F) << 8);
                    enabled = (data & 0x80) != 0;
                    if (!enabled) { accumulator = 0; step = 0; }
                    break;
            }
        }

        void tick() {
            if (!enabled) return;
            if (timer == 0) {
                timer = period;
                step++;
                if (step >= 14) {
                    step = 0;
                    accumulator = 0;
                } else if ((step & 1) == 0) {
                    // Add rate every 2 clocks
                    accumulator += rate;
                }
            } else {
                timer--;
            }
        }

        float output() const {
            if (!enabled) return 0.0f;
            // Output is upper 5 bits of accumulator
            return static_cast<float>((accumulator >> 3) & 0x1F) / 31.0f;
        }
    };

    Pulse pulse_[2];
    Sawtooth saw_;

public:
    MapperVRC6(uint8_t prgBanks, uint8_t chrBanks)
        : prg_banks_(prgBanks), chr_banks_(chrBanks) {}

    void reset() override {
        prg_bank_16k_ = 0;
        prg_bank_8k_ = 0;
        for (int i = 0; i < 8; i++) chr_bank_[i] = 0;
        mirror_mode_ = header_mirror_;
        irq_.reset();
        pulse_[0] = {};
        pulse_[1] = {};
        saw_ = {};
    }

    Mirror mirror() override { return mirror_mode_; }
    bool irq_state() override { return irq_.active; }
    void irq_clear() override { irq_.active = false; }

    void notify_cpu_cycle() override {
        irq_.tick_cpu();
    }

    void notify_a12(bool a12_high, uint64_t /*ppu_cycle*/) override {
        if (a12_high) irq_.clock_scanline();
    }

    // Expansion audio
    void audio_tick() override {
        pulse_[0].tick();
        pulse_[1].tick();
        saw_.tick();
    }

    float audio_output() const override {
        // Mix expansion channels — normalize to [-1, 1] range
        float sum = pulse_[0].output() + pulse_[1].output() + saw_.output();
        return sum / 3.0f;
    }

    void get_prg_bank_config(MapperBankConfig& config) const override {
        // $8000-$BFFF: 16KB switchable
        // $C000-$DFFF: 8KB switchable
        // $E000-$FFFF: 8KB fixed (last)
        using namespace mapper_helpers;
        uint32_t n16 = static_cast<uint32_t>(prg_rom_size_ / 0x4000);
        if (n16 == 0) n16 = 1;
        uint32_t n8 = prg_8k_count(prg_rom_size_);

        uint32_t lo = (prg_bank_16k_ % n16) * 0x4000;
        for (int i = 0; i < 4; i++)
            config.prg_pages[i] = prg_rom_ + lo + i * 0x1000;

        uint32_t mid = (prg_bank_8k_ % n8) * 0x2000;
        config.prg_pages[4] = prg_rom_ + mid;
        config.prg_pages[5] = prg_rom_ + mid + 0x1000;

        uint32_t hi = (n8 - 1) * 0x2000;
        config.prg_pages[6] = prg_rom_ + hi;
        config.prg_pages[7] = prg_rom_ + hi + 0x1000;

        config.prg_ram_base = prg_ram_;
        config.prg_ram_size = static_cast<uint32_t>(prg_ram_size_);
        config.prg_ram_enabled = (prg_ram_ != nullptr);
    }

    void get_chr_bank_config(MapperChrConfig& config) const override {
        mapper_helpers::set_chr_1k_pages(config, chr_mem_, chr_mem_size_, chr_is_ram_, chr_bank_);
    }

    bool register_write(uint16_t addr, uint8_t data) override {
        if (addr < 0x8000) return false;

        uint16_t remapped = Traits::remap(addr);
        uint16_t reg = remapped & 0xF003;

        switch (reg & 0xF000) {
            case 0x8000:
                prg_bank_16k_ = data & 0x0F;
                return true;

            case 0x9000:
                pulse_[0].write(reg & 0x03, data);
                return false;

            case 0xA000:
                pulse_[1].write(reg & 0x03, data);
                return false;

            case 0xB000:
                if ((reg & 0x03) == 3) {
                    // $B003: mirroring + PPU banking control
                    mirror_mode_ = mapper_helpers::mirror_from_2bit((data >> 2) & 0x03);
                    return true;
                }
                saw_.write(reg & 0x03, data);
                return false;

            case 0xC000:
                prg_bank_8k_ = data & 0x1F;
                return true;

            case 0xD000:
                chr_bank_[reg & 0x03] = data;
                return true;

            case 0xE000:
                chr_bank_[4 + (reg & 0x03)] = data;
                return true;

            case 0xF000:
                irq_.write(reg & 0x03, data);
                return false;
        }
        return false;
    }
};

// Type aliases
using Mapper024 = MapperVRC6<VRC6aTraits>;
using Mapper026 = MapperVRC6<VRC6bTraits>;

} // namespace nes_system
