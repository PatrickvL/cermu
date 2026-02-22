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
#ifdef IMGUI_VERSION
#include <imgui.h>
#include "../../../gui/chip_visualization.h"
#include "../../../gui/global_chip_style.h"
#endif
#include <stdio.h>

// ============================================================================
// MOS 6560/6561 VIC LAYOUT (40-pin DIP)
// ============================================================================

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
    PIN_LR(layout,  1, D7,      VDD, 40)       // data hi / +5V
    PIN_LR(layout,  2, D6,      CSYNC, 39)     // / comp sync
    PIN_LR(layout,  3, D5,      LUMA, 38)      // / luminance
    PIN_LR(layout,  4, D4,      CHROMA, 37)    // / chrominance
    PIN_LR(layout,  5, D3,      RW, 36)        // / R/W
    PIN_LR(layout,  6, D2,      A13, 35)       // / addr hi
    PIN_LR(layout,  7, D1,      A12, 34)
    PIN_LR(layout,  8, D0,      A11, 33)       // data lo
    PIN_LR(layout,  9, MA7,     A10, 32)       // mux bus hi / addr
    PIN_LR(layout, 10, MA6,     A9, 31)
    PIN_LR(layout, 11, MA5,     A8, 30)        // / addr lo
    PIN_LR(layout, 12, MA4,     A7, 29)
    PIN_LR(layout, 13, MA3,     A6, 28)
    PIN_LR(layout, 14, MA2,     A5, 27)
    PIN_LR(layout, 15, MA1,     SOUND, 26)     // mux bus lo / audio
    PIN_LR(layout, 16, MA0,     POTX, 25)      // / paddle X
    PIN_LR(layout, 17, PHI0,    POTY, 24)      // clock in / paddle Y
    PIN_LR(layout, 18, PHI1,    LIGHT_PEN, 23) // clock phi1 / light pen
    PIN_LR(layout, 19, PHI2,    _CS, 22)       // clock phi2 / chip sel
    PIN_LR(layout, 20, VSS,     _IRQ, 21)      // gnd / interrupt

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
// ChipBase interface implementation
// ============================================================================

ChipIdentity vic_base_s::chip_identity() const {
    return {is_pal ? "MOS6561" : "MOS6560", "MOS Technology",
            is_pal ? VideoStandard::PAL : VideoStandard::NTSC};
}

bool vic_base_s::has_debug_content()    const { return true; }
bool vic_base_s::has_settings_content() const { return true; }
bool vic_base_s::has_layout_content()   const { return true; }

// ============================================================================
// VIC GUI DEBUG WINDOW
// ============================================================================

void vic_base_s::render_debug_content() {
    vic_base_t* vic = this;

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

void vic_base_s::render_settings_content() {
    vic_base_t* vic = this;

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

void vic_base_s::render_layout_content() {
    vic_base_t* vic = this;

#ifdef IMGUI_VERSION
    const char* chip_name = get_vic_type_name(vic);

    ChipLayout& layout = get_vic_layout(vic->is_pal);
    std::vector<PinSignalState> pin_states = get_vic_pin_states(vic, &layout);
    render_chip_layout(layout, pin_states, chip_name);
#endif
}