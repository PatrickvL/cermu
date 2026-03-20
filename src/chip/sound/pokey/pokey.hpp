#pragma once
/*
 * pokey.hpp — Atari POKEY (C012294) sound + I/O chip
 *
 * The POKEY is a custom LSI chip designed by Doug Neubauer at Atari in
 * 1979.  It provides:
 *   - 4-channel audio synthesis (square waves, polynomial noise)
 *   - Timer/counter interrupts
 *   - Serial I/O (keyboard, cassette)
 *   - Paddle (pot) inputs
 *   - Random number generator (17/9-bit LFSR)
 *
 * Used in: Asteroids Deluxe, Tempest, Centipede, Missile Command,
 *          Atari 400/800/XL/XE home computers, and many other systems.
 *
 * Audio architecture:
 *   Each channel has:
 *     - AUDFn: 8-bit frequency divider reload value
 *     - AUDCn: control (4-bit volume + poly/noise select + force-high)
 *   Base clock: 64 KHz (1.79 MHz / 28) or 15 KHz (1.79 MHz / 114)
 *   Channels can be linked for 16-bit resolution (1+2 or 3+4)
 *   Polynomial counters: 4-bit, 5-bit, 9-bit, 17-bit LFSR
 *
 * Register map (accent addresses for arcade use):
 *   Write registers ($x0-$x8):
 *     $00 AUDF1    Channel 1 frequency
 *     $01 AUDC1    Channel 1 control
 *     $02 AUDF2    Channel 2 frequency
 *     $03 AUDC2    Channel 2 control
 *     $04 AUDF3    Channel 3 frequency
 *     $05 AUDC3    Channel 3 control
 *     $06 AUDF4    Channel 4 frequency
 *     $07 AUDC4    Channel 4 control
 *     $08 AUDCTL   Audio control (clock select, channel linking, filters)
 *     $09 STIMER   Start timers
 *     $0A SKREST   Reset serial status
 *     $0B POTGO    Start pot scan
 *     $0D SEROUT   Serial output data
 *     $0E IRQEN    IRQ enable
 *     $0F SKCTL    Serial port control
 *
 *   Read registers ($x0-$x0F):
 *     $00-$07 POT0-POT7  Pot counter values
 *     $08 ALLPOT   Pot port status
 *     $09 KBCODE   Keyboard code
 *     $0A RANDOM   Random number (LFSR output)
 *     $0D SERIN    Serial input data
 *     $0E IRQST    IRQ status
 *     $0F SKSTAT   Serial port status
 *
 * Clock domain:
 *   In arcade systems the POKEY is typically clocked at 1.789773 MHz
 *   (Asteroids Deluxe) or at the CPU clock rate.  The audio dividers
 *   use an internal fast clock of clock/28 (~64 KHz) or clock/114
 *   (~15 KHz) selected by AUDCTL.  Some arcade boards feed the POKEY
 *   at 1.512 MHz; the audio dividers still function identically.
 */

#include "chip/sound/sound_chip_base.hpp"
#include "core/signal/audio_port.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// POKEY CONSTANTS
// ============================================================================

namespace pokey_constants {

    // Write registers
    inline constexpr uint8_t AUDF1   = 0x00;
    inline constexpr uint8_t AUDC1   = 0x01;
    inline constexpr uint8_t AUDF2   = 0x02;
    inline constexpr uint8_t AUDC2   = 0x03;
    inline constexpr uint8_t AUDF3   = 0x04;
    inline constexpr uint8_t AUDC3   = 0x05;
    inline constexpr uint8_t AUDF4   = 0x06;
    inline constexpr uint8_t AUDC4   = 0x07;
    inline constexpr uint8_t AUDCTL  = 0x08;
    inline constexpr uint8_t STIMER  = 0x09;
    inline constexpr uint8_t SKREST  = 0x0A;
    inline constexpr uint8_t POTGO   = 0x0B;
    inline constexpr uint8_t SEROUT  = 0x0D;
    inline constexpr uint8_t IRQEN   = 0x0E;
    inline constexpr uint8_t SKCTL   = 0x0F;

    // Read registers
    inline constexpr uint8_t POT0    = 0x00;
    inline constexpr uint8_t ALLPOT  = 0x08;
    inline constexpr uint8_t KBCODE  = 0x09;
    inline constexpr uint8_t RANDOM  = 0x0A;
    inline constexpr uint8_t SERIN   = 0x0D;
    inline constexpr uint8_t IRQST   = 0x0E;
    inline constexpr uint8_t SKSTAT  = 0x0F;

    // AUDCTL bits
    inline constexpr uint8_t AUDCTL_POLY9        = 0x80;  // 1 = 9-bit poly (vs 17-bit)
    inline constexpr uint8_t AUDCTL_CH1_179MHZ   = 0x40;  // 1 = ch1 uses 1.79 MHz clock
    inline constexpr uint8_t AUDCTL_CH3_179MHZ   = 0x20;  // 1 = ch3 uses 1.79 MHz clock
    inline constexpr uint8_t AUDCTL_CH12_LINKED  = 0x10;  // 1 = ch1+2 linked as 16-bit
    inline constexpr uint8_t AUDCTL_CH34_LINKED  = 0x08;  // 1 = ch3+4 linked as 16-bit
    inline constexpr uint8_t AUDCTL_HIPASS_CH1   = 0x04;  // 1 = ch1 high-pass filtered by ch3
    inline constexpr uint8_t AUDCTL_HIPASS_CH2   = 0x02;  // 1 = ch2 high-pass filtered by ch4
    inline constexpr uint8_t AUDCTL_15KHZ        = 0x01;  // 1 = use 15 KHz base (vs 64 KHz)

    // AUDCn bits
    inline constexpr uint8_t AUDC_VOLUME_MASK    = 0x0F;  // bits 0-3: volume (0-15)
    inline constexpr uint8_t AUDC_VOLUME_ONLY    = 0x10;  // bit 4: force output high (DAC mode)
    inline constexpr uint8_t AUDC_POLY_MASK      = 0xE0;  // bits 5-7: noise/tone select

    // AUDCn poly select values (bits 7:5)
    inline constexpr uint8_t AUDC_POLY_5_17      = 0x00;  // 5+17 bit poly
    inline constexpr uint8_t AUDC_POLY_5         = 0x20;  // 5 bit poly only
    inline constexpr uint8_t AUDC_POLY_5_4       = 0x40;  // 5+4 bit poly
    inline constexpr uint8_t AUDC_POLY_5_TONE    = 0x60;  // 5 bit poly + pure tone
    inline constexpr uint8_t AUDC_POLY_17        = 0x80;  // 17 bit poly (no 5-bit gate)
    inline constexpr uint8_t AUDC_TONE           = 0xA0;  // pure tone (square wave)
    inline constexpr uint8_t AUDC_POLY_4         = 0xC0;  // 4 bit poly (no 5-bit gate)
    inline constexpr uint8_t AUDC_TONE_ALT       = 0xE0;  // pure tone (alternate encoding)

    // Base clock dividers (from master clock)
    inline constexpr uint8_t DIV_64KHZ  = 28;   // ~63.9 KHz from 1.789773 MHz
    inline constexpr uint8_t DIV_15KHZ  = 114;  // ~15.7 KHz from 1.789773 MHz

    // LFSR polynomials
    inline constexpr uint32_t POLY4_MASK  = 0x000F;
    inline constexpr uint32_t POLY5_MASK  = 0x001F;
    inline constexpr uint32_t POLY9_MASK  = 0x01FF;
    inline constexpr uint32_t POLY17_MASK = 0x1FFFF;

}  // namespace pokey_constants

// ============================================================================
// POKEY AUDIO CHANNEL
// ============================================================================

struct PokeyChannel {
    uint8_t  audf     = 0;        // Frequency divider reload (AUDFn)
    uint8_t  audc     = 0;        // Control register (AUDCn)
    uint16_t counter  = 0;        // Current divider countdown
    bool     output   = false;    // Current channel output state (high/low)

    /// Reload the frequency divider to its programmed value.
    void reload() { counter = audf; }

    /// Get volume (0-15) from AUDCn.
    uint8_t volume() const { return audc & pokey_constants::AUDC_VOLUME_MASK; }

    /// Is this channel in forced-output (volume-only / DAC) mode?
    bool is_volume_only() const { return (audc & pokey_constants::AUDC_VOLUME_ONLY) != 0; }

    /// Get poly select bits (bits 7:5).
    uint8_t poly_select() const { return audc & pokey_constants::AUDC_POLY_MASK; }
};

// ============================================================================
// POKEY CHIP
// ============================================================================

struct pokey_t : public SoundChipBase {

    pokey_t() : SoundChipBase(ChipInfo{"POKEY", "Atari", "C012294"}) {}

    // ========================================================================
    // Initialization
    // ========================================================================

    void init() {
        reset();
        init_poly_tables();
    }

    void reset() {
        for (auto& ch : channel_) {
            ch.audf = 0;
            ch.audc = 0;
            ch.counter = 0;
            ch.output = false;
        }
        audctl_   = 0;
        irqen_    = 0;
        irqst_    = 0xFF;   // No pending IRQs (active-low)
        skctl_    = 0;
        skstat_   = 0xFF;   // No errors
        poly4_pos_  = 0;
        poly5_pos_  = 0;
        poly9_pos_  = 0;
        poly17_pos_ = 0;
        div_counter_ = 0;
        random_      = 0xFFFF;
    }

    // ========================================================================
    // Audio output
    // ========================================================================

    /// Set the output audio port for downsampled audio.
    void set_audio_port(AudioPort* port) { audio_port_ = port; }

    // ========================================================================
    // Register interface — read from CPU
    // ========================================================================

    uint8_t read(uint8_t reg) const {
        switch (reg & 0x0F) {
            case pokey_constants::RANDOM:
                // RANDOM: return current LFSR state (bits depend on AUDCTL poly9 setting)
                if (audctl_ & pokey_constants::AUDCTL_POLY9)
                    return static_cast<uint8_t>(random_ & pokey_constants::POLY9_MASK);
                else
                    return static_cast<uint8_t>(random_ >> 8);

            case pokey_constants::IRQST:
                return irqst_;

            case pokey_constants::SKSTAT:
                return skstat_;

            case pokey_constants::ALLPOT:
                return 0x00;  // All pots complete (no paddles connected)

            case pokey_constants::KBCODE:
                return 0xFF;  // No key pressed

            case pokey_constants::SERIN:
                return 0xFF;  // No serial data

            default:
                // POT0-POT7: return 228 (center position)
                if ((reg & 0x0F) < 0x08)
                    return 228;
                return 0x00;
        }
    }

    // ========================================================================
    // Register interface — write from CPU
    // ========================================================================

    void write(uint8_t reg, uint8_t data) {
        switch (reg & 0x0F) {
            case pokey_constants::AUDF1: channel_[0].audf = data; break;
            case pokey_constants::AUDC1: channel_[0].audc = data; break;
            case pokey_constants::AUDF2: channel_[1].audf = data; break;
            case pokey_constants::AUDC2: channel_[1].audc = data; break;
            case pokey_constants::AUDF3: channel_[2].audf = data; break;
            case pokey_constants::AUDC3: channel_[2].audc = data; break;
            case pokey_constants::AUDF4: channel_[3].audf = data; break;
            case pokey_constants::AUDC4: channel_[3].audc = data; break;

            case pokey_constants::AUDCTL:
                audctl_ = data;
                break;

            case pokey_constants::STIMER:
                // Reset all channel dividers
                for (auto& ch : channel_)
                    ch.reload();
                break;

            case pokey_constants::SKREST:
                // Reset serial status bits
                skstat_ = 0xFF;
                break;

            case pokey_constants::POTGO:
                // Start pot scan — immediate complete for stub
                break;

            case pokey_constants::IRQEN:
                irqen_ = data;
                // Clear any pending IRQs no longer enabled
                irqst_ |= ~data;
                break;

            case pokey_constants::SKCTL:
                skctl_ = data;
                // If serial mode (bits 1:0) is 0, init mode resets poly counters
                if ((data & 0x03) == 0) {
                    poly4_pos_ = 0;
                    poly5_pos_ = 0;
                    poly9_pos_ = 0;
                    poly17_pos_ = 0;
                }
                break;

            default:
                break;
        }
    }

    // ========================================================================
    // Execution — call once per POKEY clock cycle
    // ========================================================================
    //
    // The POKEY runs at the system clock (typically 1.789773 MHz in home
    // computers, 1.512 MHz in Asteroids Deluxe).  Internally it has two
    // base clock rates for the audio channels:
    //   - 64 KHz (clock/28) — default
    //   - 15 KHz (clock/114) — selected by AUDCTL bit 0
    // Channels 1 and 3 can optionally use the raw clock (1.79 MHz mode).
    //
    // Each tick:
    //   1. Advance base clock divider
    //   2. When base divider fires, update all channel frequency dividers
    //   3. When a channel divider fires, toggle channel output (with poly gating)
    //   4. Mix channels and emit audio sample

    void tick() {
        // Advance LFSR (runs every clock)
        advance_lfsr();

        // Base clock divider
        uint8_t div = (audctl_ & pokey_constants::AUDCTL_15KHZ)
                    ? pokey_constants::DIV_15KHZ
                    : pokey_constants::DIV_64KHZ;

        bool base_tick = false;
        if (++div_counter_ >= div) {
            div_counter_ = 0;
            base_tick = true;
        }

        // Update each channel's frequency divider
        for (int i = 0; i < 4; i++) {
            bool use_fast = false;
            // Ch1 uses 1.79 MHz if AUDCTL bit 6 set
            if (i == 0 && (audctl_ & pokey_constants::AUDCTL_CH1_179MHZ))
                use_fast = true;
            // Ch3 uses 1.79 MHz if AUDCTL bit 5 set
            if (i == 2 && (audctl_ & pokey_constants::AUDCTL_CH3_179MHZ))
                use_fast = true;

            bool should_tick = use_fast || base_tick;

            // 16-bit linked mode: ch2 clocks from ch1 output, ch4 from ch3
            if ((i == 1) && (audctl_ & pokey_constants::AUDCTL_CH12_LINKED))
                should_tick = channel_[0].output;  // ch2 counts ch1 underflows
            if ((i == 3) && (audctl_ & pokey_constants::AUDCTL_CH34_LINKED))
                should_tick = channel_[2].output;  // ch4 counts ch3 underflows

            if (!should_tick) continue;

            auto& ch = channel_[i];
            if (ch.counter == 0) {
                ch.counter = ch.audf;
                // Toggle output through polynomial gating
                ch.output = apply_poly(ch);
            } else {
                ch.counter--;
            }
        }

        // High-pass filter: XOR ch1 output with ch3, ch2 with ch4
        bool ch0_out = channel_[0].output;
        bool ch1_out = channel_[1].output;
        if (audctl_ & pokey_constants::AUDCTL_HIPASS_CH1)
            ch0_out = ch0_out ^ channel_[2].output;
        if (audctl_ & pokey_constants::AUDCTL_HIPASS_CH2)
            ch1_out = ch1_out ^ channel_[3].output;

        // Mix output — each channel contributes 0-15 volume units
        float mix = 0.f;
        for (int i = 0; i < 4; i++) {
            auto& ch = channel_[i];
            if (ch.is_volume_only()) {
                // DAC mode: volume directly drives output
                mix += static_cast<float>(ch.volume());
            } else {
                bool out = (i == 0) ? ch0_out : (i == 1) ? ch1_out : ch.output;
                if (out)
                    mix += static_cast<float>(ch.volume());
            }
        }

        // Normalize: max output = 4 channels × 15 = 60
        float sample = mix / 60.f;

        if (audio_port_)
            audio_port_->drive(sample);
    }

    // ========================================================================
    // State (public for debug inspection)
    // ========================================================================

    PokeyChannel channel_[4] = {};
    uint8_t audctl_   = 0;
    uint8_t irqen_    = 0;
    uint8_t irqst_    = 0xFF;
    uint8_t skctl_    = 0;
    uint8_t skstat_   = 0xFF;

private:
    AudioPort* audio_port_ = nullptr;

    // Clock state
    uint8_t div_counter_ = 0;

    // LFSR state (random number generator)
    uint32_t random_ = 0xFFFF;

    // Polynomial counter positions (for lookup tables)
    uint32_t poly4_pos_  = 0;
    uint32_t poly5_pos_  = 0;
    uint32_t poly9_pos_  = 0;
    uint32_t poly17_pos_ = 0;

    // Precomputed polynomial lookup tables
    static constexpr int POLY4_SIZE  = 15;
    static constexpr int POLY5_SIZE  = 31;
    static constexpr int POLY9_SIZE  = 511;
    static constexpr int POLY17_SIZE = 131071;

    // Small tables stored inline; the 17-bit table uses the LFSR directly
    uint8_t poly4_table_[POLY4_SIZE]  = {};
    uint8_t poly5_table_[POLY5_SIZE]  = {};
    uint8_t poly9_table_[POLY9_SIZE]  = {};

    // ========================================================================
    // Polynomial table initialization
    // ========================================================================

    void init_poly_tables() {
        // 4-bit LFSR: taps at bits 0,1 (x^4 + x + 1)
        {
            uint32_t lfsr = 0x0F;
            for (int i = 0; i < POLY4_SIZE; i++) {
                poly4_table_[i] = lfsr & 1;
                uint32_t feedback = ((lfsr >> 0) ^ (lfsr >> 1)) & 1;
                lfsr = (lfsr >> 1) | (feedback << 3);
            }
        }

        // 5-bit LFSR: taps at bits 0,2 (x^5 + x^3 + 1) — matches POKEY hardware
        {
            uint32_t lfsr = 0x1F;
            for (int i = 0; i < POLY5_SIZE; i++) {
                poly5_table_[i] = lfsr & 1;
                uint32_t feedback = ((lfsr >> 0) ^ (lfsr >> 2)) & 1;
                lfsr = (lfsr >> 1) | (feedback << 4);
            }
        }

        // 9-bit LFSR: taps at bits 0,4 (x^9 + x^5 + 1)
        {
            uint32_t lfsr = 0x1FF;
            for (int i = 0; i < POLY9_SIZE; i++) {
                poly9_table_[i] = lfsr & 1;
                uint32_t feedback = ((lfsr >> 0) ^ (lfsr >> 4)) & 1;
                lfsr = (lfsr >> 1) | (feedback << 8);
            }
        }
    }

    // ========================================================================
    // LFSR advancement (for RANDOM register)
    // ========================================================================

    void advance_lfsr() {
        // 17-bit LFSR: taps at bits 0 and 5 (or 0 and 2 for 9-bit mode)
        if (audctl_ & pokey_constants::AUDCTL_POLY9) {
            uint32_t feedback = ((random_ >> 0) ^ (random_ >> 4)) & 1;
            random_ = ((random_ >> 1) | (feedback << 8)) & pokey_constants::POLY9_MASK;
        } else {
            uint32_t feedback = ((random_ >> 0) ^ (random_ >> 5)) & 1;
            random_ = ((random_ >> 1) | (feedback << 16)) & pokey_constants::POLY17_MASK;
        }
    }

    // ========================================================================
    // Polynomial gating for channel output toggle
    // ========================================================================

    /// Apply polynomial counter to determine if channel output should toggle.
    /// Returns the new output state for the channel.
    bool apply_poly(PokeyChannel& ch) {
        uint8_t sel = ch.poly_select();

        // Get poly5 gate (used by most modes)
        bool poly5 = poly5_table_[poly5_pos_ % POLY5_SIZE];

        switch (sel) {
            case pokey_constants::AUDC_POLY_5_17:
                // 5+17 bit: toggle only when both poly5 and poly17 are high
                if (poly5) {
                    bool poly17 = (random_ & 1);
                    return poly17;
                }
                return ch.output;

            case pokey_constants::AUDC_POLY_5:
                // 5-bit poly only
                return poly5;

            case pokey_constants::AUDC_POLY_5_4:
                // 5+4 bit: toggle when poly5 high, use poly4 for output
                if (poly5) {
                    return poly4_table_[poly4_pos_ % POLY4_SIZE];
                }
                return ch.output;

            case pokey_constants::AUDC_POLY_5_TONE:
                // 5-bit poly gated with pure tone
                if (poly5) {
                    return !ch.output;  // Toggle (square wave)
                }
                return ch.output;

            case pokey_constants::AUDC_POLY_17:
                // 17-bit poly (no 5-bit gate)
                return (random_ & 1) != 0;

            case pokey_constants::AUDC_TONE:
            case pokey_constants::AUDC_TONE_ALT:
                // Pure tone: simple toggle (square wave)
                return !ch.output;

            case pokey_constants::AUDC_POLY_4:
                // 4-bit poly (no 5-bit gate)
                return poly4_table_[poly4_pos_ % POLY4_SIZE];

            default:
                return !ch.output;
        }
    }
};
