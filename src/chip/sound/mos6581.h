#pragma once

#include "../../core/chip.h"
#include "../../core/bus_cycle_interface.h"
#include "../../core/system_lines.h" // For bus_state_t
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// SID MOS 6581 DIP has 28 pins; Pinout:
typedef enum {
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
} mos6581_pin_t;

// Register dimensions - Modern C++ constants
namespace sid_constants {
    constexpr uint8_t REGS_BITS = 5;
    constexpr uint8_t REGS_SIZE = (1 << REGS_BITS); // 32
}

// Legacy macro compatibility
#define SID_REGS_BITS sid_constants::REGS_BITS
#define SID_REGS_SIZE sid_constants::REGS_SIZE
#define SID_REGS_MASK (SID_REGS_SIZE - 1) // 31

// SID chip revisions — only revisions that produce different emulation
// behaviour are active.  The rest are commented out until per-revision
// differences (combined-waveform tables, filter curves, etc.) are modelled.
typedef enum {
    // SID_REVISION_6581_R1,     // same behaviour as R4AR for now
    // SID_REVISION_6581_R2,     // same behaviour as R4AR for now
    // SID_REVISION_6581_R3,     // same behaviour as R4AR for now
    // SID_REVISION_6581_R4,     // same behaviour as R4AR for now
    SID_REVISION_6581_R4AR,      // MOS 6581 family (filter distortion, volume-click)
    SID_REVISION_8580_R5,        // MOS 8580 family (clean filter, no volume-click)
    // SID_REVISION_CSG_6581,    // same behaviour as R4AR for now
    // SID_REVISION_CSG_8580     // same behaviour as 8580_R5 for now
} sid_revision_t;

// Voice envelope cycle states
typedef enum {
    CYCLE_OFF = 0,     // Off cycle (0)
    CYCLE_ATTACK = 1,  // Attack cycle (1)
    CYCLE_DECAY = 2,   // Decay cycle (2)
    CYCLE_SUSTAIN = 3, // Sustain cycle (3)
    CYCLE_RELEASE = 4  // Release cycle (4)
} envelope_cycle_t;

// Waveform bits — positioned at their VCREG bit locations (bits 4-7)
// so hot-path tests use control_reg & WAVEFORM_xxx with no shift.
typedef enum {
    WAVEFORM_NONE     = 0x00, // No waveform
    WAVEFORM_TRIANGLE = 0x10, // Triangle waveform (VCREG bit 4)
    WAVEFORM_SAWTOOTH = 0x20, // Sawtooth waveform (VCREG bit 5)
    WAVEFORM_PULSE    = 0x40, // Pulse waveform    (VCREG bit 6)
    WAVEFORM_NOISE    = 0x80, // Noise waveform    (VCREG bit 7)
    WAVEFORM_MASK     = 0xF0  // All waveform bits
} waveform_bits_t;

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
#define ACC_POWERUP_VALUE           0x555555   // VICE: even bits high on power-up
#define ACC_BIT19                   0x080000   // Bit 19: noise LFSR clock source

// SID constants — oscillator output
#define OSCILLATOR_MAX              0xFFF      // 12-bit oscillator DAC range
#define OSCILLATOR_CENTER           2048       // Mid-point for signed centering
#define OSCILLATOR_SHIFT_TRI        11         // 24-bit acc → 12-bit triangle
#define OSCILLATOR_SHIFT_SAW        12         // 24-bit acc → 12-bit sawtooth

// SID constants — envelope
#define ENVELOPE_MAX                0xFF       // 8-bit envelope output
#define ENVELOPE_RATE_TABLE_SIZE    16         // Rate lookup entries (0-15)
#define ENVELOPE_RATE_OVERFLOW      0x8000     // 15-bit rate counter overflow

// SID constants — pulse / noise
#define PULSE_WIDTH_MAX             0xFFF      // 12-bit pulse width
#define NOISE_LFSR_MASK             0x7FFFFF   // 23-bit LFSR mask
#define NOISE_LFSR_RESET            0x7FFFFE   // LFSR value after chip reset
#define NOISE_LFSR_TEST             0x7FFFFF   // LFSR value when test bit set

// SID constants — sample buffer
#define SAMPLE_BUFFER_SIZE          8192       // SPSC ring buffer size

// SID constants — filter
#define FILTER_CUTOFF_MAX           2048.0f
#define FILTER_RESONANCE_MAX        15.0f

// SID constants — audio output
#define SID_6581_DIGI_BIAS          0.13f      // DC bias for 6581 digi playback
#define DC_BLOCKER_ALPHA            0.997f     // ~20 Hz high-pass coefficient
#define SIGVOL_VOL_MAX              15.0f      // Maximum master volume (4-bit)
#define VOLUME_CLICK_DURATION       1000       // Volume-bug click duration (cycles)

// Voice register addressing helpers
#define SID_VOICE_REG_COUNT         (3 * VOICE_REGS) // Total voice registers (21)

// Unused/padding register range (reads as 0xFF)
#define SID_REG_UNUSED_START        0x1D
#define SID_REG_UNUSED_END          0x1F

// Combined waveform lookup table size
// Forward declarations - Modern C++ style
struct mos6581_s;
struct filter_state_s;
struct ring_buffer_s;
using mos6581_t = mos6581_s;
using filter_state_t = filter_state_s;
using ring_buffer_t = ring_buffer_s;

// Ring buffer for sample output (SPSC: emulation thread writes, audio thread reads).
// write_pos and read_pos are volatile to prevent the compiler from caching them
// in registers across function calls — essential for correct cross-thread visibility
// in Release builds (-O2+).
typedef struct ring_buffer_s {
    float* buffer;
    uint32_t size;
    volatile uint32_t write_pos;
    volatile uint32_t read_pos;
    uint32_t mask;
} ring_buffer_t;

// Filter state structure
typedef struct filter_state_s {
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
} filter_state_t;

// Voice structure - Enhanced with all SID features
typedef struct voice_s {
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
    
    // Noise generation state
    uint32_t noise_lfsr;              // 23-bit LFSR state
    uint32_t noise_output;            // Current noise output
    bool noise_clock_enable;          // Noise clock enable from accumulator
    
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
    
    // Reference to parent chip
    mos6581_t* sid;
} voice_t;

// Main SID chip structure - Enhanced (C++ class inheriting ChipBase)
typedef struct mos6581_s : public ChipBase {

    // Bus interface
    bus_cycle_ops_t bus_interface = {};

    // SID register array
    uint8_t regs[SID_REGS_SIZE] = {};
    uint8_t bus_value = 0;            // Last bus value for read-only registers

    // Three voices with cross-references
    voice_t voice1 = {};
    voice_t voice2 = {};
    voice_t voice3 = {};
    voice_t* voices[3] = {nullptr, nullptr, nullptr};  // Array for easy iteration

    // Filter state
    filter_state_t filter_state = {};
    uint16_t filter_cutoff_frequency = 0; // Filter cutoff frequency (CUTLO/CUTHI)
    // Decoded fields for RESON and SIGVOL registers are read from
    // regs[SID_REG_RESON] / regs[SID_REG_SIGVOL] at sample-rate;
    // no pre-decoded copies needed.

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
    
    // Volume bug state (6581 specific)
    bool volume_change_click = false; // Volume change click flag
    float volume_click_amplitude = 0.0f; // Click amplitude
    uint32_t volume_click_counter = 0;   // Click duration counter
    
    // External input
    float external_input = 0.0f;      // External audio input level
    
    // POT interface
    uint8_t pot_x_value = 0;          // POT X value
    uint8_t pot_y_value = 0;          // POT Y value
    
    // Fractional sample accumulator for cycle-accurate output
    double sample_accumulator = 0.0;  // Fractional accumulator for sample generation
    float cpu_clock = 0.0f;           // CPU clock frequency (e.g. 985248 for PAL)
    
    // Per-cycle filter output accumulation for anti-aliased downsampling.
    // The filter runs every CPU cycle (~1 MHz); the accumulated output is
    // averaged at sample time (~44.1 kHz) for band-limited resampling.
    double output_acc = 0.0;          // Accumulated post-filter mixed output
    uint32_t sample_cycle_count = 0;  // Cycles accumulated since last sample
    
    // DC blocker state for clean audio output (removes constant DC,
    // preserves fast changes for volume-register digi playback)
    float dc_blocker_prev_in = 0.0f;  // Previous input to DC blocker
    float dc_blocker_prev_out = 0.0f; // Previous output from DC blocker
    
    // Statistics and debugging
    uint32_t total_cycles = 0;        // Total cycles processed
    uint32_t samples_generated = 0;   // Total samples generated

    // Destructor — cleans up ring buffer
    ~mos6581_s() override;

    // ChipBase interface
    ChipIdentity chip_identity() const override;
    bool has_debug_content() const override { return true; }
    bool has_settings_content() const override { return true; }
    bool has_layout_content() const override { return true; }
    void render_debug_content() override;
    void render_settings_content() override;
    void render_layout_content() override;
    
} mos6581_t;

// Function declarations

// System functions
void mos6581_reset(mos6581_t* sid);

// Main cycle function with unified bus state threading
// Consolidated SID tick function - main entry point for cycle processing
bus_state_t mos6581_tick(void* chip, bus_state_t bus_state);

// Voice output
void mos6581_generate_samples(mos6581_t* sid, float* output, uint32_t sample_count);

// Register I/O functions
bus_state_t mos6581_registers_read(void* context, bus_state_t bus_state);
bus_state_t mos6581_registers_write(void* context, bus_state_t bus_state);

// Utility functions
void mos6581_set_revision(mos6581_t* sid, sid_revision_t revision);
void mos6581_set_timing(mos6581_t* sid, bool pal_timing);
void mos6581_set_sample_rate(mos6581_t* sid, float sample_rate);
void mos6581_set_cpu_clock(mos6581_t* sid, float clock_hz);

// Typed lifecycle functions
mos6581_t* mos6581_create();
void mos6581_destroy(mos6581_t* sid);

// Legacy GUI wrappers (deprecated — use ChipBase virtual methods)
extern "C" {
void mos6581_render_debug_content(void* chip);
void mos6581_render_settings_content(void* chip);
void mos6581_render_layout_content(void* chip);
}
