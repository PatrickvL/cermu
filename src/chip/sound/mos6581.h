#pragma once

#include "sound_chip_base.h"
#include "../../core/bus_cycle_interface.h"
#include "../../core/system_lines.h" // For bus_state_t
#include <atomic>
#include <cstdint>

#include <cmath>

// SID MOS 6581 DIP has 28 pins; Pinout:
enum mos6581_pin_t {
    SID_PIN_CAP1A = 1, SID_PIN_VDD = 28,
    SID_PIN_CAP1B = 2, SID_PIN_AUDIO_OUT = 27,
    SID_PIN_CAP2A = 3, SID_PIN_EXT_IN = 26,
    SID_PIN_CAP2B = 4, SID_PIN_VCC = 25,
    SID_PIN_RES = 5, SID_PIN_POT_X = 24,
    SID_PIN_PHI2 = 6, SID_PIN_POT_Y = 23,
    SID_PIN_R_W = 7, SID_PIN_D7 = 22,
    SID_PIN_CS = 8, SID_PIN_D6 = 21,
    SID_PIN_A0 = 9, SID_PIN_D5 = 20,
    SID_PIN_A1 = 10, SID_PIN_D4 = 19,
    SID_PIN_A2 = 11, SID_PIN_D3 = 18,
    SID_PIN_A3 = 12, SID_PIN_D2 = 17,
    SID_PIN_A4 = 13, SID_PIN_D1 = 16,
    SID_PIN_GND = 14, SID_PIN_D0 = 15,
};

// Register dimensions - Modern C++ constants
namespace sid_constants {
    constexpr uint8_t REGS_BITS = 5;
    constexpr uint8_t REGS_SIZE = (1 << REGS_BITS); // 32
    constexpr uint8_t REGS_MASK = REGS_SIZE - 1;    // 31
}

// SID chip revisions — only revisions that produce different emulation
// behaviour are active.  The rest are commented out until per-revision
// differences (combined-waveform tables, filter curves, etc.) are modelled.
enum sid_revision_t {
    // SID_REVISION_6581_R1,     // same behaviour as R4AR for now
    // SID_REVISION_6581_R2,     // same behaviour as R4AR for now
    // SID_REVISION_6581_R3,     // same behaviour as R4AR for now
    // SID_REVISION_6581_R4,     // same behaviour as R4AR for now
    SID_REVISION_6581_R4AR,      // MOS 6581 family (filter distortion, volume-click)
    SID_REVISION_8580_R5,        // MOS 8580 family (clean filter, no volume-click)
    // SID_REVISION_CSG_6581,    // same behaviour as R4AR for now
    // SID_REVISION_CSG_8580     // same behaviour as 8580_R5 for now
};

// Voice envelope cycle states (matches reSID State enum)
enum envelope_cycle_t {
    CYCLE_OFF = 0,      // Off cycle (0)
    CYCLE_ATTACK = 1,   // Attack cycle (1)
    CYCLE_DECAY = 2,    // Decay/Sustain cycle (reSID DECAY_SUSTAIN)
    CYCLE_SUSTAIN = 3,  // Sustain cycle (unused — decay handles sustain check)
    CYCLE_RELEASE = 4,  // Release cycle (4)
    CYCLE_FREEZED = 5   // Frozen at zero (reSID FREEZED)
};

// Waveform bits — positioned at their VCREG bit locations (bits 4-7)
// so hot-path tests use control_reg & WAVEFORM_xxx with no shift.
enum waveform_bits_t {
    WAVEFORM_NONE     = 0x00, // No waveform
    WAVEFORM_TRIANGLE = 0x10, // Triangle waveform (VCREG bit 4)
    WAVEFORM_SAWTOOTH = 0x20, // Sawtooth waveform (VCREG bit 5)
    WAVEFORM_PULSE    = 0x40, // Pulse waveform    (VCREG bit 6)
    WAVEFORM_NOISE    = 0x80, // Noise waveform    (VCREG bit 7)
    WAVEFORM_MASK     = 0xF0  // All waveform bits
};

// VCREG control bits (lower nibble)
#define VCREG_GATE   0x01
#define VCREG_SYNC   0x02
#define VCREG_RING   0x04
#define VCREG_TEST   0x08

// Per-voice register offsets within a voice block
#define VOICE_FRELO  0  // Frequency low byte
#define VOICE_FREHI  1  // Frequency high byte
#define VOICE_PWLO   2  // Pulse width low byte
#define VOICE_PWHI   3  // Pulse width high nibble
#define VOICE_VCREG  4  // Voice control register
#define VOICE_ATDCY  5  // Attack / Decay
#define VOICE_SUREL  6  // Sustain / Release
#define VOICE_REGS   7  // Number of registers per voice

// Global SID register addresses
#define SID_REG_CUTLO   0x15  // Filter cutoff low 3 bits
#define SID_REG_CUTHI   0x16  // Filter cutoff high 8 bits
#define SID_REG_RESON   0x17  // Filter resonance / voice routing
#define SID_REG_SIGVOL  0x18  // Filter mode select / master volume
#define SID_REG_POTX    0x19  // Paddle X (read-only)
#define SID_REG_POTY    0x1A  // Paddle Y (read-only)
#define SID_REG_OSC3    0x1B  // Oscillator 3 output (read-only)
#define SID_REG_ENV3    0x1C  // Envelope 3 output (read-only)

// ============================================================================
// SID DECLARATION TABLE — registers + derived fields (32 REG + 34 FLD)
// Voices 1-3 each occupy 7 registers; global filter/misc at $15-$1C;
// $1D-$1F are unused/unmapped.
// ============================================================================

// REG(addr, symbol, description)
// FLD(parent_reg, symbol, hi:lo, description, kind, display_shift, display_scale)
#define SID_DECL(REG, FLD, CMP)                                         \
    REG(0x00, V1_FRELO,  "Voice 1 freq lo")                                  \
    REG(0x01, V1_FREHI,  "Voice 1 freq hi")                                  \
    REG(0x02, V1_PWLO,   "Voice 1 pulse W lo")                               \
    REG(0x03, V1_PWHI,   "Voice 1 pulse W hi")                               \
    REG(0x04, V1_VCREG,  "Voice 1 control")                                  \
        FLD(V1_VCREG, V1_GATE, 0:0, "Gate",  Flag, 0, 0)                     \
        FLD(V1_VCREG, V1_SYNC, 1:1, "Sync",  Flag, 0, 0)                     \
        FLD(V1_VCREG, V1_RING, 2:2, "Ring",  Flag, 0, 0)                     \
        FLD(V1_VCREG, V1_TEST, 3:3, "Test",  Flag, 0, 0)                     \
    REG(0x05, V1_ATDCY,  "Voice 1 atk/decay")                                \
        FLD(V1_ATDCY, V1_ATK, 7:4, "Attack", Value, 0, 0)                    \
        FLD(V1_ATDCY, V1_DCY, 3:0, "Decay",  Value, 0, 0)                    \
    REG(0x06, V1_SUREL,  "Voice 1 sus/release")                              \
        FLD(V1_SUREL, V1_SUS, 7:4, "Sustain", Value, 0, 0)                   \
        FLD(V1_SUREL, V1_REL, 3:0, "Release", Value, 0, 0)                   \
    REG(0x07, V2_FRELO,  "Voice 2 freq lo")                                  \
    REG(0x08, V2_FREHI,  "Voice 2 freq hi")                                  \
    REG(0x09, V2_PWLO,   "Voice 2 pulse W lo")                               \
    REG(0x0A, V2_PWHI,   "Voice 2 pulse W hi")                               \
    REG(0x0B, V2_VCREG,  "Voice 2 control")                                  \
        FLD(V2_VCREG, V2_GATE, 0:0, "Gate",  Flag, 0, 0)                     \
        FLD(V2_VCREG, V2_SYNC, 1:1, "Sync",  Flag, 0, 0)                     \
        FLD(V2_VCREG, V2_RING, 2:2, "Ring",  Flag, 0, 0)                     \
        FLD(V2_VCREG, V2_TEST, 3:3, "Test",  Flag, 0, 0)                     \
    REG(0x0C, V2_ATDCY,  "Voice 2 atk/decay")                                \
        FLD(V2_ATDCY, V2_ATK, 7:4, "Attack", Value, 0, 0)                    \
        FLD(V2_ATDCY, V2_DCY, 3:0, "Decay",  Value, 0, 0)                    \
    REG(0x0D, V2_SUREL,  "Voice 2 sus/release")                              \
        FLD(V2_SUREL, V2_SUS, 7:4, "Sustain", Value, 0, 0)                   \
        FLD(V2_SUREL, V2_REL, 3:0, "Release", Value, 0, 0)                   \
    REG(0x0E, V3_FRELO,  "Voice 3 freq lo")                                  \
    REG(0x0F, V3_FREHI,  "Voice 3 freq hi")                                  \
    REG(0x10, V3_PWLO,   "Voice 3 pulse W lo")                               \
    REG(0x11, V3_PWHI,   "Voice 3 pulse W hi")                               \
    REG(0x12, V3_VCREG,  "Voice 3 control")                                  \
        FLD(V3_VCREG, V3_GATE, 0:0, "Gate",  Flag, 0, 0)                     \
        FLD(V3_VCREG, V3_SYNC, 1:1, "Sync",  Flag, 0, 0)                     \
        FLD(V3_VCREG, V3_RING, 2:2, "Ring",  Flag, 0, 0)                     \
        FLD(V3_VCREG, V3_TEST, 3:3, "Test",  Flag, 0, 0)                     \
    REG(0x13, V3_ATDCY,  "Voice 3 atk/decay")                                \
        FLD(V3_ATDCY, V3_ATK, 7:4, "Attack", Value, 0, 0)                    \
        FLD(V3_ATDCY, V3_DCY, 3:0, "Decay",  Value, 0, 0)                    \
    REG(0x14, V3_SUREL,  "Voice 3 sus/release")                              \
        FLD(V3_SUREL, V3_SUS, 7:4, "Sustain", Value, 0, 0)                   \
        FLD(V3_SUREL, V3_REL, 3:0, "Release", Value, 0, 0)                   \
    REG(0x15, CUTLO,     "Filter cutoff lo")                                  \
    REG(0x16, CUTHI,     "Filter cutoff hi")                                  \
    REG(0x17, RESON,     "Filter reso/routing")                               \
        FLD(RESON,  FILT1,  0:0, "Filt Voice 1",  Flag,  0, 0)               \
        FLD(RESON,  FILT2,  1:1, "Filt Voice 2",  Flag,  0, 0)               \
        FLD(RESON,  FILT3,  2:2, "Filt Voice 3",  Flag,  0, 0)               \
        FLD(RESON,  FILTEX, 3:3, "Filt External", Flag,  0, 0)               \
        FLD(RESON,  RES,    7:4, "Resonance",     Value, 0, 0)               \
    REG(0x18, SIGVOL,    "Filter mode/volume")                                \
        FLD(SIGVOL, VOLUME, 3:0, "Volume",      Value, 0, 0)                 \
        FLD(SIGVOL, LP,     4:4, "Low Pass",    Flag,  0, 0)                 \
        FLD(SIGVOL, BP,     5:5, "Band Pass",   Flag,  0, 0)                 \
        FLD(SIGVOL, HP,     6:6, "High Pass",   Flag,  0, 0)                 \
        FLD(SIGVOL, V3OFF,  7:7, "Voice 3 Off", Flag,  0, 0)                 \
    REG(0x19, POTX,      "Paddle X (read)")                                   \
    REG(0x1A, POTY,      "Paddle Y (read)")                                   \
    REG(0x1B, OSC3,      "Osc 3 output (read)")                               \
    REG(0x1C, ENV3,      "Env 3 output (read)")                               \
    REG(0x1D, R1D,       "-")                                                 \
    REG(0x1E, R1E,       "-")                                                 \
    REG(0x1F, R1F,       "-")

// --- Extract register constants ---
namespace sid_regs {
    SID_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
}

#ifdef CERMU_HAS_CHIP_DEBUG
DECL_EXTRACT_ALL(SID, SID_DECL)
#endif

// RESON register (0x17) bit fields
#define RESON_FILT1     0x01  // Route voice 1 through filter
#define RESON_FILT2     0x02  // Route voice 2 through filter
#define RESON_FILT3     0x04  // Route voice 3 through filter
#define RESON_FILTEX    0x08  // Route external input through filter
#define RESON_RES_MASK  0xF0  // Resonance nibble (bits 4-7)
#define RESON_RES_SHIFT 4     // Shift to extract resonance value

// SIGVOL register (0x18) bit fields
#define SIGVOL_LP       0x10  // Low-pass filter enable
#define SIGVOL_BP       0x20  // Band-pass filter enable
#define SIGVOL_HP       0x40  // High-pass filter enable
#define SIGVOL_3OFF     0x80  // Voice 3 disconnect from output
#define SIGVOL_VOL_MASK 0x0F  // Master volume nibble (bits 0-3)

// SID constants — accumulator
#define WAVEFORM_ACCUMULATOR_MAX    0xFFFFFF   // 24-bit accumulator mask
#define WAVEFORM_ACCUMULATOR_MSB    0x800000   // Bit 23 (MSB)
#define ACC_BIT19                   0x080000   // Bit 19: noise LFSR clock source

// SID constants — oscillator output
#define OSCILLATOR_MAX              0xFFF      // 12-bit oscillator DAC range
#define OSCILLATOR_CENTER           2048       // Mid-point for signed centering

// SID constants — pulse / noise
#define NOISE_LFSR_MASK             0x7FFFFF   // 23-bit LFSR mask

// SID constants — sample buffer
#define SAMPLE_BUFFER_SIZE          8192       // SPSC ring buffer size

// SID constants — filter
#define FILTER_RESONANCE_MAX        15.0f

// SID constants — default clock / sample rate
#define SID_DEFAULT_CPU_CLOCK_PAL   985248.0f  // PAL C64 CPU clock (Hz)

// SID constants — audio output
// Voice DC offset for 6581 digi playback.  On real 6581 hardware each voice
// amplifier has a significant DC bias (~5 V operating point with 1.5 V signal
// swing).  The master volume register multiplies the ENTIRE mixer output
// (voices + DC).  When programs rapidly write $D418 the DC component is
// modulated, producing a 4-bit PCM "digi" signal.  We fold the three per-voice
// DC contributions into a single constant added to the mixer sum every cycle.
// The DC blocker (~20 Hz high-pass) removes the static DC×avg_vol product,
// leaving only the rapid volume variations as audible digi audio.
//
// Calibrated against reSID's 6581 voice_DC / voice_signal_max ratio (~3.33×).
// Real HW: each voice has 5.0V DC offset with 1.5V signal swing.  Three voices
// produce 15.0V total DC vs 4.5V peak signal → DC is 3.33× voice amplitude.
// Our model normalises 3 voices to ±1.0 peak, so the equivalent DC would be
// 3.33.  We reduce to 1.5 to compensate for our linear (non-compressed) output
// model vs the real 6581's nonlinear op-amp saturation in the mixer/gain stage.
// At 1.5, digi amplitude roughly equals voice amplitude after the DC blocker,
// matching the perceptual balance on real hardware.

// Unused/padding register range (reads as 0xFF)
#define SID_REG_UNUSED_START        0x1D

// Combined waveform lookup table size

// Ring buffer for sample output (SPSC: emulation thread writes, audio thread reads).
// write_pos and read_pos use std::atomic with release/acquire ordering to ensure
// correct cross-thread visibility with minimal overhead on x86 (acquire/release
// are free on x86; on ARM they emit the appropriate barriers).
struct ring_buffer_t {
    float* buffer;
    uint32_t size;
    std::atomic<uint32_t> write_pos;
    std::atomic<uint32_t> read_pos;
    uint32_t mask;

    // Methods
    void init(uint32_t size);
    void destroy();
    void write(float sample);
    float read();
    bool empty();
    uint32_t available() const;
};

// Filter state structure
struct filter_state_t {
    // ZDF (zero-delay feedback) topology-preserving SVF state.
    // Unlike the naive SVF, this formulation is unconditionally stable
    // at any cutoff/resonance combination — critical because we process
    // the filter at ~44.1 kHz (not every CPU cycle at ~1 MHz like reSID).
    float cutoff_frequency;
    float resonance;
    float low_pass_output;
    float band_pass_output;
    float high_pass_output;
    
    // ZDF SVF coefficients (recomputed when cutoff or resonance changes)
    float g;            // tan(π * fc / fs) — integrator gain
    float k;            // 1/Q — damping (same as old 'q')
    float a1;           // 1 / (1 + g*(g + k))
    float a2;           // g * a1
    float a3;           // g * a2
    float ic1eq;        // First integrator state (band-pass)
    float ic2eq;        // Second integrator state (low-pass)
    
    // Nonlinear distortion state (6581 specific)
    bool enable_distortion;
};

// Forward declaration (voice_t references parent mos6581_t)
struct mos6581_t;

// Voice structure - Enhanced with all SID features
struct voice_t {
    // Write-only voice register values
    uint16_t frequency;               // Voice frequency control (FRELO/FREHI)
    uint16_t pulse_waveform_width;    // Pulse waveform width (PWLO/PWHI)
    uint8_t control_reg;              // Raw VCREG byte (gate/sync/ring/test + waveform bits)
    uint8_t sustain_level;            // Sustain level (SUREL) - 8-bit (nibble duplicated)

    // Read-only voice register values
    uint8_t oscillator_output;        // Oscillator output (OSC3)
    uint8_t envelope_output;          // Envelope output (ENV3)

    // Internal state - Enhanced
    uint32_t waveform_accumulator;    // 24-bit phase accumulator
    uint32_t envelope_accumulator;    // Envelope timing accumulator
    envelope_cycle_t envelope_cycle;  // Current envelope state
    uint8_t envelope_amplitude;       // Current envelope amplitude (8-bit, like real SID)
    uint32_t oscillator_waveform;     // Current oscillator output
    
    // Envelope generation state
    uint32_t envelope_rate_counter;   // Rate counter for envelope timing
    uint32_t envelope_rate_period;    // Rate period for current cycle
    bool envelope_hold_zero;          // Hold envelope at zero (reSID hold_zero)
    uint8_t exponential_counter;      // Exponential counter for decay/release
    uint8_t exponential_counter_period; // Period for exponential counter
    uint32_t envelope_pipeline;       // Pipeline delay for envelope counter change (reSID)
    uint32_t exponential_pipeline;    // Pipeline delay for exponential counter check
    bool reset_rate_counter;          // Deferred rate counter reset (reSID pipeline)
    int32_t state_pipeline;           // Pipeline delay for envelope state transitions (reSID)
    envelope_cycle_t envelope_next_state; // Next state for deferred state transition
    
    // Noise generation state
    uint32_t noise_lfsr;              // 23-bit LFSR state
    uint32_t noise_output;            // Current noise output
    bool noise_clock_enable;          // Noise clock enable from accumulator
    uint32_t shift_pipeline;          // 2-cycle pipeline delay for noise shift
    uint32_t shift_register_reset;    // Countdown for test bit LFSR fade to 0x7FFFFF
    
    // Waveform generation state
    uint32_t triangle_output;         // Triangle waveform output
    uint32_t sawtooth_output;         // Sawtooth waveform output
    uint32_t pulse_output;            // Pulse waveform output
    uint32_t combined_output;         // Combined waveform output
    
    // Sync state
    uint32_t prev_accumulator;        // Previous accumulator for sync detection
    bool sync_trigger;                // Sync trigger flag
    
    // Voice result and timing
    uint32_t result;                  // Final voice output
    float cpu_clock;                  // CPU clock frequency
    uint32_t voice_index;             // Voice index (0, 1, 2)
    int model_index = 0;             // Cached chip model (0=6581, 1=8580) for table lookup

    // Cached waveform lookup state — updated on control register or model change
    // to avoid per-cycle recomputation of table address and bitmasks.
    const uint16_t* cached_wave_table = nullptr;  // &model_wave[model][wf_index][0]
    uint32_t cached_ring_msb_mask = 0;  // Ring mod MSB mask (waveform-dependent)
    uint32_t cached_no_pulse_mask = 0xFFF;  // 0x000 when pulse selected, 0xFFF otherwise
    uint32_t cached_no_noise_mask = 0xFFF;  // 0x000 when noise selected, 0xFFF otherwise
    uint8_t cached_wf_mask = 0;       // control_reg & WAVEFORM_MASK (non-zero = waveform active)
    
    // Reference to parent chip
    mos6581_t* sid;

    // Methods
    void reset();
    void clock_cycle();
    void apply_sync(voice_t* sync_source, voice_t* sync_source_source);
    void set_waveform_output(voice_t* ring_source);
    void write_pulse_waveform_width(uint16_t value);
    void write_control_register_value(uint8_t value);
    void write_attack_decay_register_value(uint8_t value);
    void write_sustain_release_register_value(uint8_t value);
    int cycles_per_millisecond();

    static uint32_t rate_to_period(int rate);
    void update_cached_waveform_state();  // Refresh cached_wave_table, masks from control_reg

private:
    void update_exponential_period();
    void envelope_clock();
    void envelope_state_change();
};

// Main SID chip structure - Enhanced (C++ class inheriting ChipBase)
struct mos6581_t : public SoundChipBase {
    mos6581_t() : SoundChipBase(ChipInfo{"MOS6581", "MOS Technology"}) {
    }

    // Bus interface
    bus_cycle_ops_t bus_interface = {};

    // SID register array
    uint8_t regs[sid_constants::REGS_SIZE] = {};
    uint8_t bus_value = 0;            // Last bus value for read-only registers

    // Three voices with cross-references
    voice_t voice1 = {};
    voice_t voice2 = {};
    voice_t voice3 = {};
    voice_t* voices[3] = {nullptr, nullptr, nullptr};  // Array for easy iteration

    // Filter state
    filter_state_t filter_state = {};
    uint16_t filter_cutoff_frequency = 0; // Filter cutoff frequency (CUTLO/CUTHI)

    // Cached decoded routing from RESON and SIGVOL registers.
    // Updated on register writes; avoids per-cycle reg[] + bit-test overhead.
    bool filt1 = false;             // Voice 1 routed through filter
    bool filt2 = false;             // Voice 2 routed through filter
    bool filt3 = false;             // Voice 3 routed through filter
    bool filtex = false;            // External input routed through filter
    bool voice3_off = false;        // Voice 3 output disabled
    bool filter_lp = false;         // Low-pass filter output enabled
    bool filter_bp = false;         // Band-pass filter output enabled
    bool filter_hp = false;         // High-pass filter output enabled
    uint8_t master_volume = 0;      // Master volume (0-15)

    // Timing and sample generation
    uint32_t cycle_count = 0;         // Cycle counter
    uint32_t subcycle_count = 0;      // Sub-cycle counter
    bool pal_timing = false;          // PAL (true) vs NTSC (false) timing
    float sample_rate = 0.0f;         // Output sample rate
    float sid_rate = 0.0f;            // Internal SID update rate
    
    // Sample output
    ring_buffer_t sample_buffer = {}; // Ring buffer for samples
    
    // Chip revision and features
    sid_revision_t revision = SID_REVISION_6581_R4AR; // SID chip revision
    bool enable_filter = false;       // Filter enable flag
    bool enable_distortion = false;   // Distortion enable (6581 specific)
    bool enable_digiboost = false;    // Digital boost for 4-bit samples
    
    // External input
    float external_input = 0.0f;      // External audio input level
    
    // POT interface
    uint8_t pot_x_value = 0;          // POT X value
    uint8_t pot_y_value = 0;          // POT Y value
    
    // Fractional sample accumulator for cycle-accurate output
    float sample_accumulator = 0.0f;   // Fractional accumulator for sample generation
    float sample_rate_ratio = 0.0f;    // Precomputed sample_rate / cpu_clock
    float cpu_clock = 0.0f;           // CPU clock frequency (e.g. 985248 for PAL)
    
    // CIC-3 (3rd-order Cascaded Integrator-Comb) decimation filter.
    // Three cascaded running sums give a triangular→B-spline window with
    // -39 dB sidelobes (vs -13 dB for a box filter), at near-zero cost:
    // just two extra float additions per cycle.  This is the primary
    // anti-alias mechanism for the 985 kHz → 44.1 kHz downsampling.
    float cic_s1 = 0.0f;             // 1st integrator (box filter)
    float cic_s2 = 0.0f;             // 2nd integrator (triangular window)
    float cic_s3 = 0.0f;             // 3rd integrator (B-spline window)
    uint32_t sample_cycle_count = 0;  // Cycles accumulated since last sample
    
    // DC blocker state for clean audio output (removes constant DC,
    // preserves fast changes for volume-register digi playback)
    float dc_blocker_prev_in = 0.0f;  // Previous input to DC blocker
    float dc_blocker_prev_out = 0.0f; // Previous output from DC blocker
    
    // Statistics and debugging
    uint32_t total_cycles = 0;        // Total cycles processed
    uint32_t samples_generated = 0;   // Total samples generated

    // Optional write-capture callback — called for each register write.
    // Signature: callback(context, cycle, register, value)
    // When non-null, registers_write() invokes this after updating state.
    // Caller owns the context; SID does not allocate or free.
    void (*write_capture_fn)(void*, uint32_t, uint8_t, uint8_t) = nullptr;
    void* write_capture_ctx = nullptr;

    // Destructor — cleans up ring buffer
    ~mos6581_t() override;

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif

    // ChipBase interface
#ifdef CERMU_HAS_GUI
    bool has_settings_content() const override { return true; }
    void render_settings_content() override;
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // Public methods
    void init();
    void reset();
    bus_state_t tick(bus_state_t bus_state);
    void generate_samples(float* output, uint32_t sample_count);
    void set_revision(sid_revision_t revision);
    void set_timing(bool pal_timing);
    void set_sample_rate(float sample_rate);
    void set_cpu_clock(float clock_hz);

    // Static methods for C function pointer compatibility (io_page_handlers_t)
    static bus_state_t registers_read(void* context, bus_state_t bus_state);
    static bus_state_t registers_write(void* context, bus_state_t bus_state);

private:
    // Internal helpers
    void filter_update_cutoff();
    float filter_process(float input);
    void filter_reset();
    void filter_init();
    void write_resonance_control_register_value(uint8_t value);
    bus_state_t advance_cycle(bus_state_t bus_state);
    uint32_t calculate_envelope_time_ms(voice_t* v, envelope_cycle_t cycle, uint8_t rate_index);
    
};
