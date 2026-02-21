/*
 * ted7360_gui.cpp — TED 7360/8360 Debug/Settings GUI Windows
 *
 * Hardware-accurate 48-pin DIP pinout based on the MOS 7360 TED datasheet.
 * The TED (Text Editing Device) is a combined video, sound, and I/O controller
 * used in the Commodore 16, 116, and Plus/4 computers.  It generates video
 * display, produces 2-channel sound, scans the keyboard matrix, manages
 * memory banking (ROM/RAM), and contains three 16-bit countdown timers.
 *
 * Pinout reference: MOS Technology 7360 TED Datasheet (1984)
 */

#include "ted7360.h"
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
// TED 7360 LAYOUT (48-pin DIP)
// ============================================================================
//
// Hardware-accurate MOS 7360 TED pinout (48-pin DIP):
//
//           ╔═════════════╗
//    VSS ──┤ 1        48 ├── VDD
//    D0  ──┤ 2        47 ├── /RAS
//    D1  ──┤ 3        46 ├── /CAS
//    D2  ──┤ 4        45 ├── MUX  
//    D3  ──┤ 5        44 ├── BA
//    D4  ──┤ 6        43 ├── /IRQ
//    D5  ──┤ 7        42 ├── Φ0 (in)
//    D6  ──┤ 8        41 ├── Φ2 (out)
//    D7  ──┤ 9        40 ├── R/W
//    A0  ──┤10        39 ├── A15
//    A1  ──┤11        38 ├── A14
//    A2  ──┤12        37 ├── A13
//    A3  ──┤13        36 ├── A12
//    A4  ──┤14        35 ├── A11
//    A5  ──┤15        34 ├── A10
//    A6  ──┤16        33 ├── A9
//    A7  ──┤17        32 ├── A8
//    K0  ──┤18        31 ├── K7
//    K1  ──┤19        30 ├── K6
//    K2  ──┤20        29 ├── K5
//    K3  ──┤21        28 ├── K4
//   LUMA ──┤22        27 ├── SOUND
//  CHROMA──┤23        26 ├── /CS1
//    VSS ──┤24        25 ├── CS0
//           ╚═════════════╝
//
// Notes:
// - K0-K7 are keyboard matrix column select / row return lines
// - Two VSS pins (1, 24) for improved grounding
// - BA = Bus Available (active high when TED releases bus to CPU)
// - MUX = Address multiplexer output (drives external DRAM)

inline ChipLayout create_ted7360_layout() {
    ChipLayout layout = create_custom_dip(48, "TED7360");

    layout.left_pins.clear();
    layout.right_pins.clear();

    layout.markings = {
        "MOS7360 TED",
        "MOS Technology",
        nullptr, nullptr, nullptr, nullptr,
        true, true, false, false
    };

    // Hardware-accurate TED 7360 pinout (48-pin DIP, 24 pins per side)
    PIN_LR(layout,  1, VSS,     VDD, 48)       // Ground / +5V
    PIN_LR(layout,  2, D0,      UNKNOWN, 47)   // Data 0 / /RAS
    PIN_LR(layout,  3, D1,      UNKNOWN, 46)   // Data 1 / /CAS
    PIN_LR(layout,  4, D2,      UNKNOWN, 45)   // Data 2 / MUX
    PIN_LR(layout,  5, D3,      BA, 44)        // Data 3 / Bus Available
    PIN_LR(layout,  6, D4,      IRQ, 43)       // Data 4 / /IRQ
    PIN_LR(layout,  7, D5,      PHI0, 42)      // Data 5 / Clock In
    PIN_LR(layout,  8, D6,      PHI2, 41)      // Data 6 / Clock Out
    PIN_LR(layout,  9, D7,      RW, 40)        // Data 7 / R/W
    PIN_LR(layout, 10, A0,      A15, 39)       // Address 0 / Address 15
    PIN_LR(layout, 11, A1,      A14, 38)       // Address 1 / Address 14
    PIN_LR(layout, 12, A2,      A13, 37)       // Address 2 / Address 13
    PIN_LR(layout, 13, A3,      A12, 36)       // Address 3 / Address 12
    PIN_LR(layout, 14, A4,      A11, 35)       // Address 4 / Address 11
    PIN_LR(layout, 15, A5,      A10, 34)       // Address 5 / Address 10
    PIN_LR(layout, 16, A6,      A9, 33)        // Address 6 / Address 9
    PIN_LR(layout, 17, A7,      A8, 32)        // Address 7 / Address 8
    PIN_LR(layout, 18, UNKNOWN, UNKNOWN, 31)   // K0 / K7
    PIN_LR(layout, 19, UNKNOWN, UNKNOWN, 30)   // K1 / K6
    PIN_LR(layout, 20, UNKNOWN, UNKNOWN, 29)   // K2 / K5
    PIN_LR(layout, 21, UNKNOWN, UNKNOWN, 28)   // K3 / K4
    PIN_LR(layout, 22, LUMA,    SOUND, 27)     // Luminance / Sound Output
    PIN_LR(layout, 23, CHROMA,  CS1, 26)       // Chrominance / /CS1
    PIN_LR(layout, 24, VSS,     CS0, 25)       // Ground / CS0

    return layout;
}

// ============================================================================
// TED PIN STATES
// ============================================================================

static std::vector<PinSignalState> get_ted_pin_states(ted7360_t* ted, const ChipLayout* layout) {
    std::vector<PinSignalState> pin_states;
    if (!ted || !layout) return pin_states;

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
    pin_states[0].signal_level = false;   // VSS (pin 1)
    pin_states[0].high_impedance = false;
    pin_states[23].signal_level = false;  // VSS (pin 24)
    pin_states[23].high_impedance = false;
    pin_states[47].signal_level = true;   // VDD (pin 48)
    pin_states[47].high_impedance = false;

    // IRQ pin (pin 43, index 42) — active low
    pin_states[42].signal_level = !ted7360_irq_pending(ted);
    pin_states[42].drive_direction = true;
    pin_states[42].high_impedance = false;

    // BA pin (pin 44, index 43) — high when CPU has bus
    pin_states[43].signal_level = !ted->bus.ba_low;
    pin_states[43].drive_direction = true;
    pin_states[43].high_impedance = false;

    // Video output pins (always driven)
    pin_states[21].signal_level = true; // LUMA (pin 22)
    pin_states[21].drive_direction = true;
    pin_states[21].high_impedance = false;
    pin_states[22].signal_level = true; // CHROMA (pin 23)
    pin_states[22].drive_direction = true;
    pin_states[22].high_impedance = false;

    // Sound output (pin 27, index 26) — active if any channel enabled
    bool sound_active = ted->sound.ch1_enabled || ted->sound.ch2_enabled || ted->sound.noise_enabled;
    pin_states[26].signal_level = sound_active;
    pin_states[26].drive_direction = true;
    pin_states[26].high_impedance = false;

    return pin_states;
}

static ChipLayout& get_ted_layout() {
    static ChipLayout layout = create_ted7360_layout();
    return layout;
}

static const char* get_screen_mode_name(uint8_t mode) {
    switch (mode) {
        case TED_GM_STANDARD_TEXT:      return "Standard Text";
        case TED_GM_MULTICOLOR_TEXT:    return "Multicolor Text";
        case TED_GM_STANDARD_BITMAP:    return "Standard Bitmap";
        case TED_GM_MULTICOLOR_BITMAP:  return "Multicolor Bitmap";
        case TED_GM_ECM_TEXT:           return "Extended Color Text";
        default:                        return "Invalid Mode";
    }
}

// ============================================================================
// TED 7360 GUI DEBUG WINDOW
// ============================================================================

void ted7360_render_debug_window(void* chip, bool* show_window) {
    ted7360_t* ted = (ted7360_t*)chip;
    if (!ted || !show_window || !*show_window) return;

#ifdef IMGUI_VERSION
    if (!ImGui::Begin("TED 7360 Debug", show_window)) {
        ImGui::End();
        return;
    }

    // Two-column layout
    ImVec2 window_size = ImGui::GetWindowSize();

    ImVec2 chip_viz_size = ImVec2(260.0f, 0);
    if (ImGui::BeginChild("ChipVisualization", chip_viz_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Chip Visualization");
        ImGui::Separator();

        ImVec2 chip_center = ImGui::GetCursorScreenPos();
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        chip_center.x += content_region.x * 0.5f;
        chip_center.y += 240.0f; // 48-pin is taller than 40-pin

        ChipVisualization& renderer = GetGlobalChipRenderer();
        ChipLayout& layout = get_ted_layout();
        std::vector<PinSignalState> pin_states = get_ted_pin_states(ted, &layout);
        renderer.render(layout, chip_center, pin_states, "TED 7360");
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 5.0f);

    ImVec2 right_column_size = ImVec2(window_size.x - 280.0f, 0);
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("TED 7360 - Text Editing Device");
        ImGui::Separator();

        // Raster Information
        if (ImGui::CollapsingHeader("Raster Information", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Raster Line:   %d / %d", ted->timing.raster_counter, ted->timing.lines_per_frame);
            ImGui::Text("X Cycle:       %d / %d", ted->timing.x_cycle, ted->timing.cpu_cycles_per_line);
            ImGui::Text("Frame Count:   %u", ted->timing.frame_count);
            ImGui::Text("Standard:      %s", ted->timing.is_pal ? "PAL" : "NTSC");
            ImGui::Text("DMA Line:      %s", ted->video_logic.is_dma_line ? "YES" : "NO");
            ImGui::Text("BA Low:        %s", ted->bus.ba_low ? "YES" : "NO");

            float raster_progress = (float)ted->timing.raster_counter / (float)ted->timing.lines_per_frame;
            ImGui::ProgressBar(raster_progress, ImVec2(-1, 0), NULL);
        }

        // Control Registers
        if (ImGui::CollapsingHeader("Control Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
            uint8_t cr1 = ted->registers.data[TED_REG_CONTROL1];
            uint8_t cr2 = ted->registers.data[TED_REG_CONTROL2];

            ImGui::Text("Control 1 ($FF06): $%02X", cr1);
            ImGui::Indent(20.0f);
            ImGui::Text("DEN: %d  BMM: %d  ECM: %d  RSEL: %d  YSCROLL: %d",
                   (cr1 & TED_CR1_DEN) ? 1 : 0,
                   (cr1 & TED_CR1_BMM) ? 1 : 0,
                   (cr1 & TED_CR1_ECM) ? 1 : 0,
                   (cr1 & TED_CR1_RSEL) ? 1 : 0,
                   cr1 & TED_CR1_YSCROLL_MASK);
            ImGui::Unindent(20.0f);

            ImGui::Text("Control 2 ($FF07): $%02X", cr2);
            ImGui::Indent(20.0f);
            ImGui::Text("MCM: %d  CSEL: %d  FREEZE: %d  PAL: %d  RVS: %d  XSCROLL: %d",
                   (cr2 & TED_CR2_MCM) ? 1 : 0,
                   (cr2 & TED_CR2_CSEL) ? 1 : 0,
                   (cr2 & TED_CR2_FREEZE) ? 1 : 0,
                   (cr2 & TED_CR2_PAL_NTSC) ? 0 : 1,  // 0=PAL
                   (cr2 & TED_CR2_RVS) ? 1 : 0,
                   cr2 & TED_CR2_XSCROLL_MASK);
            ImGui::Unindent(20.0f);

            ImGui::Text("Graphics Mode: %s", get_screen_mode_name(ted->sequencer.graphics_mode));
        }

        // Timers
        if (ImGui::CollapsingHeader("Timers", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Timer 1: $%04X (%d)  Latch: $%04X",
                        ted->timer1.counter, ted->timer1.counter, ted->timer1.latch);
            ImGui::Text("Timer 2: $%04X (%d)  Latch: $%04X",
                        ted->timer2.counter, ted->timer2.counter, ted->timer2.latch);
            ImGui::Text("Timer 3: $%04X (%d)  Latch: $%04X",
                        ted->timer3.counter, ted->timer3.counter, ted->timer3.latch);
        }

        // Sound
        if (ImGui::CollapsingHeader("Sound", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Channel 1: %s  Freq: $%04X (%d)",
                        ted->sound.ch1_enabled ? "ON " : "OFF",
                        ted->sound.freq1, ted->sound.freq1);
            ImGui::Text("Channel 2: %s  Freq: $%04X (%d)  Noise: %s",
                        ted->sound.ch2_enabled ? "ON " : "OFF",
                        ted->sound.freq2, ted->sound.freq2,
                        ted->sound.noise_enabled ? "YES" : "NO");
            ImGui::Text("Volume:    %d", ted->sound.volume);
        }

        // IRQ
        if (ImGui::CollapsingHeader("Interrupts", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("IRQ Status ($FF09): $%02X", ted->irq_status);
            ImGui::Text("IRQ Mask   ($FF0A): $%02X", ted->irq_mask);
            ImGui::Text("IRQ Pending: %s", ted7360_irq_pending(ted) ? "YES" : "NO");
            ImGui::Indent(20.0f);
            ImGui::Text("Raster:  %s", (ted->irq_status & TED_IRQ_RASTER) ? "SET" : "---");
            ImGui::Text("Timer 1: %s", (ted->irq_status & TED_IRQ_TIMER1) ? "SET" : "---");
            ImGui::Text("Timer 2: %s", (ted->irq_status & TED_IRQ_TIMER2) ? "SET" : "---");
            ImGui::Text("Timer 3: %s", (ted->irq_status & TED_IRQ_TIMER3) ? "SET" : "---");
            ImGui::Unindent(20.0f);
        }

        // Memory Mapping
        if (ImGui::CollapsingHeader("Memory Mapping")) {
            ImGui::Text("Screen Base:  $%04X", ted->memory.screen_base);
            ImGui::Text("Char Base:    $%04X", ted->memory.char_base);
            ImGui::Text("Bitmap Base:  $%04X", ted->memory.bitmap_base);
            ImGui::Text("ROM Enabled:  %s", ted->rom_enabled ? "YES" : "NO");
        }

        // Video Logic
        if (ImGui::CollapsingHeader("Video Logic")) {
            ImGui::Text("Display State: %s", ted->video_logic.display_state ? "Display" : "Idle");
            ImGui::Text("VC:    $%04X  VCBASE: $%04X", ted->video_logic.vc, ted->video_logic.vcbase);
            ImGui::Text("RC:    %d     VMLI:   %d", ted->video_logic.rc, ted->video_logic.vmli);
            ImGui::Text("Border Main FF: %s  Vert FF: %s",
                        ted->border.main_ff ? "ON" : "OFF",
                        ted->border.vert_ff ? "ON" : "OFF");
        }

        // Colors
        if (ImGui::CollapsingHeader("Colors")) {
            ImGui::Text("BG0 ($FF15): $%02X", ted->registers.data[TED_REG_COLOR_BG0]);
            ImGui::Text("BG1 ($FF16): $%02X", ted->registers.data[TED_REG_COLOR_BG1]);
            ImGui::Text("BG2 ($FF17): $%02X", ted->registers.data[TED_REG_COLOR_BG2]);
            ImGui::Text("BG3 ($FF18): $%02X", ted->registers.data[TED_REG_COLOR_BG3]);
            ImGui::Text("Border ($FF19): $%02X", ted->registers.data[TED_REG_BORDER]);
        }
    }
    ImGui::EndChild();

    ImGui::End();
#endif
}

// ============================================================================
// TED 7360 GUI SETTINGS WINDOW
// ============================================================================

void ted7360_render_settings_window(void* chip, bool* show_window) {
    ted7360_t* ted = (ted7360_t*)chip;
    if (!ted || !show_window || !*show_window) return;

#ifdef IMGUI_VERSION
    if (!ImGui::Begin("TED 7360 Settings", show_window, 0)) {
        ImGui::End();
        return;
    }

    ImGui::Text("TED 7360 - Text Editing Device Configuration");
    ImGui::Separator();
    ImGui::Text("Chip Type: MOS 7360 TED (48-pin DIP)");
    ImGui::Text("Standard: %s", ted->timing.is_pal ? "PAL" : "NTSC");
    ImGui::Text("Lines/frame: %d", ted->timing.lines_per_frame);
    ImGui::Text("CPU cycles/line: %d", ted->timing.cpu_cycles_per_line);

    ImGui::Separator();

    // Raw register dump
    if (ImGui::CollapsingHeader("Raw Registers ($FF00-$FF1F)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static const char* reg_names[] = {
            "Timer1 Lo",  "Timer1 Hi",  "Timer2 Lo",  "Timer2 Hi",
            "Timer3 Lo",  "Timer3 Hi",  "Control 1",  "Control 2",
            "Keyboard",   "IRQ Status", "IRQ Mask",   "Cursor Lo",
            "Cursor Hi",  "Sound1 Lo",  "Sound1 Hi",  "Sound2 Lo",
            "Sound2 Hi",  "Sound Ctrl", "Mem Ctrl",   "Char Hi",
            "Bitmap Addr","Color BG0",  "Color BG1",  "Color BG2",
            "Color BG3",  "Border",     "CharPos Hi", "Raster Lo",
            "VPos",       "HPos",       "Flash",      "ROM/RAM"
        };
        for (int i = 0; i < TED_NUM_REGS; i++) {
            ImGui::Text("$FF%02X %-12s: $%02X", i, reg_names[i], ted->registers.data[i]);
        }
    }

    ImGui::End();
#endif
}
