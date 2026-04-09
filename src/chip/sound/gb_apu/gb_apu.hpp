#pragma once
/*
 * gb_apu.hpp — Game Boy APU (Audio Processing Unit)
 *
 * The Game Boy APU provides 4 audio channels:
 *   Channel 1: Square wave with sweep (frequency sweep, volume envelope, duty cycle)
 *   Channel 2: Square wave (volume envelope, duty cycle)
 *   Channel 3: Programmable wave (32 4-bit samples)
 *   Channel 4: Noise (LFSR-based, volume envelope)
 *
 * Global: master volume, channel panning (L/R), power control.
 *
 * Register map ($FF10–$FF3F):
 *   $FF10–$FF14: Channel 1 (sweep, duty, volume, frequency)
 *   $FF15–$FF19: Channel 2 (duty, volume, frequency) [$FF15 unused]
 *   $FF1A–$FF1E: Channel 3 (enable, length, output level, frequency)
 *   $FF1F–$FF23: Channel 4 (length, volume, polynomial, trigger)
 *   $FF24: NR50 — Master volume + VIN enable
 *   $FF25: NR51 — Channel panning (L/R per channel)
 *   $FF26: NR52 — Sound on/off + channel status
 *   $FF30–$FF3F: Wave RAM (16 bytes = 32 4-bit samples)
 */

#include "chip/sound/sound_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>

#define GB_APU_DECL(REG, FLD, CMP) \
    /* Channel 1 */ \
    REG(0x00, NR10,  "Ch1 Sweep")                                              \
      FLD(NR10, SWEEP_TIME,  6:4, "Sweep time",        Value, 0, 0)           \
      FLD(NR10, SWEEP_DIR,   3:3, "Sweep direction",   Flag,  0, 0)           \
      FLD(NR10, SWEEP_SHIFT, 2:0, "Sweep shift",       Value, 0, 0)           \
    REG(0x01, NR11,  "Ch1 Duty/Length")                                        \
      FLD(NR11, DUTY,        7:6, "Duty cycle",        Value, 0, 0)           \
      FLD(NR11, LENGTH,      5:0, "Length load",        Value, 0, 0)           \
    REG(0x02, NR12,  "Ch1 Volume Envelope")                                    \
      FLD(NR12, INIT_VOL,    7:4, "Initial volume",    Value, 0, 0)           \
      FLD(NR12, ENV_DIR,     3:3, "Envelope direction", Flag,  0, 0)          \
      FLD(NR12, ENV_PERIOD,  2:0, "Envelope period",   Value, 0, 0)           \
    REG(0x03, NR13,  "Ch1 Frequency low")                                      \
    REG(0x04, NR14,  "Ch1 Frequency high / Trigger")                           \
      FLD(NR14, TRIGGER,     7:7, "Trigger",           Flag,  0, 0)           \
      FLD(NR14, LENGTH_EN,   6:6, "Length enable",      Flag,  0, 0)          \
      FLD(NR14, FREQ_HI,     2:0, "Frequency high bits",Value, 0, 0)         \
    /* Channel 2 */ \
    REG(0x05, NR20,  "Ch2 (unused)")                                           \
    REG(0x06, NR21,  "Ch2 Duty/Length")                                        \
    REG(0x07, NR22,  "Ch2 Volume Envelope")                                    \
    REG(0x08, NR23,  "Ch2 Frequency low")                                      \
    REG(0x09, NR24,  "Ch2 Frequency high / Trigger")                           \
    /* Channel 3 */ \
    REG(0x0A, NR30,  "Ch3 Enable")                                             \
      FLD(NR30, DAC_EN,      7:7, "DAC enable",        Flag,  0, 0)           \
    REG(0x0B, NR31,  "Ch3 Length")                                             \
    REG(0x0C, NR32,  "Ch3 Output Level")                                       \
      FLD(NR32, OUT_LEVEL,   6:5, "Output level",      Value, 0, 0)           \
    REG(0x0D, NR33,  "Ch3 Frequency low")                                      \
    REG(0x0E, NR34,  "Ch3 Frequency high / Trigger")                           \
    /* Channel 4 */ \
    REG(0x0F, NR40,  "Ch4 (unused)")                                           \
    REG(0x10, NR41,  "Ch4 Length")                                             \
    REG(0x11, NR42,  "Ch4 Volume Envelope")                                    \
    REG(0x12, NR43,  "Ch4 Polynomial Counter")                                 \
      FLD(NR43, SHIFT_CLK,   7:4, "Shift clock freq",  Value, 0, 0)           \
      FLD(NR43, WIDTH_MODE,  3:3, "Counter width",     Flag,  0, 0)           \
      FLD(NR43, DIV_RATIO,   2:0, "Dividing ratio",    Value, 0, 0)           \
    REG(0x13, NR44,  "Ch4 Trigger")                                            \
    /* Master control */ \
    REG(0x14, NR50,  "Master Volume")                                          \
    REG(0x15, NR51,  "Channel Panning")                                        \
    REG(0x16, NR52,  "Sound On/Off")                                           \
      FLD(NR52, POWER,       7:7, "All sound on/off",  Flag,  0, 0)

namespace gb_apu {
    namespace reg {
        GB_APU_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 23;
    }
    using namespace reg;

    inline constexpr int WAVE_RAM_SIZE = 16;  // 32 × 4-bit samples
    inline constexpr uint32_t CPU_FREQ = 4194304;  // 4.194304 MHz
}

DECL_EXTRACT(GB_APU, GB_APU_DECL)

// ============================================================================
// Game Boy APU — Stub Implementation
// ============================================================================

struct gb_apu_t : public SoundChipBase {

    gb_apu_t()
        : SoundChipBase(ChipInfo{"SM83_APU", "Sharp", "Game Boy Audio Processing Unit"})
    {
        init_regs(gb_apu::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(GB_APU_REG_INFO);
        register_debug_fields();
#endif
    }

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x3F;
        if (addr < gb_apu::reg::REG_COUNT) {
            BUS_SET_DATA(bus, regs_.data[addr]);
        } else if (addr >= 0x20 && addr < 0x30) {
            // Wave RAM: $FF30–$FF3F
            BUS_SET_DATA(bus, wave_ram_[addr - 0x20]);
        } else {
            BUS_SET_DATA(bus, 0xFF);
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x3F;
        uint8_t data = BUS_GET_DATA(bus);
        if (addr < gb_apu::reg::REG_COUNT) {
            regs_.data[addr] = data;
        } else if (addr >= 0x20 && addr < 0x30) {
            wave_ram_[addr - 0x20] = data;
        }
        return bus;
    }

    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        // TODO: Channel state machines, envelope, sweep, length counter,
        //       noise LFSR, wave playback, mixer, DAC output
        return bus;
    }

    void reset() override {
        std::memset(regs_.data, 0, gb_apu::reg::REG_COUNT);
        std::memset(wave_ram_, 0, sizeof(wave_ram_));
    }

    uint8_t wave_ram_[gb_apu::WAVE_RAM_SIZE] = {};

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("GB APU — Channel 1 (Square+Sweep)")
            .value("NR10 (Sweep)",  +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->regs_.data[gb_apu::NR10]; })
            .value("NR12 (Envelope)", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->regs_.data[gb_apu::NR12]; })
            .category("GB APU — Channel 3 (Wave)")
            .value("NR30 (Enable)", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->regs_.data[gb_apu::NR30]; })
            .category("GB APU — Master")
            .value("NR50 (Volume)", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->regs_.data[gb_apu::NR50]; })
            .value("NR52 (Power)",  +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->regs_.data[gb_apu::NR52]; });
    }
#endif
};
