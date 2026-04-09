#include "chip/sound/mos6581.hpp"
#include "core/chip_layout.hpp"
#include "core/pin_macros.hpp"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif
#include <cstdio>
#include <memory>

// ============================================================================
// MOS6581 SID LAYOUT (28-pin DIP)
// ============================================================================

#ifdef CERMU_HAS_GUI

// ============================================================================
// MOS6581 SID layout virtuals
// ============================================================================

ChipLayout* mos6581_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        // MOS6581 SID — 28-pin DIP
        ChipLayout layout = create_dip28_layout();

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
    }();
    return &layout;
}

std::vector<PinSignalState> mos6581_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, bus_snapshot_, [](auto& ps) {
        // Audio output pin (always driven)
        ps[26].signal_level = true;  // AUDIO_OUT (pin 27)
        ps[26].drive_direction = true;
        ps[26].high_impedance = false;
    });
}

// ============================================================================
// MOS6581 SID GUI SETTINGS WINDOW
// ============================================================================

void mos6581_t::render_settings_content() {
    mos6581_t* sid = this;

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
        // Poke SID registers to produce a brief A-440 sawtooth on voice 1.
        // Uses the normal register-write path so all internal state updates.
        auto poke = [sid](uint8_t reg, uint8_t val) {
            bus_state_t bus = 0;
            BUS_SET_ADDR(bus, reg);
            BUS_SET_DATA(bus, val);
            registers_write(sid, bus);
        };

        // Compute frequency register for A-440
        // freq_reg = 440 * 16777216 / cpu_clock
        float clk = sid->cpu_clock > 0.0f ? sid->cpu_clock : SID_DEFAULT_CPU_CLOCK_PAL;
        uint16_t freq = (uint16_t)(440.0f * 16777216.0f / clk);

        poke(VOICE_FRELO, freq & 0xFF);          // Voice 1 freq lo
        poke(VOICE_FREHI, (freq >> 8) & 0xFF);   // Voice 1 freq hi
        poke(VOICE_ATDCY, 0x09);                 // Attack=0, Decay=9
        poke(VOICE_SUREL, 0xA0);                 // Sustain=10, Release=0
        poke(SID_REG_SIGVOL, 0x0F);              // Max volume, no filter
        poke(VOICE_VCREG, WAVEFORM_SAWTOOTH | VCREG_GATE); // Sawtooth + gate on
    }

    // Reset to single column at the end
    ImGui::Columns(1, nullptr, false);
}

#endif // CERMU_HAS_GUI
