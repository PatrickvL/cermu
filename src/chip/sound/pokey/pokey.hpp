#pragma once
/*
 * pokey.hpp — Atari POKEY (C012294) sound + I/O chip — template core
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
 *          Atari 400/800/XL/XE home computers, Atari 5200, and many
 *          other arcade boards (often with multiple POKEYs per board).
 *
 * Audio architecture:
 *   Each channel has:
 *     - AUDFn: 8-bit frequency divider reload value
 *     - AUDCn: control (4-bit volume + poly/noise select + force-high)
 *   Base clock: 64 KHz (1.79 MHz / 28) or 15 KHz (1.79 MHz / 114)
 *   Channels can be linked for 16-bit resolution (1+2 or 3+4)
 *   Polynomial counters: 4-bit, 5-bit, 9-bit, 17-bit LFSR
 *
 * BUS PROTOCOL:
 *   tick() receives and returns bus_state_t.  The system services the
 *   bus between ticks.  POKEY drives address+control for any memory
 *   access it needs; CPU-driven register R/W is decoded by the system
 *   and dispatched via read()/write() before calling tick().
 *
 * AUDIO THREADING:
 *   Audio sample generation is decoupled via AudioRingBuffer (SPSC
 *   lock-free).  When no state readbacks are required, sample generation
 *   could run in a separate host thread reading from the ring buffer.
 *
 * Register map:
 *   Write registers ($x0-$x0F):
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

#include "chip/sound/pokey/pokey_traits.hpp"
#include "chip/sound/sound_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include "core/signal/audio_port.hpp"
#include "core/system_lines.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// POKEY REGISTER DECLARATION TABLE — single source of truth
// ============================================================================
//
// REG(offset, symbol, description)
// FLD(reg_sym, field_sym, hi:lo, description, kind, display_shift, display_scale)

#define POKEY_DECL(REG, FLD, CMP) \
    REG(0x00, AUDF1,   "Channel 1 frequency")                                   \
    REG(0x01, AUDC1,   "Channel 1 control")                                     \
      FLD(AUDC1, POLY_SEL, 7:5, "Poly/noise select",     Value, 0, 0)          \
      FLD(AUDC1, VOL_ONLY, 4:4, "Volume only (DAC)",     Flag,  0, 0)          \
      FLD(AUDC1, VOLUME,   3:0, "Volume (0-15)",         Level, 0, 0)          \
    REG(0x02, AUDF2,   "Channel 2 frequency")                                   \
    REG(0x03, AUDC2,   "Channel 2 control")                                     \
      FLD(AUDC2, POLY_SEL, 7:5, "Poly/noise select",     Value, 0, 0)          \
      FLD(AUDC2, VOL_ONLY, 4:4, "Volume only (DAC)",     Flag,  0, 0)          \
      FLD(AUDC2, VOLUME,   3:0, "Volume (0-15)",         Level, 0, 0)          \
    REG(0x04, AUDF3,   "Channel 3 frequency")                                   \
    REG(0x05, AUDC3,   "Channel 3 control")                                     \
      FLD(AUDC3, POLY_SEL, 7:5, "Poly/noise select",     Value, 0, 0)          \
      FLD(AUDC3, VOL_ONLY, 4:4, "Volume only (DAC)",     Flag,  0, 0)          \
      FLD(AUDC3, VOLUME,   3:0, "Volume (0-15)",         Level, 0, 0)          \
    REG(0x06, AUDF4,   "Channel 4 frequency")                                   \
    REG(0x07, AUDC4,   "Channel 4 control")                                     \
      FLD(AUDC4, POLY_SEL, 7:5, "Poly/noise select",     Value, 0, 0)          \
      FLD(AUDC4, VOL_ONLY, 4:4, "Volume only (DAC)",     Flag,  0, 0)          \
      FLD(AUDC4, VOLUME,   3:0, "Volume (0-15)",         Level, 0, 0)          \
    REG(0x08, AUDCTL,  "Audio control")                                          \
      FLD(AUDCTL, POLY9,       7:7, "9-bit poly (vs 17)",  Flag, 0, 0)         \
      FLD(AUDCTL, CH1_179MHZ,  6:6, "Ch1 1.79 MHz clock",  Flag, 0, 0)         \
      FLD(AUDCTL, CH3_179MHZ,  5:5, "Ch3 1.79 MHz clock",  Flag, 0, 0)         \
      FLD(AUDCTL, CH12_LINKED, 4:4, "Ch1+2 16-bit link",   Flag, 0, 0)         \
      FLD(AUDCTL, CH34_LINKED, 3:3, "Ch3+4 16-bit link",   Flag, 0, 0)         \
      FLD(AUDCTL, HIPASS_CH1,  2:2, "Ch1 high-pass (ch3)",  Flag, 0, 0)        \
      FLD(AUDCTL, HIPASS_CH2,  1:1, "Ch2 high-pass (ch4)",  Flag, 0, 0)        \
      FLD(AUDCTL, BASE_15KHZ,  0:0, "15 KHz base clock",    Flag, 0, 0)        \
    REG(0x09, STIMER,  "Start timers (write)")                                   \
    REG(0x0A, SKREST,  "Reset serial status (write)")                            \
    REG(0x0B, POTGO,   "Start pot scan (write)")                                 \
    REG(0x0C, UNUSED,  "Unused")                                                 \
    REG(0x0D, SEROUT,  "Serial output data (write)")                             \
    REG(0x0E, IRQEN,   "IRQ enable (write)")                                     \
      FLD(IRQEN, BREAK_KEY,   7:7, "Break key IRQ",      Flag, 0, 0)           \
      FLD(IRQEN, SER_RCV,     5:5, "Serial recv done",   Flag, 0, 0)           \
      FLD(IRQEN, SER_XMT,     4:4, "Serial xmit done",   Flag, 0, 0)          \
      FLD(IRQEN, SER_XMT_RDY, 3:3, "Serial xmit ready",  Flag, 0, 0)          \
      FLD(IRQEN, TIMER4,      2:2, "Timer 4 IRQ",        Flag, 0, 0)           \
      FLD(IRQEN, TIMER2,      1:1, "Timer 2 IRQ",        Flag, 0, 0)           \
      FLD(IRQEN, TIMER1,      0:0, "Timer 1 IRQ",        Flag, 0, 0)           \
    REG(0x0F, SKCTL,   "Serial port control (write)")                            \
      FLD(SKCTL, FORCE_BREAK,  7:7, "Force break",       Flag, 0, 0)           \
      FLD(SKCTL, TWO_TONE,     3:3, "Two-tone mode",     Flag, 0, 0)           \
      FLD(SKCTL, FAST_POT,     2:2, "Fast pot scan",     Flag, 0, 0)           \
      FLD(SKCTL, SER_MODE,     1:0, "Serial mode",       Value, 0, 0)

// --- Read register bank offset into regs_ array ---
#define POKEY_READ_BASE 16

// ============================================================================
// Extract constants and debug metadata
// ============================================================================

namespace pokey::reg {
    POKEY_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    constexpr uint8_t WRITE_REG_COUNT = 16;
    constexpr uint8_t READ_REG_COUNT  = 16;
    constexpr uint8_t TOTAL_REGS      = WRITE_REG_COUNT + READ_REG_COUNT;

    // Read register indices (offset by POKEY_READ_BASE into regs_ array)
    constexpr uint8_t R_POT0    = POKEY_READ_BASE + 0x00;
    constexpr uint8_t R_POT1    = POKEY_READ_BASE + 0x01;
    constexpr uint8_t R_POT2    = POKEY_READ_BASE + 0x02;
    constexpr uint8_t R_POT3    = POKEY_READ_BASE + 0x03;
    constexpr uint8_t R_POT4    = POKEY_READ_BASE + 0x04;
    constexpr uint8_t R_POT5    = POKEY_READ_BASE + 0x05;
    constexpr uint8_t R_POT6    = POKEY_READ_BASE + 0x06;
    constexpr uint8_t R_POT7    = POKEY_READ_BASE + 0x07;
    constexpr uint8_t R_ALLPOT  = POKEY_READ_BASE + 0x08;
    constexpr uint8_t R_KBCODE  = POKEY_READ_BASE + 0x09;
    constexpr uint8_t R_RANDOM  = POKEY_READ_BASE + 0x0A;
    constexpr uint8_t R_UNUSED1 = POKEY_READ_BASE + 0x0B;
    constexpr uint8_t R_UNUSED2 = POKEY_READ_BASE + 0x0C;
    constexpr uint8_t R_SERIN   = POKEY_READ_BASE + 0x0D;
    constexpr uint8_t R_IRQST   = POKEY_READ_BASE + 0x0E;
    constexpr uint8_t R_SKSTAT  = POKEY_READ_BASE + 0x0F;
} // namespace pokey::reg

// Bitfield accessors
namespace pokey::fld {
#define POKEY_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld    = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
POKEY_DECL(DECL_REG_NOP, POKEY_X_FLD_NS_, DECL_CMP_NOP)
#undef POKEY_X_FLD_NS_
} // namespace pokey::fld

DECL_EXTRACT(POKEY, POKEY_DECL)

// ============================================================================
// POKEY CONSTANTS (audio engine)
// ============================================================================

namespace pokey_constants {

    // AUDCn poly select values (bits 7:5)
    inline constexpr uint8_t AUDC_POLY_5_17      = 0x00;
    inline constexpr uint8_t AUDC_POLY_5         = 0x20;
    inline constexpr uint8_t AUDC_POLY_5_4       = 0x40;
    inline constexpr uint8_t AUDC_POLY_5_TONE    = 0x60;
    inline constexpr uint8_t AUDC_POLY_17        = 0x80;
    inline constexpr uint8_t AUDC_TONE           = 0xA0;
    inline constexpr uint8_t AUDC_POLY_4         = 0xC0;
    inline constexpr uint8_t AUDC_TONE_ALT       = 0xE0;

    // Base clock dividers (from master clock)
    inline constexpr uint8_t DIV_64KHZ  = 28;
    inline constexpr uint8_t DIV_15KHZ  = 114;

    // LFSR polynomials
    inline constexpr uint32_t POLY4_MASK  = 0x000F;
    inline constexpr uint32_t POLY5_MASK  = 0x001F;
    inline constexpr uint32_t POLY9_MASK  = 0x01FF;
    inline constexpr uint32_t POLY17_MASK = 0x1FFFF;

    // Audio ring buffer size (SPSC)
    inline constexpr uint32_t AUDIO_BUFFER_SIZE = 4096;

}  // namespace pokey_constants

// ============================================================================
// POKEY AUDIO CHANNEL — per-channel audio state
// ============================================================================

struct PokeyChannel {
    uint16_t counter  = 0;        // Current divider countdown
    bool     output   = false;    // Current channel output state (high/low)
    bool     borrow   = false;    // Underflow flag (for 16-bit linked clocking)

    void reload(uint8_t audf) { counter = audf; }
};

// ============================================================================
// POKEY CHIP — trait-driven template
// ============================================================================

namespace pokey {

template <const POKEYTraits& Traits>
class pokey_t : public SoundChipBase {
public:

    // === Compile-time feature detection ===
    static constexpr bool has_keyboard()      { return Traits.has_keyboard(); }
    static constexpr bool has_serial()         { return Traits.has_serial(); }
    static constexpr bool has_pot_inputs()     { return Traits.has_pot_inputs(); }
    static constexpr bool has_timers()         { return Traits.has_timers(); }
    static constexpr bool has_timer_bug_fix()  { return Traits.has_timer_bug_fix(); }

    // === Chip identity ===
    pokey_t()
        : SoundChipBase(ChipInfo{Traits.chip_id, Traits.vendor, Traits.display_name})
        , audio_buffer_(pokey_constants::AUDIO_BUFFER_SIZE)
    {
        init_regs(pokey::reg::TOTAL_REGS);
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    // ========================================================================
    // Initialization
    // ========================================================================

    void init() {
        reset();
        init_poly_tables();
    }

    void reset() {
        regs_.clear();

        // Read register defaults
        regs_[pokey::reg::R_IRQST]  = 0xFF;   // No pending IRQs (active-low)
        regs_[pokey::reg::R_SKSTAT] = 0xFF;   // No errors
        // POT0-POT7: center position
        for (int i = 0; i < 8; i++)
            regs_[pokey::reg::R_POT0 + i] = 228;
        regs_[pokey::reg::R_KBCODE] = 0xFF;    // No key pressed
        regs_[pokey::reg::R_SERIN]  = 0xFF;    // No serial data

        for (auto& ch : channel_) {
            ch.counter = 0;
            ch.output = false;
            ch.borrow = false;
        }
        poly4_pos_   = 0;
        poly5_pos_   = 0;
        poly9_pos_   = 0;
        poly17_pos_  = 0;
        div_counter_ = 0;
        random_      = 0xFFFF;

        audio_buffer_.reset();
        audio_cycle_accum_ = 0.0;
    }

    // ========================================================================
    // Audio output — SPSC ring buffer for thread-safe sample generation
    // ========================================================================

    void set_audio_port(AudioPort* port) { audio_port_ = port; }

    void set_clock_frequency(uint32_t hz) {
        clock_hz_ = hz;
        update_cycles_per_sample();
    }

    void set_audio_sample_rate(int rate) {
        audio_sample_rate_ = rate;
        update_cycles_per_sample();
    }

    uint32_t audio_read(float* buffer, uint32_t max_samples) {
        return audio_buffer_.read(buffer, max_samples);
    }

    uint32_t audio_available() const {
        return audio_buffer_.available();
    }

    // ========================================================================
    // Device callbacks — decoupled from external devices (CIA pattern)
    // ========================================================================

    // Keyboard scan callback: called when POKEY reads keyboard matrix.
    // Returns current key code (or 0xFF for no key).
    uint8_t (*keyboard_read_callback)(void* context) = nullptr;
    void* keyboard_read_context = nullptr;

    // Keyboard key-pressed callback: returns true if any key is currently pressed.
    bool (*keyboard_pressed_callback)(void* context) = nullptr;
    void* keyboard_pressed_context = nullptr;

    // Pot (paddle) input callbacks: called during pot scan.
    // Returns pot value (0-228) for the given pot index (0-7).
    uint8_t (*pot_read_callback)(void* context, uint8_t pot_index) = nullptr;
    void* pot_read_context = nullptr;

    // Serial input callback: called when POKEY needs serial data.
    // Returns received byte (or 0xFF for no data).
    uint8_t (*serial_read_callback)(void* context) = nullptr;
    void* serial_read_context = nullptr;

    // Serial output callback: called when POKEY sends serial data.
    void (*serial_write_callback)(void* context, uint8_t data) = nullptr;
    void* serial_write_context = nullptr;

    // IRQ output callback: called when IRQ state changes.
    void (*irq_callback)(void* context, bool irq_active) = nullptr;
    void* irq_callback_context = nullptr;

    // ========================================================================
    // Bus MMIO interface — enables BusMap auto-wiring
    // ========================================================================

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        BUS_SET_DATA(bus, read(static_cast<uint8_t>(BUS_GET_ADDR(bus) & 0x0F)));
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        write(static_cast<uint8_t>(BUS_GET_ADDR(bus) & 0x0F), BUS_GET_DATA(bus));
        return bus;
    }

    // ========================================================================
    // Register interface — read from CPU
    // ========================================================================

    uint8_t read(uint8_t reg) const {
        uint8_t r = reg & 0x0F;
        switch (r) {
            case 0x0A: // RANDOM
                if (regs_[pokey::reg::AUDCTL] & pokey::fld::AUDCTL_POLY9)
                    return static_cast<uint8_t>(random_ & pokey_constants::POLY9_MASK);
                else
                    return static_cast<uint8_t>(random_ >> 8);

            case 0x0E: // IRQST
                return regs_[pokey::reg::R_IRQST];

            case 0x0F: // SKSTAT
                return regs_[pokey::reg::R_SKSTAT];

            case 0x08: // ALLPOT
                return regs_[pokey::reg::R_ALLPOT];

            case 0x09: // KBCODE
                if constexpr (has_keyboard()) {
                    if (keyboard_read_callback)
                        return keyboard_read_callback(keyboard_read_context);
                }
                return regs_[pokey::reg::R_KBCODE];

            case 0x0D: // SERIN
                if constexpr (has_serial()) {
                    if (serial_read_callback)
                        return serial_read_callback(serial_read_context);
                }
                return regs_[pokey::reg::R_SERIN];

            default:
                // POT0-POT7 (0x00-0x07)
                if (r < 0x08) {
                    if constexpr (has_pot_inputs()) {
                        if (pot_read_callback)
                            return pot_read_callback(pot_read_context, r);
                    }
                    return regs_[pokey::reg::R_POT0 + r];
                }
                return 0x00;
        }
    }

    // ========================================================================
    // Register interface — write from CPU
    // ========================================================================

    void write(uint8_t reg, uint8_t data) {
        uint8_t r = reg & 0x0F;
        regs_[r] = data;  // Store in write register bank

        switch (r) {
            case pokey::reg::STIMER:
                // Reset all channel dividers
                for (int i = 0; i < 4; i++)
                    channel_[i].reload(regs_[i * 2]);  // AUDFn at even offsets
                break;

            case pokey::reg::SKREST:
                regs_[pokey::reg::R_SKSTAT] = 0xFF;  // Reset serial status
                break;

            case pokey::reg::POTGO:
                // Start pot scan
                if constexpr (has_pot_inputs()) {
                    regs_[pokey::reg::R_ALLPOT] = 0xFF;  // All pots scanning
                    pot_scan_counter_ = 0;
                }
                break;

            case pokey::reg::IRQEN: {
                uint8_t irqst = regs_[pokey::reg::R_IRQST];
                // Clear pending IRQs no longer enabled
                irqst |= ~data;
                regs_[pokey::reg::R_IRQST] = irqst;
                update_irq_output();
                break;
            }

            case pokey::reg::SKCTL:
                // Init mode: serial mode bits 1:0 = 0 resets poly counters
                if ((data & 0x03) == 0) {
                    poly4_pos_ = 0;
                    poly5_pos_ = 0;
                    poly9_pos_ = 0;
                    poly17_pos_ = 0;
                }
                break;

            case pokey::reg::SEROUT:
                if constexpr (has_serial()) {
                    if (serial_write_callback)
                        serial_write_callback(serial_write_context, data);
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
    //   1. Advance LFSR (random number generator — runs every clock)
    //   2. Advance base clock divider
    //   3. When base divider fires, update all channel frequency dividers
    //   4. When a channel divider fires, toggle channel output (with poly gating)
    //   5. Mix channels and emit audio sample
    //   6. Advance pot scan counter (if active)

    bus_state_t tick(bus_state_t pins) {
        // Advance LFSR (runs every clock)
        advance_lfsr();

        // Advance polynomial counter positions
        poly4_pos_++;
        poly5_pos_++;

        // Base clock divider
        uint8_t audctl = regs_[pokey::reg::AUDCTL];
        uint8_t div = (audctl & pokey::fld::AUDCTL_BASE_15KHZ)
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
            if (i == 0 && (audctl & pokey::fld::AUDCTL_CH1_179MHZ))
                use_fast = true;
            // Ch3 uses 1.79 MHz if AUDCTL bit 5 set
            if (i == 2 && (audctl & pokey::fld::AUDCTL_CH3_179MHZ))
                use_fast = true;

            bool should_tick = use_fast || base_tick;

            // 16-bit linked mode: ch2 clocks from ch1 borrow, ch4 from ch3 borrow
            if ((i == 1) && (audctl & pokey::fld::AUDCTL_CH12_LINKED))
                should_tick = channel_[0].borrow;
            if ((i == 3) && (audctl & pokey::fld::AUDCTL_CH34_LINKED))
                should_tick = channel_[2].borrow;

            channel_[i].borrow = false;

            if (!should_tick) continue;

            auto& ch = channel_[i];
            if (ch.counter == 0) {
                ch.counter = regs_[i * 2];  // Reload from AUDFn
                ch.borrow = true;
                // Toggle output through polynomial gating
                ch.output = apply_poly(regs_[i * 2 + 1]);  // AUDCn
            } else {
                ch.counter--;
            }
        }

        // High-pass filter: XOR ch1 output with ch3, ch2 with ch4
        bool ch0_out = channel_[0].output;
        bool ch1_out = channel_[1].output;
        if (audctl & pokey::fld::AUDCTL_HIPASS_CH1)
            ch0_out = ch0_out ^ channel_[2].output;
        if (audctl & pokey::fld::AUDCTL_HIPASS_CH2)
            ch1_out = ch1_out ^ channel_[3].output;

        // Mix output — each channel contributes 0-15 volume units
        float mix = 0.f;
        for (int i = 0; i < 4; i++) {
            uint8_t audc = regs_[i * 2 + 1];  // AUDCn
            uint8_t vol = audc & pokey::fld::AUDC1_VOLUME;
            if (audc & pokey::fld::AUDC1_VOL_ONLY) {
                // DAC mode: volume directly drives output
                mix += static_cast<float>(vol);
            } else {
                bool out = (i == 0) ? ch0_out : (i == 1) ? ch1_out : channel_[i].output;
                if (out)
                    mix += static_cast<float>(vol);
            }
        }

        // Normalize: max output = 4 channels × 15 = 60
        float sample = mix / 60.f;
        buffer_sample(sample);

        // Timer IRQs: fire when channel counter borrows (underflows)
        if constexpr (has_timers()) {
            uint8_t irqen = regs_[pokey::reg::IRQEN];
            uint8_t irqst = regs_[pokey::reg::R_IRQST];
            // Timer 1 (ch1), Timer 2 (ch2), Timer 4 (ch4)
            if (channel_[0].borrow && (irqen & 0x01)) irqst &= ~0x01;
            if (channel_[1].borrow && (irqen & 0x02)) irqst &= ~0x02;
            if (channel_[3].borrow && (irqen & 0x04)) irqst &= ~0x04;
            if (irqst != regs_[pokey::reg::R_IRQST]) {
                regs_[pokey::reg::R_IRQST] = irqst;
                update_irq_output();
                // Assert IRQ on bus if any timer fired
                if ((~irqst & irqen) != 0)
                    BUS_CLR_BIT(pins, BUS_IRQ_BIT);
            }
        }

        // Pot scan counter advancement
        if constexpr (has_pot_inputs()) {
            if (regs_[pokey::reg::R_ALLPOT] != 0x00) {
                pot_scan_counter_++;
                for (int i = 0; i < 8; i++) {
                    if (regs_[pokey::reg::R_ALLPOT] & (1 << i)) {
                        uint8_t pot_val = 228;  // Default center
                        if (pot_read_callback)
                            pot_val = pot_read_callback(pot_read_context, i);
                        if (pot_scan_counter_ >= pot_val) {
                            regs_[pokey::reg::R_POT0 + i] = pot_val;
                            regs_[pokey::reg::R_ALLPOT] &= ~(1 << i);  // Done
                        }
                    }
                }
            }
        }

        // Store bus snapshot for edge detection / GUI pin rendering
        bus_snapshot_ = pins;

        return pins;
    }

    // ========================================================================
    // Direct sample generation — for use when state readbacks are not needed
    // Can be called from a separate audio thread.
    // ========================================================================

    float get_sample() const {
        uint8_t audctl = regs_[pokey::reg::AUDCTL];
        bool ch0_out = channel_[0].output;
        bool ch1_out = channel_[1].output;
        if (audctl & pokey::fld::AUDCTL_HIPASS_CH1)
            ch0_out = ch0_out ^ channel_[2].output;
        if (audctl & pokey::fld::AUDCTL_HIPASS_CH2)
            ch1_out = ch1_out ^ channel_[3].output;

        float mix = 0.f;
        for (int i = 0; i < 4; i++) {
            uint8_t audc = regs_[i * 2 + 1];
            uint8_t vol = audc & pokey::fld::AUDC1_VOLUME;
            if (audc & pokey::fld::AUDC1_VOL_ONLY) {
                mix += static_cast<float>(vol);
            } else {
                bool out = (i == 0) ? ch0_out : (i == 1) ? ch1_out : channel_[i].output;
                if (out) mix += static_cast<float>(vol);
            }
        }
        return mix / 60.f;
    }

    // ========================================================================
    // State — public for debug inspection
    // ========================================================================

    PokeyChannel channel_[4] = {};

    // === ChipBase GUI virtuals ===
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif

private:
    AudioPort* audio_port_ = nullptr;

    // Audio ring buffer (SPSC: emulation thread writes, audio thread reads)
    AudioRingBuffer audio_buffer_;
    uint32_t clock_hz_ = 0;
    int      audio_sample_rate_ = 0;
    double   audio_cycles_per_sample_ = 0.0;
    double   audio_cycle_accum_ = 0.0;

    // Clock state
    uint8_t div_counter_ = 0;

    // LFSR state (random number generator)
    uint32_t random_ = 0xFFFF;

    // Polynomial counter positions (for lookup tables)
    uint32_t poly4_pos_  = 0;
    uint32_t poly5_pos_  = 0;
    uint32_t poly9_pos_  = 0;
    uint32_t poly17_pos_ = 0;

    // Pot scan
    uint16_t pot_scan_counter_ = 0;

    // Precomputed polynomial lookup tables
    static constexpr int POLY4_SIZE  = 15;
    static constexpr int POLY5_SIZE  = 31;
    static constexpr int POLY9_SIZE  = 511;

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
        if (regs_[pokey::reg::AUDCTL] & pokey::fld::AUDCTL_POLY9) {
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
    /// audc is the AUDCn register value for the channel.
    bool apply_poly(uint8_t audc) {
        uint8_t sel = audc & pokey::fld::AUDC1_POLY_SEL;

        // Get poly5 gate (used by most modes)
        bool poly5 = poly5_table_[poly5_pos_ % POLY5_SIZE];

        switch (sel) {
            case pokey_constants::AUDC_POLY_5_17:
                if (poly5) return (random_ & 1) != 0;
                return false;

            case pokey_constants::AUDC_POLY_5:
                return poly5;

            case pokey_constants::AUDC_POLY_5_4:
                if (poly5) return poly4_table_[poly4_pos_ % POLY4_SIZE] != 0;
                return false;

            case pokey_constants::AUDC_POLY_5_TONE:
                return poly5;

            case pokey_constants::AUDC_POLY_17:
                return (random_ & 1) != 0;

            case pokey_constants::AUDC_TONE:
            case pokey_constants::AUDC_TONE_ALT:
                return true;  // Toggle always (square wave)

            case pokey_constants::AUDC_POLY_4:
                return poly4_table_[poly4_pos_ % POLY4_SIZE] != 0;

            default:
                return true;
        }
    }

    // ========================================================================
    // Audio sample buffering (decimated to output rate)
    // ========================================================================

    void update_cycles_per_sample() {
        if (audio_sample_rate_ > 0 && clock_hz_ > 0) {
            audio_cycles_per_sample_ =
                static_cast<double>(clock_hz_) / audio_sample_rate_;
        }
    }

    void buffer_sample(float sample) {
        // Always drive the AudioPort (for systems that use direct port wiring)
        if (audio_port_) {
            audio_port_->drive(sample);
            return;
        }
        // Ring buffer mode: decimate to configured sample rate
        if (audio_cycles_per_sample_ <= 0.0) return;
        audio_cycle_accum_ += 1.0;
        if (audio_cycle_accum_ < audio_cycles_per_sample_) return;
        audio_cycle_accum_ -= audio_cycles_per_sample_;
        audio_buffer_.write(&sample, 1);
    }

    // ========================================================================
    // IRQ output management
    // ========================================================================

    void update_irq_output() {
        uint8_t irqst = regs_[pokey::reg::R_IRQST];
        uint8_t irqen = regs_[pokey::reg::IRQEN];
        // IRQ active when any enabled interrupt has fired (active-low in IRQST)
        bool irq_active = (~irqst & irqen) != 0;
        if (irq_callback)
            irq_callback(irq_callback_context, irq_active);
    }
};

} // namespace pokey
