/*
 * mos6522_gui.cpp — MOS 6522 VIA Debug/Settings GUI Windows
 *
 * Hardware-accurate 40-pin DIP pinout based on the MOS 6522 datasheet.
 * The VIA (Versatile Interface Adapter) provides two 8-bit bidirectional
 * I/O ports, two 16-bit timer/counters, and a serial shift register.
 *
 * Pinout reference: MOS Technology MOS 6522 Datasheet (1977)
 */

#include "mos6522.h"
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

// Forward declarations
static const char* mos6522_get_via_name(mos6522_t* via);

// ============================================================================
// MOS6522 VIA LAYOUT (40-pin DIP)
// ============================================================================
//
// Hardware-accurate MOS 6522 VIA pinout (MOS Technology datasheet, 1977):
//
//         ╔═══════════╗
//   VSS ──┤ 1      40 ├── CA1
//   PA0 ──┤ 2      39 ├── CA2
//   PA1 ──┤ 3      38 ├── RS3
//   PA2 ──┤ 4      37 ├── RS2
//   PA3 ──┤ 5      36 ├── RS1
//   PA4 ──┤ 6      35 ├── RS0
//   PA5 ──┤ 7      34 ├── /RES
//   PA6 ──┤ 8      33 ├── D7
//   PA7 ──┤ 9      32 ├── D6
//   PB0 ──┤10      31 ├── D5
//   PB1 ──┤11      30 ├── D4
//   PB2 ──┤12      29 ├── D3
//   PB3 ──┤13      28 ├── D2
//   PB4 ──┤14      27 ├── D1
//   PB5 ──┤15      26 ├── D0
//   PB6 ──┤16      25 ├── Φ2
//   PB7 ──┤17      24 ├── CS1
//   CB1 ──┤18      23 ├── /CS2
//   CB2 ──┤19      22 ├── R/W
//   VCC ──┤20      21 ├── /IRQ
//         ╚═══════════╝

inline ChipLayout create_mos6522_layout() {
    ChipLayout layout = create_dip40_layout();

    layout.left_pins.clear();
    layout.right_pins.clear();

    layout.markings = {
        "MOS6522",
        "MOS Technology",
        nullptr, nullptr, nullptr, nullptr,
        true, true, false, false
    };

    // Hardware-accurate MOS6522 VIA pinout (40-pin DIP)
    PIN_LR(layout,  1, VSS,     UNKNOWN, 40)    // Ground / CA1 (handshake input)
    PIN_LR(layout,  2, PA0,     UNKNOWN, 39)    // Port A Bit 0 / CA2 (handshake I/O)
    PIN_LR(layout,  3, PA1,     A3, 38)         // Port A Bit 1 / RS3 (Register Select 3)
    PIN_LR(layout,  4, PA2,     A2, 37)         // Port A Bit 2 / RS2
    PIN_LR(layout,  5, PA3,     A1, 36)         // Port A Bit 3 / RS1
    PIN_LR(layout,  6, PA4,     A0, 35)         // Port A Bit 4 / RS0
    PIN_LR(layout,  7, PA5,     RES, 34)        // Port A Bit 5 / /RESET
    PIN_LR(layout,  8, PA6,     D7, 33)         // Port A Bit 6 / Data 7
    PIN_LR(layout,  9, PA7,     D6, 32)         // Port A Bit 7 / Data 6
    PIN_LR(layout, 10, PB0,     D5, 31)         // Port B Bit 0 / Data 5
    PIN_LR(layout, 11, PB1,     D4, 30)         // Port B Bit 1 / Data 4
    PIN_LR(layout, 12, PB2,     D3, 29)         // Port B Bit 2 / Data 3
    PIN_LR(layout, 13, PB3,     D2, 28)         // Port B Bit 3 / Data 2
    PIN_LR(layout, 14, PB4,     D1, 27)         // Port B Bit 4 / Data 1
    PIN_LR(layout, 15, PB5,     D0, 26)         // Port B Bit 5 / Data 0
    PIN_LR(layout, 16, PB6,     PHI2, 25)       // Port B Bit 6 / Clock
    PIN_LR(layout, 17, PB7,     CS1, 24)        // Port B Bit 7 / Chip Select 1
    PIN_LR(layout, 18, UNKNOWN, CS2, 23)        // CB1 / /CS2
    PIN_LR(layout, 19, UNKNOWN, RW, 22)         // CB2 / Read/Write
    PIN_LR(layout, 20, VDD,     IRQ, 21)        // +5V Power / /IRQ

    return layout;
}

// ============================================================================
// MOS6522 VIA PIN STATES
// ============================================================================

static std::vector<PinSignalState> get_via_pin_states(mos6522_t* via, const ChipLayout* layout) {
    std::vector<PinSignalState> pin_states;
    if (!via || !layout) return pin_states;

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

    // Port A pins (PA0-PA7, pins 2-9)
    for (int i = 0; i < 8; i++) {
        int pin_idx = i + 1; // PA0 is pin 2, index 1
        if (pin_idx < total_pins) {
            pin_states[pin_idx].signal_level = (via->port_a_data & (1 << i)) != 0;
            pin_states[pin_idx].drive_direction = (via->port_a_ddr & (1 << i)) != 0;
            pin_states[pin_idx].high_impedance = !(via->port_a_ddr & (1 << i));
            pin_states[pin_idx].signal_valid = true;
        }
    }

    // Port B pins (PB0-PB7, pins 10-17)
    for (int i = 0; i < 8; i++) {
        int pin_idx = i + 9; // PB0 is pin 10, index 9
        if (pin_idx < total_pins) {
            pin_states[pin_idx].signal_level = (via->port_b_data & (1 << i)) != 0;
            pin_states[pin_idx].drive_direction = (via->port_b_ddr & (1 << i)) != 0;
            pin_states[pin_idx].high_impedance = !(via->port_b_ddr & (1 << i));
            pin_states[pin_idx].signal_valid = true;
        }
    }

    // IRQ pin (pin 21, index 20)
    if (20 < total_pins) {
        pin_states[20].signal_level = !via->interrupt_active; // IRQ is active low
        pin_states[20].signal_valid = true;
        pin_states[20].drive_direction = true;
        pin_states[20].high_impedance = false;
    }

    // Power pins
    pin_states[0].signal_level = false;  // VSS (Ground, pin 1)
    pin_states[0].high_impedance = false;
    pin_states[19].signal_level = true;  // VCC (+5V, pin 20)
    pin_states[19].high_impedance = false;
    if (33 < total_pins) {
        pin_states[33].signal_level = true; // /RES (Reset, pin 34) - active high when not reset
        pin_states[33].high_impedance = false;
    }

    return pin_states;
}

static ChipLayout& get_via_layout() {
    static ChipLayout layout = create_mos6522_layout();
    return layout;
}

// ============================================================================
// MOS6522 VIA GUI DEBUG WINDOW
// ============================================================================

void mos6522_s::render_debug_content() {
    mos6522_t* via = this;

#ifdef IMGUI_VERSION
    const char* via_name = mos6522_get_via_name(via);

    // Two-column layout: chip visualization | debug info
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
        ChipLayout& layout = get_via_layout();
        std::vector<PinSignalState> pin_states = get_via_pin_states(via, &layout);
        renderer.render(layout, chip_center, pin_states, via_name);
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 5.0f);

    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0);
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Versatile Interface Adapter - %s", via_name);
        ImGui::Separator();

        // Data Ports
        if (ImGui::CollapsingHeader("Data Ports", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Port A Data (ORA):  $%02X", via->port_a_data);
            ImGui::Text("Port A DDR (DDRA):  $%02X", via->port_a_ddr);
            ImGui::Text("Port B Data (ORB):  $%02X", via->port_b_data);
            ImGui::Text("Port B DDR (DDRB):  $%02X", via->port_b_ddr);
        }

        // Timers
        if (ImGui::CollapsingHeader("Timers", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Timer 1 Counter: $%04X (%d)", via->timer1_counter, via->timer1_counter);
            ImGui::Text("Timer 1 Latch:   $%04X", via->timer1_latch);
            ImGui::Text("Timer 1 Running: %s", via->timer1_running ? "YES" : "NO");
            ImGui::Text("Timer 1 Mode:    %s", via->timer1_continuous ? "Free-running" : "One-shot");

            ImGui::Separator();

            ImGui::Text("Timer 2 Counter: $%04X (%d)", via->timer2_counter, via->timer2_counter);
            ImGui::Text("Timer 2 Latch:   $%04X", via->timer2_latch);
            ImGui::Text("Timer 2 Running: %s", via->timer2_running ? "YES" : "NO");
        }

        // Control Registers
        if (ImGui::CollapsingHeader("Control Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("ACR (Auxiliary Control): $%02X", via->acr);
            ImGui::Indent(20.0f);
            ImGui::Text("T1 Control:    %s", (via->acr & MOS6522_ACR_T1_CONT) ? "Free-running" : "One-shot");
            ImGui::Text("T1 PB7 Output: %s", (via->acr & MOS6522_ACR_T1_PB7) ? "YES" : "NO");
            ImGui::Text("T2 Control:    %s", (via->acr & MOS6522_ACR_T2_CONT) ? "Count PB6" : "Timed");
            ImGui::Text("SR Mode:       %d", (via->acr & MOS6522_ACR_SR_MODE) >> 2);
            ImGui::Unindent(20.0f);

            ImGui::Text("PCR (Peripheral Control): $%02X", via->pcr);
            ImGui::Indent(20.0f);
            ImGui::Text("CA1 Control: %s", (via->pcr & 0x01) ? "Positive edge" : "Negative edge");
            ImGui::Text("CA2 Control: %d", (via->pcr >> 1) & 0x07);
            ImGui::Text("CB1 Control: %s", (via->pcr & 0x10) ? "Positive edge" : "Negative edge");
            ImGui::Text("CB2 Control: %d", (via->pcr >> 5) & 0x07);
            ImGui::Unindent(20.0f);
        }

        // Interrupts
        if (ImGui::CollapsingHeader("Interrupt Control", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("IFR (Interrupt Flags):  $%02X", via->ifr);
            ImGui::Text("IER (Interrupt Enable): $%02X", via->ier);
            ImGui::Text("IRQ Active: %s", via->interrupt_active ? "YES" : "NO");
            ImGui::Indent(20.0f);
            ImGui::Text("Timer 1:  %s", (via->ifr & MOS6522_IFR_T1)  ? "SET" : "---");
            ImGui::Text("Timer 2:  %s", (via->ifr & MOS6522_IFR_T2)  ? "SET" : "---");
            ImGui::Text("CB1:      %s", (via->ifr & MOS6522_IFR_CB1) ? "SET" : "---");
            ImGui::Text("CB2:      %s", (via->ifr & MOS6522_IFR_CB2) ? "SET" : "---");
            ImGui::Text("Shift:    %s", (via->ifr & MOS6522_IFR_SR)  ? "SET" : "---");
            ImGui::Text("CA1:      %s", (via->ifr & MOS6522_IFR_CA1) ? "SET" : "---");
            ImGui::Text("CA2:      %s", (via->ifr & MOS6522_IFR_CA2) ? "SET" : "---");
            ImGui::Unindent(20.0f);
        }

        // Shift Register
        if (ImGui::CollapsingHeader("Shift Register")) {
            ImGui::Text("Shift Register: $%02X", via->shift_register);
            ImGui::Text("Shift Counter:  %d", via->shift_counter);
        }
    }
    ImGui::EndChild();
#endif
}

// ============================================================================
// MOS6522 VIA GUI SETTINGS
// ============================================================================

void mos6522_s::render_settings_content() {
    mos6522_t* via = this;

#ifdef IMGUI_VERSION
    const char* via_name = mos6522_get_via_name(via);

    ImGui::Text("Versatile Interface Adapter - %s Configuration", via_name);
    ImGui::Separator();
    ImGui::Text("Chip Type: MOS 6522 VIA");

    // Extended register view
    if (ImGui::CollapsingHeader("Raw Registers ($00-$0F)", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < 16; i++) {
            ImGui::Text("$%02X: $%02X", i, via->registers[i]);
        }
    }

    if (ImGui::CollapsingHeader("Port Details")) {
        // Port A pin-by-pin
        ImGui::Text("Port A (pin by pin):");
        for (int i = 0; i < 8; i++) {
            bool is_output = (via->port_a_ddr & (1 << i)) != 0;
            bool pin_level = (via->port_a_data & (1 << i)) != 0;
            ImGui::Text("  PA%d: %s  Dir: %s", i, pin_level ? "HIGH" : "LOW",
                        is_output ? "OUT" : "IN");
        }

        ImGui::Separator();

        // Port B pin-by-pin
        ImGui::Text("Port B (pin by pin):");
        for (int i = 0; i < 8; i++) {
            bool is_output = (via->port_b_ddr & (1 << i)) != 0;
            bool pin_level = (via->port_b_data & (1 << i)) != 0;
            ImGui::Text("  PB%d: %s  Dir: %s", i, pin_level ? "HIGH" : "LOW",
                        is_output ? "OUT" : "IN");
        }
    }
#endif
}

// ============================================================================
// HELPER: VIA IDENTIFICATION
// ============================================================================

static const char* mos6522_get_via_name(mos6522_t* via) {
    // The VIC-20 has two VIAs distinguished by their interrupt line
    if (via->interrupt_line == BUS_IRQ_BIT) {
        return "VIA 1 ($9110)";
    } else if (via->interrupt_line == BUS_NMI_BIT) {
        return "VIA 2 ($9120)";
    }
    return "VIA";
}

// ============================================================================
// MOS6522 VIA LAYOUT WINDOW (standalone pinout diagram)
// ============================================================================

void mos6522_s::render_layout_content() {
    mos6522_t* via = this;

#ifdef IMGUI_VERSION
    const char* via_name = mos6522_get_via_name(via);

    ChipLayout& layout = get_via_layout();
    std::vector<PinSignalState> pin_states = get_via_pin_states(via, &layout);
    render_chip_layout(layout, pin_states, via_name);
#endif
}
