#ifndef MOS6581_H
#define MOS6581_H

#include "../../core/chip.h"
#include "../../core/bus_cycle_interface.h"
#include <stdint.h>
#include <stdbool.h>

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

// Register dimensions
#define SID_REGS_BITS 5
#define SID_REGS_SIZE (1 << SID_REGS_BITS) // 32
#define SID_REGS_MASK (SID_REGS_SIZE - 1) // 31

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

// Constants from C# code
#define WAVEFORM_ACCUMULATOR_MSB 0x8000000       // Bit 23 (24th counting from 0)
#define WAVEFORM_ACCUMULATOR_MAX 0xFFFFFFF       // The highest 24 bit (12.12 fixed-point) number
#define OSCILLATOR_MAX 0xFFF                     // The highest 12 bit (8.4 fixed-point) number
#define AMPLITUDE_PEAK 0xFFFF                    // The highest 16 bit (8.8 fixed-point) number
#define DECAY_RELEASE_DIVIDER (-3)               // Decay and Release are decreases, and take 3 times longer than RatesinMS[Attack]
#define SAMPLE_BUFFER_SIZE (128 * 1024)          // Sample buffer size from C# code

// Envelope rate table size (16 entries, 0-15)
#define ENVELOPE_RATE_TABLE_SIZE 16

// Forward declarations
struct voice_s;
struct mos6581_s;

// Voice structure - matches C# Voice class
typedef struct voice_s {
    // Write-only voice register values :
    uint16_t frequency;               // Voice frequency control (FRELO/FREHI) - frequency of oscillator
    uint16_t pulse_waveform_width;    // Pulse waveform width (PWLO/PWHI) - pulse waveform duty cycle
    waveform_bits_t waveform;         // Waveform control register (VCREG) - voice waveform select
    bool gated;                       // Gate bit: 1=start attack/decay/sustain cycle, 0=start release cycle
    bool synchronize;                 // Oscillator sync: synchronize this oscillator with oscillator of the previous voice
    bool ring_modulation;             // Ring modulation: replace triangle waveform with ring modulation output
    bool test;                        // Test bit: 1=disable oscillator, reset noise generator
    uint8_t attack_rate;              // Attack rate (ATDCY): envelope attack rate control
    uint8_t decay_rate;               // Decay rate (ATDCY): envelope decay rate control
    uint16_t sustain_level;           // Sustain level (SUREL): envelope sustain level control (16-bit)
    uint8_t release_rate;             // Release rate (SUREL): envelope release rate control

    // Read-only voice register values :
    uint8_t oscillator_output;        // Oscillator output (OSC3): provides real-time oscillator output for voice 3
    uint8_t envelope_output;          // Envelope output (ENV3): provides real-time envelope output for voice 3    // Internal state from C# Voice class :
    uint32_t waveform_accumulator;    // Accumulator used to track the current position in the waveform generation cycle
    uint32_t envelope_accumulator;    // Accumulator used to track the current position in the envelope generation cycle
    envelope_cycle_t envelope_cycle;  // Current envelope generation cycle state: Attack, Decay, Sustain, or Release
    bool envelope_hold_zero;          // When true, holds the envelope output at zero during the attack phase
    uint8_t noise_output;             // Current output from the noise generator
    uint32_t noise_seed;              // Current state of the noise generator's linear feedback shift register
    uint16_t envelope_amplitude;      // Current envelope amplitude (16-bit 8.8 fixed-point)
    uint32_t oscillator_waveform;     // Current oscillator waveform output
    int envelope_deltas[ENVELOPE_RATE_TABLE_SIZE]; // Envelope rate deltas
    float cpu_clock;                  // CPU clock frequency
    uint32_t result;                  // Result field for voice calculations
    uint16_t envelope_next_level;     // Next level for envelope calculations

    // Reference to chip
    struct mos6581_s* sid;
} voice_t;

// MOS6581 SID structure - matches C# MOS6581 class
typedef struct mos6581_s {
    // Chip descriptor must be first
    chip_descriptor_t* desc;

    // Bus interface
    bus_cycle_ops_t bus_interface;

    // SID register array
    uint8_t regs[SID_REGS_SIZE];

    // Three voices
    voice_t voice1;
    voice_t voice2;
    voice_t voice3;

    // Frequency cutoff
    uint16_t filter_cutoff_frequency; // Filter cutoff frequency (CUTLO/CUTHI) - frequency where filter starts to have effect

    // Filter control
    uint8_t filter_resonance;         // Filter resonance control, also controls external filter input
    bool filter_voice1;               // Voice 1 filtered, 1=voice 1 filtered
    bool filter_voice2;               // Voice 2 filtered, 1=voice 2 filtered  
    bool filter_voice3;               // Voice 3 filtered, 1=voice 3 filtered
    bool filter_voice4;               // Filter external input signal, 1=external input filtered

    // Volume control  
    uint8_t volume;                   // Master volume control, also controls external filter input

    // Filter switches
    bool low_pass_enabled;            // Select low-pass filter, 1=low-pass on
    bool band_pass_enabled;           // Select band-pass filter, 1=band-pass on
    bool high_pass_enabled;           // Select high-pass filter, 1=high-pass on
    bool voice3_disabled;             // Disconnect output of voice 3, 1=voice 3 off

    // Internal state from C# MOS6581 class
    uint32_t filter_voice_count;      // Number of voices being filtered
    uint32_t sample_index;            // Current sample buffer index
    uint8_t sample_buffer[SAMPLE_BUFFER_SIZE]; // Sample buffer
} mos6581_t;

// Function declarations
void* mos6581_system_create(chip_descriptor_t* desc);
void mos6581_system_destroy(void* chip);
void mos6581_reset(mos6581_t* sid);
uint8_t mos6581_registers_read(void* context, uint16_t address);
void mos6581_registers_write(void* context, uint16_t address, uint8_t value);
void mos6581_cycle(mos6581_t* sid);

// Voice functions
void voice_write_pulse_waveform_width(voice_t* voice, uint16_t value);
void voice_write_voice_control_register_value(voice_t* voice, uint8_t value);
void voice_write_attack_decay_register_value(voice_t* voice, uint8_t value);
void voice_write_sustain_release_register_value(voice_t* voice, uint8_t value);
int voice_rate_to_delta(voice_t* voice, int rate);
void voice_clock_cycle(voice_t* voice);
void voice_reset(voice_t* voice);
int voice_cycles_per_millisecond(voice_t* voice);

// Filter functions
void mos6581_write_resonance_control_register_value(mos6581_t* sid, uint8_t value);
void mos6581_write_volume_and_filter_select_register_value(mos6581_t* sid, uint8_t value);

// Pin check function
bool mos6581_pin_read(mos6581_t* sid, mos6581_pin_t pin);

// Chip descriptor
extern chip_descriptor_t mos6581_descriptor;

#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
// GUI function declarations
void mos6581_render_debug_window(void* chip, bool* show_window);
void mos6581_render_settings_window(void* chip, bool* show_window);
#endif

#endif // MOS6581_H
