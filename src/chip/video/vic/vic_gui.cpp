/*
 * vic_gui.cpp — MOS 6560/6561 VIC Debug/Settings GUI Windows
 *
 * Hardware-accurate 40-pin DIP pinout based on the MOS 6560/6561 datasheets.
 * The VIC (Video Interface Chip) generates video display, produces 4-channel
 * audio (3 square wave + 1 noise), and handles light pen input.
 *
 * MOS 6560 = NTSC variant (VIC-20 NTSC)
 * MOS 6561 = PAL variant  (VIC-20 PAL)
 *
 * Pinout reference: MOS Technology MOS 6560/6561 Datasheet (1980)
 */

#include "vic_common.h"
#include "../../../core/chip_layout.h"
#include "../../../core/pin_macros.h"
#ifdef IMGUI_VERSION
#include <imgui.h>
#include "../../../gui/chip_visualization.h"
#include "../../../gui/global_chip_style.h"
#endif
#include <stdio.h>
#include <memory>

// ============================================================================
// MOS 6560/6561 VIC LAYOUT (40-pin DIP)
// ============================================================================
//
// Hardware-accurate MOS 6560/6561 VIC pinout (MOS Technology datasheet):
//
//         ╔═══════════╗
//   D7  ──┤ 1      40 ├── VDD (+5V)
//   D6  ──┤ 2      39 ├── COMP SYNC
//   D5  ──┤ 3      38 ├── LUMA OUT
//   D4  ──┤ 4      37 ├── /CHROMA
//   D3  ──┤ 5      36 ├── R/W
//   D2  ──┤ 6      35 ├── A13
//   D1  ──┤ 7      34 ├── A12
//   D0  ──┤ 8      33 ├── A11
//   DB7 ──┤ 9      32 ├── A10
//   DB6 ──┤10      31 ├── A9
//   DB5 ──┤11      30 ├── A8
//   DB4 ──┤12      29 ├── A7
//   DB3 ──┤13      28 ├── A6
//   DB2 ──┤14      27 ├── A5
//   DB1 ──┤15      26 ├── SOUND
//   DB0 ──┤16      25 ├── /POTX
//  Φ0   ──┤17      24 ├── /POTY
//   Φ1  ──┤18      23 ├── LIGHT PEN
//   Φ2  ──┤19      22 ├── /CS
//   VSS ──┤20      21 ├── /IRQ (not connected)
//         ╚═══════════╝
//
// Note: Pins 9-16 (DB0-DB7) are the VIC's multiplexed address/data
// bus for accessing video memory. The VIC generates addresses on these
// pins (muxed with A0-A5 on the address bus) during PHI1.

inline ChipLayout create_vic_layout(const char* part_number) {
    ChipLayout layout = create_dip40_layout();

    layout.left_pins.clear();
    layout.right_pins.clear();

    layout.markings = {
        part_number,
        "MOS Technology",
        nullptr, nullptr, nullptr, nullptr,
        true, true, false, false
    };

    // Hardware-accurate MOS 6560/6561 VIC pinout (40-pin DIP)
    PIN_LR(layout,  1, D7,      VDD, 40)       // Data 7 / +5V Power
    PIN_LR(layout,  2, D6,      CSYNC, 39)     // Data 6 / Composite Sync
    PIN_LR(layout,  3, D5,      LUMA, 38)      // Data 5 / Luminance Output
    PIN_LR(layout,  4, D4,      CHROMA, 37)    // Data 4 / Chrominance Output
    PIN_LR(layout,  5, D3,      RW, 36)        // Data 3 / Read/Write
    PIN_LR(layout,  6, D2,      A13, 35)       // Data 2 / Address 13
    PIN_LR(layout,  7, D1,      A12, 34)       // Data 1 / Address 12
    PIN_LR(layout,  8, D0,      A11, 33)       // Data 0 / Address 11
    PIN_LR(layout,  9, MA7,     A10, 32)       // Mux Bus 7 / Address 10
    PIN_LR(layout, 10, MA6,     A9, 31)        // Mux Bus 6 / Address 9
    PIN_LR(layout, 11, MA5,     A8, 30)        // Mux Bus 5 / Address 8
    PIN_LR(layout, 12, MA4,     A7, 29)        // Mux Bus 4 / Address 7
    PIN_LR(layout, 13, MA3,     A6, 28)        // Mux Bus 3 / Address 6
    PIN_LR(layout, 14, MA2,     A5, 27)        // Mux Bus 2 / Address 5
    PIN_LR(layout, 15, MA1,     SOUND, 26)     // Mux Bus 1 / Sound Output
    PIN_LR(layout, 16, MA0,     POTX, 25)      // Mux Bus 0 / Paddle X
    PIN_LR(layout, 17, PHI0,    POTY, 24)      // Clock Input / Paddle Y
    PIN_LR(layout, 18, PHI1,    LIGHT_PEN, 23) // Clock Phase 1 / Light Pen
    PIN_LR(layout, 19, PHI2,    CS, 22)        // Clock Phase 2 / Chip Select
    PIN_LR(layout, 20, VSS,     IRQ, 21)       // Ground / /IRQ (active low)

    return layout;
}

// ============================================================================
// VIC PIN STATES
// ============================================================================

static std::vector<PinSignalState> get_vic_pin_states(vic_base_t* vic, const ChipLayout* layout) {
    std::vector<PinSignalState> pin_states;
    if (!vic || !layout) return pin_states;

    int total_pins = layout->get_total_pins();
    pin_states.resize(total_pins);

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

    // Power pins
    pin_states[19].signal_level = false; // VSS (Ground, pin 20)
    pin_states[19].high_impedance = false;
    pin_states[39].signal_level = true;  // VDD (+5V, pin 40)
    pin_states[39].high_impedance = false;

    // Video output pins (active when generating display)
    pin_states[37].signal_level = true; // LUMA (pin 38)
    pin_states[37].drive_direction = true;
    pin_states[37].high_impedance = false;
    pin_states[36].signal_level = true; // CHROMA (pin 37)
    pin_states[36].drive_direction = true;
    pin_states[36].high_impedance = false;
    pin_states[38].signal_level = true; // CSYNC (pin 39)
    pin_states[38].drive_direction = true;
    pin_states[38].high_impedance = false;

    // Sound output (pin 26) — active if any voice is enabled
    bool any_voice_on = false;
    for (int v = VIC_REG_BASS_FREQ; v <= VIC_REG_NOISE_FREQ; v++) {
        if (vic->registers[v] & VIC_VOICE_ENABLE) {
            any_voice_on = true;
            break;
        }
    }
    pin_states[25].signal_level = any_voice_on; // SOUND (pin 26)
    pin_states[25].drive_direction = true;
    pin_states[25].high_impedance = false;

    return pin_states;
}

static ChipLayout& get_vic_layout(bool is_pal) {
    static ChipLayout layout_ntsc = create_vic_layout("MOS6560");
    static ChipLayout layout_pal  = create_vic_layout("MOS6561");
    return is_pal ? layout_pal : layout_ntsc;
}

static const char* get_vic_type_name(vic_base_t* vic) {
    if (vic->is_pal) return "MOS 6561 (PAL)";
    return "MOS 6560 (NTSC)";
}

// ============================================================================
// VIC GUI DEBUG WINDOW
// ============================================================================

void vic_gui_render_debug_content(void* chip) {
    vic_base_t* vic = (vic_base_t*)chip;
    if (!vic) return;

#ifdef IMGUI_VERSION
    // Two-column layout
    ImVec2 window_size = ImGui::GetContentRegionAvail();

    ImVec2 chip_viz_size = ImVec2(250.0f, 0);
    if (ImGui::BeginChild("ChipVisualization", chip_viz_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Chip Visualization");
        ImGui::Separator();

        ImVec2 chip_center = ImGui::GetCursorScreenPos();
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        chip_center.x += content_region.x * 0.5f;
        chip_center.y += 200.0f;

        ChipVisualization& renderer = GetGlobalChipRenderer();
        ChipLayout& layout = get_vic_layout(vic->is_pal);
        std::vector<PinSignalState> pin_states = get_vic_pin_states(vic, &layout);
        renderer.render(layout, chip_center, pin_states, get_vic_type_name(vic));
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 5.0f);

    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0);
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Video Interface Chip - %s", get_vic_type_name(vic));
        ImGui::Separator();

        // Raster Information
        if (ImGui::CollapsingHeader("Raster Information", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Raster Line:   %d / %d", vic->raster_counter, vic->total_lines);
            ImGui::Text("Cycle:         %d / %d", vic->current_cycle % vic->cycles_per_line, vic->cycles_per_line);
            ImGui::Text("Video Standard: %s", vic->is_pal ? "PAL (6561)" : "NTSC (6560)");
            float raster_progress = (float)vic->raster_counter / (float)vic->total_lines;
            ImGui::ProgressBar(raster_progress, ImVec2(-1, 0), NULL);
        }

        // Screen Configuration
        if (ImGui::CollapsingHeader("Screen Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {
            uint8_t ctrl1 = vic->registers[VIC_REG_CONTROL1];
            uint8_t ctrl2 = vic->registers[VIC_REG_CONTROL2];
            uint8_t vm_reg = vic->registers[VIC_REG_VIDEO_MATRIX];
            uint8_t rows = vic->registers[VIC_REG_ROWS];
            uint8_t cb = vic->registers[VIC_REG_CHAR_BASE];

            ImGui::Text("Control 1 ($9000): $%02X", ctrl1);
            ImGui::Indent(20.0f);
            ImGui::Text("Interlace: %s", (ctrl1 & VIC_C1_INTERLACE) ? "YES" : "NO");
            ImGui::Text("Screen Origin X: %d", ctrl1 & VIC_C1_SCREEN_ORIGIN_X_MASK);
            ImGui::Unindent(20.0f);

            ImGui::Text("Control 2 ($9001): $%02X", ctrl2);
            ImGui::Indent(20.0f);
            ImGui::Text("Screen Origin Y: %d", ctrl2);
            ImGui::Unindent(20.0f);

            ImGui::Text("Columns:      %d", vm_reg & VIC_VM_COLUMNS_MASK);
            ImGui::Text("Rows:         %d", (rows & VIC_ROWS_ROWS_MASK) >> VIC_ROWS_ROWS_SHIFT);
            ImGui::Text("Double Height: %s", (rows & VIC_ROWS_DOUBLE_HEIGHT) ? "YES" : "NO");

            // Memory pointers
            uint16_t video_base = ((cb & VIC_CB_BASE_VIDEO_MASK) >> 4) << VIC_CB_BASE_VIDEO_SHIFT;
            uint16_t char_base = (cb & VIC_CB_BASE_CHAR_MASK) << VIC_CB_BASE_CHAR_SHIFT;
            ImGui::Text("Video Matrix Base: $%04X", video_base);
            ImGui::Text("Character Base:    $%04X", char_base);
        }

        // Audio Registers
        if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
            uint8_t bass    = vic->registers[VIC_REG_BASS_FREQ];
            uint8_t alto    = vic->registers[VIC_REG_ALTO_FREQ];
            uint8_t soprano = vic->registers[VIC_REG_SOPRANO_FREQ];
            uint8_t noise   = vic->registers[VIC_REG_NOISE_FREQ];
            uint8_t aux     = vic->registers[VIC_REG_AUX_COLOR];

            ImGui::Text("Volume:  %d", aux & VIC_AUX_VOLUME_MASK);
            ImGui::Separator();
            ImGui::Text("Bass    ($900A): $%02X  %s  Freq: %d",
                        bass, (bass & VIC_VOICE_ENABLE) ? "ON " : "OFF", bass & VIC_VOICE_FREQ_MASK);
            ImGui::Text("Alto    ($900B): $%02X  %s  Freq: %d",
                        alto, (alto & VIC_VOICE_ENABLE) ? "ON " : "OFF", alto & VIC_VOICE_FREQ_MASK);
            ImGui::Text("Soprano ($900C): $%02X  %s  Freq: %d",
                        soprano, (soprano & VIC_VOICE_ENABLE) ? "ON " : "OFF", soprano & VIC_VOICE_FREQ_MASK);
            ImGui::Text("Noise   ($900D): $%02X  %s  Freq: %d",
                        noise, (noise & VIC_VOICE_ENABLE) ? "ON " : "OFF", noise & VIC_VOICE_FREQ_MASK);
        }

        // Colors
        if (ImGui::CollapsingHeader("Colors")) {
            uint8_t bg_reg = vic->registers[VIC_REG_BACKGROUND];
            uint8_t aux_reg = vic->registers[VIC_REG_AUX_COLOR];

            ImGui::Text("Background Color ($900F): $%02X", bg_reg);
            ImGui::Indent(20.0f);
            ImGui::Text("Border:     %d", bg_reg & VIC_BG_BORDER_MASK);
            ImGui::Text("Background: %d", (bg_reg & VIC_BG_BACKGROUND_MASK) >> VIC_BG_BACKGROUND_SHIFT);
            ImGui::Text("Reverse:    %s", (bg_reg & VIC_BG_REVERSE) ? "YES" : "NO");
            ImGui::Unindent(20.0f);

            ImGui::Text("Auxiliary Color ($900E): $%02X", aux_reg);
            ImGui::Indent(20.0f);
            ImGui::Text("Aux Color:  %d", (aux_reg & VIC_AUX_COLOR_MASK) >> VIC_AUX_COLOR_SHIFT);
            ImGui::Unindent(20.0f);
        }
    }
    ImGui::EndChild();
#endif
}

// ============================================================================
// VIC GUI SETTINGS
// ============================================================================

void vic_gui_render_settings_content(void* chip) {
    vic_base_t* vic = (vic_base_t*)chip;
    if (!vic) return;

#ifdef IMGUI_VERSION

    ImGui::Text("Video Interface Chip - %s Configuration", get_vic_type_name(vic));
    ImGui::Separator();
    ImGui::Text("Chip Type: %s", get_vic_type_name(vic));
    ImGui::Text("Clock: %u Hz", vic->clock_frequency);
    ImGui::Text("Lines: %u lines/frame", vic->total_lines);
    ImGui::Text("Cycles/line: %u", vic->cycles_per_line);

    ImGui::Separator();

    // Raw register dump
    if (ImGui::CollapsingHeader("Raw Registers ($9000-$900F)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static const char* reg_names[] = {
            "Control 1", "Control 2", "Video Matrix", "Rows",
            "Raster", "Char Base", "Light Pen X", "Light Pen Y",
            "Paddle X", "Paddle Y", "Bass Freq", "Alto Freq",
            "Soprano Freq", "Noise Freq", "Aux Color", "Background"
        };
        for (int i = 0; i < 16; i++) {
            ImGui::Text("$%04X ($%02X) %-13s: $%02X", 0x9000 + i, i,
                        reg_names[i], vic->registers[i]);
        }
    }
#endif
}

// ============================================================================
// VIC LAYOUT (standalone pinout diagram)
// ============================================================================

void vic_gui_render_layout_content(void* chip) {
    vic_base_t* vic = (vic_base_t*)chip;
    if (!vic) return;

#ifdef IMGUI_VERSION
    const char* chip_name = get_vic_type_name(vic);

    ChipLayout& layout = get_vic_layout(vic->is_pal);
    std::vector<PinSignalState> pin_states = get_vic_pin_states(vic, &layout);
    render_chip_layout(layout, pin_states, chip_name);
#endif
}