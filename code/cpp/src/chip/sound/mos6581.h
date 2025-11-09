#pragma once

#include "../../core/chip.h"
#include "../../core/bus_cycle_interface.h"
#include <stdint.h>
#include "../../core/system_lines.h" // For bus_state_t
#include <stdint.h>
#include <stdbool.h>
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

// SID chip revisions
typedef enum {
    SID_REVISION_6581_R1,
    SID_REVISION_6581_R2,
    SID_REVISION_6581_R3,
    SID_REVISION_6581_R4,
    SID_REVISION_6581_R4AR,
    SID_REVISION_8580_R5,
    SID_REVISION_CSG_6581,
    SID_REVISION_CSG_8580
} sid_revision_t;

// Voice envelope cycle states
typedef enum {
    CYCLE_OFF = 0,     // Off cycle (0)
    CYCLE_ATTACK = 1,  // Attack cycle (1)
    CYCLE_DECAY = 2,   // Decay cycle (2)
    CYCLE_SUSTAIN = 3, // Sustain cycle (3)
    CYCLE_RELEASE = 4  // Release cycle (4)
} envelope_cycle_t;

// Waveform types
typedef enum {
    WAVEFORM_NONE = 0x0,     // No waveform
    WAVEFORM_TRIANGLE = 0x1, // Triangle waveform
    WAVEFORM_SAWTOOTH = 0x2, // Sawtooth waveform
    WAVEFORM_PULSE = 0x4,    // Pulse waveform
    WAVEFORM_NOISE = 0x8     // Noise waveform
} waveform_bits_t;

// SID constants
#define WAVEFORM_ACCUMULATOR_MAX 0xFFFFFF       // 24-bit accumulator
#define WAVEFORM_ACCUMULATOR_MSB 0x800000       // Bit 23 (MSB)
#define OSCILLATOR_MAX 0xFFF                    // 12-bit oscillator output
#define ENVELOPE_MAX 0xFFFF                     // 16-bit envelope output
#define PULSE_WIDTH_MAX 0xFFF                   // 12-bit pulse width
#define NOISE_LFSR_MASK 0x7FFFFF               // 23-bit LFSR mask
#define SAMPLE_BUFFER_SIZE (8192)              // Reduced buffer size

// Envelope rate table size (16 entries, 0-15)
#define ENVELOPE_RATE_TABLE_SIZE 16

// Filter constants
#define FILTER_CUTOFF_MAX 2048.0f
#define FILTER_RESONANCE_MAX 15.0f

// Combined waveform lookup table size
#define COMBINED_WAVEFORM_TABLE_SIZE 4096

// Forward declarations - Modern C++ style
struct mos6581_s;
struct filter_state_s;
struct ring_buffer_s;
using mos6581_t = mos6581_s;
using filter_state_t = filter_state_s;
using ring_buffer_t = ring_buffer_s;

// Ring buffer for sample output
typedef struct ring_buffer_s {
    float* buffer;
    uint32_t size;
    uint32_t write_pos;
    uint32_t read_pos;
    uint32_t mask;
} ring_buffer_t;

// Filter state structure
typedef struct filter_state_s {
    // Two-integrator-loop biquad filter state
    float cutoff_frequency;
    float resonance;
    float low_pass_output;
    float band_pass_output;
    float high_pass_output;
    float previous_input;
    float previous_low_pass;
    float previous_band_pass;
    
    // Filter coefficients
    float w0;           // Cutoff frequency coefficient
    float q;            // Resonance coefficient
    float integrator1;  // First integrator state
    float integrator2;  // Second integrator state
    
    // Nonlinear distortion state (6581 specific)
    float distortion_level;
    bool enable_distortion;
} filter_state_t;

// Voice structure - Enhanced with all SID features
typedef struct voice_s {
    // Write-only voice register values
    uint16_t frequency;               // Voice frequency control (FRELO/FREHI)
    uint16_t pulse_waveform_width;    // Pulse waveform width (PWLO/PWHI)
    waveform_bits_t waveform;         // Waveform control register (VCREG)
    bool gated;                       // Gate bit
    bool synchronize;                 // Oscillator sync
    bool ring_modulation;             // Ring modulation
    bool test;                        // Test bit
    uint8_t attack_rate;              // Attack rate (ATDCY)
    uint8_t decay_rate;               // Decay rate (ATDCY)
    uint16_t sustain_level;           // Sustain level (SUREL)
    uint8_t release_rate;             // Release rate (SUREL)

    // Read-only voice register values
    uint8_t oscillator_output;        // Oscillator output (OSC3)
    uint8_t envelope_output;          // Envelope output (ENV3)

    // Internal state - Enhanced
    uint32_t waveform_accumulator;    // 24-bit phase accumulator
    uint32_t envelope_accumulator;    // Envelope timing accumulator
    envelope_cycle_t envelope_cycle;  // Current envelope state
    uint16_t envelope_amplitude;      // Current envelope amplitude
    uint32_t oscillator_waveform;     // Current oscillator output
    
    // Envelope generation state
    uint32_t envelope_rate_counter;   // Rate counter for envelope timing
    uint32_t envelope_rate_period;    // Rate period for current cycle
    bool envelope_hold_zero;          // Hold envelope at zero during attack
    uint16_t envelope_next_level;     // Next level for envelope transitions
    
    // Noise generation state
    uint32_t noise_lfsr;              // 23-bit LFSR state
    uint32_t noise_output;            // Current noise output
    bool noise_clock_enable;          // Noise clock enable from accumulator
    
    // Waveform generation state
    uint32_t triangle_output;         // Triangle waveform output
    uint32_t sawtooth_output;         // Sawtooth waveform output
    uint32_t pulse_output;            // Pulse waveform output
    uint32_t combined_output;         // Combined waveform output
    
    // Sync and ring modulation state
    uint32_t prev_accumulator;        // Previous accumulator for sync detection
    bool sync_trigger;                // Sync trigger flag
    bool ring_msb;                    // Ring modulation MSB state
    
    // Voice result and timing
    uint32_t result;                  // Final voice output
    float cpu_clock;                  // CPU clock frequency
    uint32_t voice_index;             // Voice index (0, 1, 2)
    
    // Reference to parent chip
    mos6581_t* sid;
} voice_t;

// Main SID chip structure - Enhanced
typedef struct mos6581_s {
    // Chip descriptor must be first
    chip_descriptor_t* desc;

    // Bus interface
    bus_cycle_ops_t bus_interface;

    // SID register array
    uint8_t regs[SID_REGS_SIZE];
    uint8_t bus_value;                // Last bus value for read-only registers

    // Three voices with cross-references
    voice_t voice1;
    voice_t voice2;
    voice_t voice3;
    voice_t* voices[3];               // Array for easy iteration

    // Filter state
    filter_state_t filter_state;
    uint16_t filter_cutoff_frequency; // Filter cutoff frequency (CUTLO/CUTHI)
    uint8_t filter_resonance;         // Filter resonance control
    bool filter_voice1;               // Voice 1 filtered
    bool filter_voice2;               // Voice 2 filtered  
    bool filter_voice3;               // Voice 3 filtered
    bool filter_voice4;               // External input filtered

    // Volume and filter control
    uint8_t volume;                   // Master volume control
    bool low_pass_enabled;            // Low-pass filter enabled
    bool band_pass_enabled;           // Band-pass filter enabled
    bool high_pass_enabled;           // High-pass filter enabled
    bool voice3_disabled;             // Voice 3 output disabled

    // Timing and sample generation
    uint32_t cycle_count;             // Cycle counter
    uint32_t subcycle_count;          // Sub-cycle counter
    bool pal_timing;                  // PAL (true) vs NTSC (false) timing
    float sample_rate;                // Output sample rate
    float sid_rate;                   // Internal SID update rate
    
    // Sample output
    ring_buffer_t sample_buffer;      // Ring buffer for samples
    float* temp_buffer;               // Temporary buffer for processing
    uint32_t temp_buffer_size;        // Size of temporary buffer
    
    // Chip revision and features
    sid_revision_t revision;          // SID chip revision
    bool enable_filter;               // Filter enable flag
    bool enable_distortion;           // Distortion enable (6581 specific)
    bool enable_digiboost;            // Digital boost for 4-bit samples
    
    // Volume bug state (6581 specific)
    bool volume_change_click;         // Volume change click flag
    float volume_click_amplitude;     // Click amplitude
    uint32_t volume_click_counter;    // Click duration counter
    
    // External input
    float external_input;             // External audio input level
    
    // POT interface
    uint8_t pot_x_value;              // POT X value
    uint8_t pot_y_value;              // POT Y value
    
    // Combined waveform lookup tables
    uint8_t* combined_waveform_table; // Combined waveform lookup table
    bool combined_waveform_enabled;   // Combined waveform enable
    
    // Statistics and debugging
    uint32_t total_cycles;            // Total cycles processed
    uint32_t samples_generated;       // Total samples generated
    
} mos6581_t;

// Function declarations

// System functions
void mos6581_reset(mos6581_t* sid);

// Main cycle function with unified bus state threading
// Consolidated SID tick function - main entry point for cycle processing
bus_state_t mos6581_tick(void* chip, bus_state_t bus_state);

// Voice output
void mos6581_generate_samples(mos6581_t* sid, float* output, uint32_t sample_count);

// Utility functions
void mos6581_set_revision(mos6581_t* sid, sid_revision_t revision);
void mos6581_set_timing(mos6581_t* sid, bool pal_timing);
void mos6581_set_sample_rate(mos6581_t* sid, float sample_rate);
float mos6581_interpolate_sample(mos6581_t* sid, float position);

// Chip descriptor
extern chip_descriptor_t mos6581_descriptor;

#ifdef IMGUI_VERSION
// GUI function declarations
void mos6581_render_debug_window(void* chip, bool* show_window);
void mos6581_render_settings_window(void* chip, bool* show_window);
#endif
