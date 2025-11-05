#include "mos6581.h"
#include "../../gui/cimgui_interface.h"
#include "../../gui/generic_chip_gui.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

static const char* waveform_names[] = {
    "None",
    "Triangle", 
    "Sawtooth",
    "Sawtooth+Triangle",
    "Pulse",
    "Pulse+Triangle", 
    "Pulse+Sawtooth",
    "Pulse+Sawtooth+Triangle",
    "Noise"
};

static const char* envelope_cycle_names[] = {
    "Attack",
    "Decay", 
    "Sustain",
    "Release",
    "Off"
};

static void render_voice_debug(voice_t* voice, int voice_num) {
    igPushID_Int(voice_num);
    
    char voice_header[32];
    snprintf(voice_header, sizeof(voice_header), "Voice %d", voice_num);
    
    if (igCollapsingHeader_BoolPtr(voice_header, NULL, 0)) {
        igIndent(16.0f);
        
        // Write-only voice register values :
        igText("Frequency: $%04X (%d)", voice->frequency, voice->frequency);
        igText("Pulse Waveform Width: $%04X (%d)", voice->pulse_waveform_width, voice->pulse_waveform_width);
        
        // Values updated by WriteVoiceControlRegisterValue()
        igText("Gated: %s", voice->gated ? "Yes" : "No");
        igText("Synchronize: %s", voice->synchronize ? "Yes" : "No");
        igText("Ring Modulation: %s", voice->ring_modulation ? "Yes" : "No");
        igText("Test: %s", voice->test ? "Yes" : "No");
        const char* waveform_name = ((int)voice->waveform < 9) ? waveform_names[(int)voice->waveform] : "Unknown";
        igText("Waveform: %s (%d)", waveform_name, (int)voice->waveform);
        
        igSeparator();
        
        // Outside readable variables (albeit after shifting)
        igText("Envelope Amplitude: $%04X (%d)", voice->envelope_amplitude, voice->envelope_amplitude);
        igText("Oscillator Waveform: $%04X (%d)", voice->oscillator_waveform, voice->oscillator_waveform);
        igText("Result: %d", voice->result);
        
        igSeparator();
        
        // Internal state
        igText("Waveform Accumulator: $%06X (%d)", voice->waveform_accumulator, voice->waveform_accumulator);
        const char* cycle_name = ((int)voice->envelope_cycle < 5) ? envelope_cycle_names[(int)voice->envelope_cycle] : "Unknown";
        igText("Envelope Cycle: %s (%d)", cycle_name, (int)voice->envelope_cycle);
        igText("Envelope Next Level: %d", voice->envelope_next_level);
        igText("Sustain Level: %d", voice->sustain_level);
        
        igText("CPU Clock: %.0f Hz", voice->cpu_clock);

        igUnindent(16.0f);
    }
    
    igPopID();
}

// ============================================================================
// MOS6581 SID LAYOUT (28-pin DIP)
// ============================================================================

inline ChipLayout create_mos6581_layout() {
    // Start with DIP-28 base layout
    ChipLayout layout = create_dip28_layout();
    
    // Update package info for MOS6581 SID
    layout.markings = {
        "MOS6581",                   // part_number
        "MOS Technology",            // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Clear default pins and create hardware-accurate MOS6581 SID pinout (28-pin DIP)
    layout.left_pins.clear();
    layout.right_pins.clear();
    
    // Left side pins (1-14)
    layout.left_pins.push_back(make_pin(1,  PinLabel::CAP1A, nullptr));   // Filter Cap 1A
    layout.left_pins.push_back(make_pin(2,  PinLabel::CAP1B, nullptr));   // Filter Cap 1B
    layout.left_pins.push_back(make_pin(3,  PinLabel::CAP2A, nullptr));   // Filter Cap 2A
    layout.left_pins.push_back(make_pin(4,  PinLabel::CAP2B, nullptr));   // Filter Cap 2B
    layout.left_pins.push_back(make_pin(5,  PinLabel::RES, nullptr));     // Reset
    layout.left_pins.push_back(make_pin(6,  PinLabel::PHI2, nullptr));    // Clock
    layout.left_pins.push_back(make_pin(7,  PinLabel::RW, nullptr));      // Read/Write
    layout.left_pins.push_back(make_pin(8,  PinLabel::CS, nullptr));      // Chip Select
    layout.left_pins.push_back(make_pin(9,  PinLabel::A0, nullptr));      // Address 0
    layout.left_pins.push_back(make_pin(10, PinLabel::A1, nullptr));      // Address 1
    layout.left_pins.push_back(make_pin(11, PinLabel::A2, nullptr));      // Address 2
    layout.left_pins.push_back(make_pin(12, PinLabel::A3, nullptr));      // Address 3
    layout.left_pins.push_back(make_pin(13, PinLabel::A4, nullptr));      // Address 4
    layout.left_pins.push_back(make_pin(14, PinLabel::VSS, nullptr));     // Ground
    
    // Right side pins (15-28)
    layout.right_pins.push_back(make_pin(28, PinLabel::VDD, nullptr));    // +12V Power
    layout.right_pins.push_back(make_pin(27, PinLabel::AUDIO_OUT, nullptr)); // Audio Output
    layout.right_pins.push_back(make_pin(26, PinLabel::EXT_IN, nullptr)); // External Input
    layout.right_pins.push_back(make_pin(25, PinLabel::VCC, nullptr));    // +5V Power
    layout.right_pins.push_back(make_pin(24, PinLabel::POTX, nullptr));   // Paddle X
    layout.right_pins.push_back(make_pin(23, PinLabel::POTY, nullptr));   // Paddle Y
    layout.right_pins.push_back(make_pin(22, PinLabel::D7, nullptr));     // Data 7
    layout.right_pins.push_back(make_pin(21, PinLabel::D6, nullptr));     // Data 6
    layout.right_pins.push_back(make_pin(20, PinLabel::D5, nullptr));     // Data 5
    layout.right_pins.push_back(make_pin(19, PinLabel::D4, nullptr));     // Data 4
    layout.right_pins.push_back(make_pin(18, PinLabel::D3, nullptr));     // Data 3
    layout.right_pins.push_back(make_pin(17, PinLabel::D2, nullptr));     // Data 2
    layout.right_pins.push_back(make_pin(16, PinLabel::D1, nullptr));     // Data 1
    layout.right_pins.push_back(make_pin(15, PinLabel::D0, nullptr));     // Data 0
    
    return layout;
}

// ============================================================================
// MOS6581 SID GUI DEBUG WINDOW
// ============================================================================
// Callback functions for generic chip GUI
static ChipLayout get_sid_layout(void* chip) {
    return create_mos6581_layout();
}

static void get_sid_pin_states(void* chip, ChipLayout* layout, bus_state_t bus_state, struct PinSignalState* pin_states) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid || !layout || !pin_states) return;
    
    int total_pins = 28; // SID is 28-pin DIP
    
    // Initialize all pins as inactive by default
    for (int i = 0; i < total_pins; i++) {
        pin_states[i].pin_number = i + 1;
        pin_states[i].signal_level = false;
        pin_states[i].drive_direction = false;
        pin_states[i].signal_value = 0;
        pin_states[i].high_impedance = false;
        pin_states[i].has_pullup = false;
        pin_states[i].has_pulldown = false;
        pin_states[i].signal_valid = true;
        pin_states[i].analog_voltage = 0.0f;
        pin_states[i].is_pwm = false;
        pin_states[i].pwm_duty_cycle = 0.0f;
    }
    
    // Set power pins as active
    pin_states[13].signal_level = false; // VSS (Ground, pin 14)
    pin_states[24].signal_level = true;  // VCC (+5V, pin 25)
    pin_states[27].signal_level = true;  // VDD (+12V, pin 28)
    
    // Audio output pin should be active if SID is producing sound
    pin_states[26].signal_level = true;  // AUDIO_OUT (pin 27)
    pin_states[26].drive_direction = true;
}

static void render_sid_specific_content(void* chip) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid) return;
    
    // This replaces the right column content from the original function
    igText("MOS 6581 SID (Sound Interface Device)");
    igText("SID MOS 6581 DIP has 28 pins");
    igSeparator();
    
    // Voices section
    if (igCollapsingHeader_BoolPtr("Voices", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        render_voice_debug(&sid->voice1, 1);
        render_voice_debug(&sid->voice2, 2);
        render_voice_debug(&sid->voice3, 3);
    }
    
    igSeparator();
    
    // Filter and Global Settings
    if (igCollapsingHeader_BoolPtr("Filter & Global Settings", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        // Write-only register values :
        igText("Filter Cutoff Frequency: $%03X (%d)", sid->filter_cutoff_frequency, sid->filter_cutoff_frequency);
        igText("Filter Voice 1: %s", sid->filter_voice1 ? "Yes" : "No");
        igText("Filter Voice 2: %s", sid->filter_voice2 ? "Yes" : "No");
        igText("Filter Voice 3: %s", sid->filter_voice3 ? "Yes" : "No");
        igText("Filter Voice 4 (External): %s", sid->filter_voice4 ? "Yes" : "No");
        igText("Filter Resonance: %d (0-15)", sid->filter_resonance);
        igText("Volume: %d (0-15)", sid->volume);
        igText("Low Pass Enabled: %s", sid->low_pass_enabled ? "Yes" : "No");
        igText("Band Pass Enabled: %s", sid->band_pass_enabled ? "Yes" : "No");
        igText("High Pass Enabled: %s", sid->high_pass_enabled ? "Yes" : "No");
        igText("Voice 3 Disabled: %s", sid->voice3_disabled ? "Yes" : "No");
        
        igSeparator();
        
        // Internal state
        igText("Sample Buffer Size: %d", SAMPLE_BUFFER_SIZE);
        
        igUnindent(16.0f);
    }
}

void mos6581_render_debug_window(void* chip, bool* show_window) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid || !sid->desc) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", sid->desc->description);
    
    // Create generic chip GUI config
    chip_gui_config_t config = generic_chip_gui_get_default_config("MOS6581", "SID");
    config.get_layout = get_sid_layout;
    config.get_pin_states = get_sid_pin_states;
    
    // Create generic chip GUI instance
    generic_chip_gui_t* gui = generic_chip_gui_create(sid, &config);
    if (!gui) {
        return;
    }
    
    // Use generic chip GUI render function
    generic_chip_gui_render_debug_panel(gui, NULL, window_title, show_window, render_sid_specific_content);
    
    // Cleanup
    generic_chip_gui_destroy(gui);
}

// ============================================================================
// MOS6581 SID GUI SETTINGS WINDOW
// ============================================================================
void mos6581_render_settings_window(void* chip, bool* show_window) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid || !sid->desc) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", sid->desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("SID Configuration");
    igSeparator();
    
    igText("Chip Type: MOS6581 SID");
    igText("Base Address: $D400-$D7FF");
    igText("Register Size: 32 bytes (repeated each 32 bytes)");
    igText("Address Mask: $1F (31)");
    
    igSeparator();
    
    // Pin Configuration
    if (igCollapsingHeader_BoolPtr("Pin Configuration", NULL, 0)) {
        igIndent(16.0f);
        igText("SID MOS 6581 DIP has 28 pins:");
        igText("CAP1A/CAP1B (1,2) - Capacitor connections");
        igText("CAP2A/CAP2B (3,4) - Capacitor connections");
        igText("/RES (5) - Reset Input");
        igText("phi2 (6) - Clock Input");
        igText("R/W (7) - Read/Write Input");
        igText("/CS (8) - Chip Select");
        igText("A0-A4 (9-13) - Address Inputs (5 bits)");
        igText("GND (14) - Ground");
        igText("D0-D7 (15-22) - Data Bus (8 bits)");
        igText("POT_Y/POT_X (23,24) - Paddle inputs");
        igText("Vcc/EXT_IN (25,26) - Power/External input");
        igText("AUDIO_OUT/Vdd (27,28) - Audio output/Power");
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Sound Settings
    if (igCollapsingHeader_BoolPtr("Sound Settings", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        static bool sound_enabled = true;
        igCheckbox("Sound Enabled", &sound_enabled);
        
        static float master_volume = 1.0f;
        igSliderFloat("Master Volume", &master_volume, 0.0f, 1.0f, "%.2f", 0);
        
        static float cpu_clock = 1000000.0f;
        igInputFloat("CPU Clock (Hz)", &cpu_clock, 1000.0f, 10000.0f, "%.0f", 0);
        
        igSeparator();
        
        igText("Filter Settings");
        static bool filter_enabled = true;
        igCheckbox("Filter Enabled", &filter_enabled);
        
        static bool accurate_sid = false;
        igCheckbox("Accurate SID Emulation", &accurate_sid);
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    // Voice Configuration
    if (igCollapsingHeader_BoolPtr("Voice Configuration", NULL, 0)) {
        igIndent(16.0f);
          for (int i = 1; i <= 3; i++) {
            igPushID_Int(i);
            
            char voice_header[32];
            snprintf(voice_header, sizeof(voice_header), "Voice %d Settings", i); // Made unique
            
            if (igCollapsingHeader_BoolPtr(voice_header, NULL, 0)) {
                igIndent(16.0f);
                
                static bool voice_enabled = true;
                igCheckbox("Enabled", &voice_enabled);
                
                static float voice_volume = 1.0f;
                igSliderFloat("Volume", &voice_volume, 0.0f, 1.0f, "%.2f", 0);
                  igUnindent(16.0f);
            }
            
            igPopID();
        }
        
        igUnindent(16.0f);
    }
    
    igSeparator();
    
    if (igButton("Reset SID", (ImVec2){0, 0})) {
        mos6581_reset(sid);
    }
    
    igSameLine(0, -1);
    
    if (igButton("Test Sound", (ImVec2){0, 0})) {
        // TODO: Generate test sound
    }

    // Reset to single column at the end
    igColumns(1, NULL, false);

    igEnd();
}