#include "mos6581.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif
#include <cstdio>
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

#ifdef CERMU_HAS_GUI
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
        ImGui::Text("Gated: %s", (voice->control_reg & VCREG_GATE) ? "Yes" : "No");
        ImGui::Text("Synchronize: %s", (voice->control_reg & VCREG_SYNC) ? "Yes" : "No");
        ImGui::Text("Ring Modulation: %s", (voice->control_reg & VCREG_RING) ? "Yes" : "No");
        ImGui::Text("Test: %s", (voice->control_reg & VCREG_TEST) ? "Yes" : "No");
        int wf_index = (voice->control_reg >> 4) & 0x0F;
        const char* waveform_name = (wf_index < 9) ? waveform_names[wf_index] : "Unknown";
        ImGui::Text("Waveform: %s (%d)", waveform_name, wf_index);
        
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
        {},                     // package_variant
        {},                     // date_code
        {},                     // lot_number
        {},                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate MOS6581 SID pinout (28-pin DIP)
    // Right-hand pins (15-28) are numbered bottom-up, not top-down
    PIN_LR(layout,  1, CAP1A,  VDD, 28);      // filter cap 1A / +12V
    PIN_LR(layout,  2, CAP1B,  AUDIO_OUT, 27);// filter cap 1B / audio
    PIN_LR(layout,  3, CAP2A,  EXT_IN, 26);   // filter cap 2A / ext in
    PIN_LR(layout,  4, CAP2B,  VCC, 25);      // filter cap 2B / +5V
    PIN_LR(layout,  5, _RES,   POTX, 24);     // reset / paddle X
    PIN_LR(layout,  6, PHI2,   POTY, 23);     // clock / paddle Y
    PIN_LR(layout,  7, RW,     D7, 22);       // R/W / data hi
    PIN_LR(layout,  8, _CS,    D6, 21);       // chip sel
    PIN_LR(layout,  9, A0,     D5, 20);       // addr lo / data
    PIN_LR(layout, 10, A1,     D4, 19);
    PIN_LR(layout, 11, A2,     D3, 18);
    PIN_LR(layout, 12, A3,     D2, 17);
    PIN_LR(layout, 13, A4,     D1, 16);       // addr hi
    PIN_LR(layout, 14, VSS,    D0, 15);       // gnd / data lo
    
    return layout;
}

// ============================================================================
// MOS6581 SID GUI DEBUG WINDOW
// ============================================================================

// Helper function to get SID pin states for visualization
static std::vector<PinSignalState> get_sid_pin_states(mos6581_t* sid, const ChipLayout* layout, bus_state_t bus_state) {
    if (!sid || !layout) return {};

    // Generic bus-derived pin states (address, data, power, clock, control)
    auto pin_states = populate_pin_states_from_bus(*layout, bus_state);

    // SID specific: Audio output pin (always driven)
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

// Class method implementation
void mos6581_t::render_debug_content() {
    mos6581_t* sid = this;
    
#ifdef CERMU_HAS_GUI
    // Create two-column layout: chip visualization on left, debugging info on right
    ImVec2 window_size = ImGui::GetContentRegionAvail();
    
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
        std::vector<PinSignalState> pin_states = get_sid_pin_states(sid, &layout, sid->bus_snapshot_);
        
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
            uint8_t reson = sid->regs[SID_REG_RESON];
            uint8_t sigvol = sid->regs[SID_REG_SIGVOL];
            ImGui::Text("Filter Voice 1: %s", (reson & RESON_FILT1) ? "Yes" : "No");
            ImGui::Text("Filter Voice 2: %s", (reson & RESON_FILT2) ? "Yes" : "No");
            ImGui::Text("Filter Voice 3: %s", (reson & RESON_FILT3) ? "Yes" : "No");
            ImGui::Text("Filter Voice 4 (External): %s", (reson & RESON_FILTEX) ? "Yes" : "No");
            ImGui::Text("Filter Resonance: %d (0-15)", (reson >> RESON_RES_SHIFT) & 0x0F);
            ImGui::Text("Volume: %d (0-15)", sigvol & SIGVOL_VOL_MASK);
            ImGui::Text("Low Pass Enabled: %s", (sigvol & SIGVOL_LP) ? "Yes" : "No");
            ImGui::Text("Band Pass Enabled: %s", (sigvol & SIGVOL_BP) ? "Yes" : "No");
            ImGui::Text("High Pass Enabled: %s", (sigvol & SIGVOL_HP) ? "Yes" : "No");
            ImGui::Text("Voice 3 Disabled: %s", (sigvol & SIGVOL_3OFF) ? "Yes" : "No");
            
            ImGui::Separator();
            
            // Internal state
            ImGui::Text("Sample Buffer Size: %d", SAMPLE_BUFFER_SIZE);
            
            ImGui::Unindent(16.0f);
        }
    }
    ImGui::EndChild();
#endif
}

// ============================================================================
// MOS6581 SID GUI SETTINGS WINDOW
// ============================================================================
// Class method implementation
void mos6581_t::render_settings_content() {
    mos6581_t* sid = this;
    
#ifdef CERMU_HAS_GUI

    ImGui::Text("SID Configuration");
    ImGui::Separator();
    
    // SID Revision selector
    {
        static const char* revision_labels[] = {
            "MOS 6581 (R4AR)",   // SID_REVISION_6581_R4AR
            "MOS 8580 (R5)",     // SID_REVISION_8580_R5
        };
        int current = (int)sid->revision;
        if (current < 0 || current > 1) current = 0;
        if (ImGui::Combo("SID Revision", &current, revision_labels, IM_ARRAYSIZE(revision_labels))) {
            sid->set_revision((sid_revision_t)current);
        }
    }
    
    ImGui::Separator();
    
    ImGui::Text("Chip Type: %s",
        sid->revision <= SID_REVISION_6581_R4AR ? "MOS 6581 SID" : "MOS 8580 SID");
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
        sid->reset();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Test Sound")) {
        // TODO: Generate test sound
    }

    // Reset to single column at the end
    ImGui::Columns(1, nullptr, false);
#endif
}

// ============================================================================
// MOS6581 SID LAYOUT (standalone pinout diagram)
// ============================================================================

// Class method implementation
void mos6581_t::render_layout_content() {
    mos6581_t* sid = this;

#ifdef CERMU_HAS_GUI
    ChipLayout& layout = get_sid_layout();
    std::vector<PinSignalState> pin_states = get_sid_pin_states(sid, &layout, sid->bus_snapshot_);
    render_chip_layout(layout, pin_states, "MOS6581");
#endif
}
