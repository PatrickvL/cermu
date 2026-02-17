#include "mos6581.h"
#include "../../gui/imgui_interface.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef IMGUI_VERSION
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif
#include <stdio.h>
#include <memory>

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

#ifdef IMGUI_VERSION
static void render_voice_debug(voice_t* voice, int voice_num) {
    ImGui::PushID(voice_num);
    
    char voice_header[32];
    snprintf(voice_header, sizeof(voice_header), "Voice %d", voice_num);
    
    if (ImGui::CollapsingHeader(voice_header)) {
        ImGui::Indent(16.0f);
        
        // Write-only voice register values :
        ImGui::Text("Frequency: $%04X (%d)", voice->frequency, voice->frequency);
        ImGui::Text("Pulse Waveform Width: $%04X (%d)", voice->pulse_waveform_width, voice->pulse_waveform_width);
        
        // Values updated by WriteVoiceControlRegisterValue()
        ImGui::Text("Gated: %s", voice->gated ? "Yes" : "No");
        ImGui::Text("Synchronize: %s", voice->synchronize ? "Yes" : "No");
        ImGui::Text("Ring Modulation: %s", voice->ring_modulation ? "Yes" : "No");
        ImGui::Text("Test: %s", voice->test ? "Yes" : "No");
        const char* waveform_name = ((int)voice->waveform < 9) ? waveform_names[(int)voice->waveform] : "Unknown";
        ImGui::Text("Waveform: %s (%d)", waveform_name, (int)voice->waveform);
        
        ImGui::Separator();
        
        // Outside readable variables (albeit after shifting)
        ImGui::Text("Envelope Amplitude: $%02X (%d)", voice->envelope_amplitude, voice->envelope_amplitude);
        ImGui::Text("Oscillator Waveform: $%04X (%d)", voice->oscillator_waveform, voice->oscillator_waveform);
        ImGui::Text("Result: %d", voice->result);
        
        ImGui::Separator();
        
        // Internal state
        ImGui::Text("Waveform Accumulator: $%06X (%d)", voice->waveform_accumulator, voice->waveform_accumulator);
        const char* cycle_name = ((int)voice->envelope_cycle < 5) ? envelope_cycle_names[(int)voice->envelope_cycle] : "Unknown";
        ImGui::Text("Envelope Cycle: %s (%d)", cycle_name, (int)voice->envelope_cycle);
        ImGui::Text("Exp Counter: %d / %d", voice->exponential_counter, voice->exponential_counter_period);
        ImGui::Text("Sustain Level: 0x%02X (%d)", voice->sustain_level, voice->sustain_level);
        
        ImGui::Text("CPU Clock: %.0f Hz", voice->cpu_clock);

        ImGui::Unindent(16.0f);
    }
    
    ImGui::PopID();
}
#endif

// ============================================================================
// MOS6581 SID LAYOUT (28-pin DIP)
// ============================================================================

inline ChipLayout create_mos6581_layout() {
    // Start with DIP-28 base layout
    ChipLayout layout = create_dip28_layout();
    
    // Clear default pins from create_dip28_layout() and add hardware-accurate MOS6581 pins
    layout.left_pins.clear();
    layout.right_pins.clear();
    
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
    
    // Hardware-accurate MOS6581 SID pinout (28-pin DIP)
    // Right-hand pins (15-28) are numbered bottom-up, not top-down
    PIN_LR(layout,  1, CAP1A,  VDD, 28);      // Filter Cap 1A / +12V Power
    PIN_LR(layout,  2, CAP1B,  AUDIO_OUT, 27); // Filter Cap 1B / Audio Output
    PIN_LR(layout,  3, CAP2A,  EXT_IN, 26);   // Filter Cap 2A / External Input
    PIN_LR(layout,  4, CAP2B,  VCC, 25);      // Filter Cap 2B / +5V Power
    PIN_LR(layout,  5, RES,    POTX, 24);     // Reset         / Paddle X
    PIN_LR(layout,  6, PHI2,   POTY, 23);     // Clock         / Paddle Y
    PIN_LR(layout,  7, RW,     D7, 22);       // Read/Write    / Data 7
    PIN_LR(layout,  8, CS,     D6, 21);       // Chip Select   / Data 6
    PIN_LR(layout,  9, A0,     D5, 20);       // Address 0     / Data 5
    PIN_LR(layout, 10, A1,     D4, 19);       // Address 1     / Data 4
    PIN_LR(layout, 11, A2,     D3, 18);       // Address 2     / Data 3
    PIN_LR(layout, 12, A3,     D2, 17);       // Address 3     / Data 2
    PIN_LR(layout, 13, A4,     D1, 16);       // Address 4     / Data 1
    PIN_LR(layout, 14, VSS,    D0, 15);       // Ground        / Data 0
    
    return layout;
}

// ============================================================================
// MOS6581 SID GUI DEBUG WINDOW
// ============================================================================

// Helper function to get SID pin states for visualization
static std::vector<PinSignalState> get_sid_pin_states(mos6581_t* sid, const ChipLayout* layout, bus_state_t bus_state) {
    std::vector<PinSignalState> pin_states;
    if (!sid || !layout) return pin_states;
    
    int total_pins = layout->get_total_pins();
    pin_states.resize(total_pins);
    
    // Initialize all pins as inactive by default
    for (int i = 0; i < total_pins; i++) {
        pin_states[i] = PinSignalState{
            .pin_number = static_cast<uint8_t>(i + 1),
            .signal_level = false,
            .drive_direction = false,
            .signal_value = 0,
            .high_impedance = true,
            .has_pullup = false,
            .has_pulldown = false,
            .signal_valid = true,
            .analog_voltage = 0.0f,
            .is_pwm = false,
            .pwm_duty_cycle = 0.0f
        };
    }
    
    // Set power pins as active
    pin_states[13].signal_level = false; // VSS (Ground, pin 14)
    pin_states[13].high_impedance = false;
    pin_states[24].signal_level = true;  // VCC (+5V, pin 25)
    pin_states[24].high_impedance = false;
    pin_states[27].signal_level = true;  // VDD (+12V, pin 28)
    pin_states[27].high_impedance = false;
    
    // Audio output pin should be active if SID is producing sound
    pin_states[26].signal_level = true;  // AUDIO_OUT (pin 27)
    pin_states[26].drive_direction = true;
    pin_states[26].high_impedance = false;
    
    return pin_states;
}

// Use global renderer for SID chip visualization
static ChipLayout& get_sid_layout() {
    static ChipLayout layout = create_mos6581_layout();
    return layout;
}

void mos6581_render_debug_window(void* chip, bool* show_window) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid || !sid->desc || !show_window || !*show_window) return;
    
#ifdef IMGUI_VERSION
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", sid->desc->description);
    
    if (!ImGui::Begin(window_title, show_window)) {
        ImGui::End();
        return;
    }

    // Create two-column layout: chip visualization on left, debugging info on right
    ImVec2 window_size = ImGui::GetWindowSize();
    
    // Left column: Chip Visualization (fixed width ~250px)
    ImVec2 chip_viz_size = ImVec2(250.0f, 0);
    if (ImGui::BeginChild("ChipVisualization", chip_viz_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Chip Visualization");
        ImGui::Separator();
        
        // Calculate chip center for visualization
        ImVec2 chip_center = ImGui::GetCursorScreenPos();
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        chip_center.x += content_region.x * 0.5f;
        chip_center.y += 150.0f; // Space for the chip (smaller for 28-pin)
        
        // Get global renderer and chip layout
        ChipVisualization& renderer = GetGlobalChipRenderer();
        ChipLayout& layout = get_sid_layout();
        
        // Get current pin states from SID
        std::vector<PinSignalState> pin_states = get_sid_pin_states(sid, &layout, 0 /* bus_state */);
        
        // Render the chip using global renderer
        renderer.render(layout, chip_center, pin_states, "MOS6581 SID");
    }
    ImGui::EndChild();
    
    ImGui::SameLine(0, 5.0f); // Small gap between columns
    
    // Right column: All debugging information
    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0); // Remaining width minus left column and gap
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        // SID Information
        ImGui::Text("MOS 6581 SID (Sound Interface Device)");
        ImGui::Text("SID MOS 6581 DIP has 28 pins");
        ImGui::Separator();
        
        // Voices section
        if (ImGui::CollapsingHeader("Voices", ImGuiTreeNodeFlags_DefaultOpen)) {
            render_voice_debug(&sid->voice1, 1);
            render_voice_debug(&sid->voice2, 2);
            render_voice_debug(&sid->voice3, 3);
        }
        
        ImGui::Separator();
        
        // Filter and Global Settings
        if (ImGui::CollapsingHeader("Filter & Global Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(16.0f);
            
            // Write-only register values :
            ImGui::Text("Filter Cutoff Frequency: $%03X (%d)", sid->filter_cutoff_frequency, sid->filter_cutoff_frequency);
            ImGui::Text("Filter Voice 1: %s", sid->filter_voice1 ? "Yes" : "No");
            ImGui::Text("Filter Voice 2: %s", sid->filter_voice2 ? "Yes" : "No");
            ImGui::Text("Filter Voice 3: %s", sid->filter_voice3 ? "Yes" : "No");
            ImGui::Text("Filter Voice 4 (External): %s", sid->filter_voice4 ? "Yes" : "No");
            ImGui::Text("Filter Resonance: %d (0-15)", sid->filter_resonance);
            ImGui::Text("Volume: %d (0-15)", sid->volume);
            ImGui::Text("Low Pass Enabled: %s", sid->low_pass_enabled ? "Yes" : "No");
            ImGui::Text("Band Pass Enabled: %s", sid->band_pass_enabled ? "Yes" : "No");
            ImGui::Text("High Pass Enabled: %s", sid->high_pass_enabled ? "Yes" : "No");
            ImGui::Text("Voice 3 Disabled: %s", sid->voice3_disabled ? "Yes" : "No");
            
            ImGui::Separator();
            
            // Internal state
            ImGui::Text("Sample Buffer Size: %d", SAMPLE_BUFFER_SIZE);
            
            ImGui::Unindent(16.0f);
        }
    }
    ImGui::EndChild();

    ImGui::End();
#endif
}

// ============================================================================
// MOS6581 SID GUI SETTINGS WINDOW
// ============================================================================
void mos6581_render_settings_window(void* chip, bool* show_window) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid || !sid->desc || !show_window || !*show_window) return;
    
#ifdef IMGUI_VERSION
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", sid->desc->description);
    
    if (!ImGui::Begin(window_title, show_window)) {
        ImGui::End();
        return;
    }

    ImGui::Text("SID Configuration");
    ImGui::Separator();
    
    ImGui::Text("Chip Type: MOS6581 SID");
    ImGui::Text("Base Address: $D400-$D7FF");
    ImGui::Text("Register Size: 32 bytes (repeated each 32 bytes)");
    ImGui::Text("Address Mask: $1F (31)");
    
    ImGui::Separator();
    
    // Pin Configuration
    if (ImGui::CollapsingHeader("Pin Configuration")) {
        ImGui::Indent(16.0f);
        ImGui::Text("SID MOS 6581 DIP has 28 pins:");
        ImGui::Text("CAP1A/CAP1B (1,2) - Capacitor connections");
        ImGui::Text("CAP2A/CAP2B (3,4) - Capacitor connections");
        ImGui::Text("/RES (5) - Reset Input");
        ImGui::Text("phi2 (6) - Clock Input");
        ImGui::Text("R/W (7) - Read/Write Input");
        ImGui::Text("/CS (8) - Chip Select");
        ImGui::Text("A0-A4 (9-13) - Address Inputs (5 bits)");
        ImGui::Text("GND (14) - Ground");
        ImGui::Text("D0-D7 (15-22) - Data Bus (8 bits)");
        ImGui::Text("POT_Y/POT_X (23,24) - Paddle inputs");
        ImGui::Text("Vcc/EXT_IN (25,26) - Power/External input");
        ImGui::Text("AUDIO_OUT/Vdd (27,28) - Audio output/Power");
        ImGui::Unindent(16.0f);
    }
    
    ImGui::Separator();
    
    // Sound Settings
    if (ImGui::CollapsingHeader("Sound Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(16.0f);
        
        static bool sound_enabled = true;
        ImGui::Checkbox("Sound Enabled", &sound_enabled);
        
        static float master_volume = 1.0f;
        ImGui::SliderFloat("Master Volume", &master_volume, 0.0f, 1.0f, "%.2f");
        
        static float cpu_clock = 1000000.0f;
        ImGui::InputFloat("CPU Clock (Hz)", &cpu_clock, 1000.0f, 10000.0f, "%.0f");
        
        ImGui::Separator();
        
        ImGui::Text("Filter Settings");
        static bool filter_enabled = true;
        ImGui::Checkbox("Filter Enabled", &filter_enabled);
        
        static bool accurate_sid = false;
        ImGui::Checkbox("Accurate SID Emulation", &accurate_sid);
        
        ImGui::Unindent(16.0f);
    }
    
    ImGui::Separator();
    
    // Voice Configuration
    if (ImGui::CollapsingHeader("Voice Configuration")) {
        ImGui::Indent(16.0f);
          for (int i = 1; i <= 3; i++) {
            ImGui::PushID(i);
            
            char voice_header[32];
            snprintf(voice_header, sizeof(voice_header), "Voice %d Settings", i); // Made unique
            
            if (ImGui::CollapsingHeader(voice_header)) {
                ImGui::Indent(16.0f);
                
                static bool voice_enabled = true;
                ImGui::Checkbox("Enabled", &voice_enabled);
                
                static float voice_volume = 1.0f;
                ImGui::SliderFloat("Volume", &voice_volume, 0.0f, 1.0f, "%.2f");
                ImGui::Unindent(16.0f);
            }
            
            ImGui::PopID();
        }
        
        ImGui::Unindent(16.0f);
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Reset SID")) {
        mos6581_reset(sid);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Test Sound")) {
        // TODO: Generate test sound
    }

    // Reset to single column at the end
    ImGui::Columns(1, nullptr, false);

    ImGui::End();
#endif
}