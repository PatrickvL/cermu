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
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif
#include <cstdio>
#include <memory>

// Forward declarations
static const char* mos6522_get_via_name(mos6522_t* via);

// ============================================================================
// MOS6522 VIA LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_mos6522_layout() {
    ChipLayout layout = create_dip40_layout();

    layout.left_pins.clear();
    layout.right_pins.clear();

    layout.markings = {
        "MOS6522",
        "MOS Technology",
        {}, {}, {}, {},
        true, true, false, false
    };

    // Hardware-accurate MOS6522 VIA pinout (40-pin DIP)
    PIN_LR(layout,  1, VSS,     CA1, 40)        // gnd / handshake in
    PIN_LR(layout,  2, PA0,     CA2, 39)        // port A lo / handshake I/O
    PIN_LR(layout,  3, PA1,     A3, 38)         // / reg sel 3
    PIN_LR(layout,  4, PA2,     A2, 37)         // / reg sel 2
    PIN_LR(layout,  5, PA3,     A1, 36)         // / reg sel 1
    PIN_LR(layout,  6, PA4,     A0, 35)         // / reg sel 0
    PIN_LR(layout,  7, PA5,     _RES, 34)       // / reset
    PIN_LR(layout,  8, PA6,     D7, 33)         // / data hi
    PIN_LR(layout,  9, PA7,     D6, 32)         // port A hi
    PIN_LR(layout, 10, PB0,     D5, 31)         // port B lo
    PIN_LR(layout, 11, PB1,     D4, 30)
    PIN_LR(layout, 12, PB2,     D3, 29)
    PIN_LR(layout, 13, PB3,     D2, 28)
    PIN_LR(layout, 14, PB4,     D1, 27)
    PIN_LR(layout, 15, PB5,     D0, 26)         // / data lo
    PIN_LR(layout, 16, PB6,     PHI2, 25)       // / clock
    PIN_LR(layout, 17, PB7,     CS1, 24)        // port B hi / chip sel 1
    PIN_LR(layout, 18, CB1,     _CS2, 23)       // handshake / chip sel 2
    PIN_LR(layout, 19, CB2,     RW, 22)         // handshake / R/W
    PIN_LR(layout, 20, VDD,     _IRQ, 21)       // +5V / interrupt

    return layout;
}

// ============================================================================
// MOS6522 VIA PIN STATES
// ============================================================================

static std::vector<PinSignalState> get_via_pin_states(mos6522_t* via, const ChipLayout* layout, bus_state_t bus_state) {
    if (!via || !layout) return {};

    // Generic bus-derived pin states (address, data, power, clock, control)
    auto pin_states = populate_pin_states_from_bus(*layout, bus_state);
    int total_pins = static_cast<int>(pin_states.size());

    // VIA specific: Port A pins (PA0-PA7, pins 2-9)
    for (int i = 0; i < 8; i++) {
        int pin_idx = i + 1; // PA0 is pin 2, index 1
        if (pin_idx < total_pins) {
            pin_states[pin_idx].signal_level = (via->port_a_regs.pins & (1 << i)) != 0;
            pin_states[pin_idx].drive_direction = (via->port_a_regs.ddr & (1 << i)) != 0;
            pin_states[pin_idx].high_impedance = !(via->port_a_regs.ddr & (1 << i));
        }
    }

    // Port B pins (PB0-PB7, pins 10-17)
    for (int i = 0; i < 8; i++) {
        int pin_idx = i + 9; // PB0 is pin 10, index 9
        if (pin_idx < total_pins) {
            pin_states[pin_idx].signal_level = (via->port_b_regs.pins & (1 << i)) != 0;
            pin_states[pin_idx].drive_direction = (via->port_b_regs.ddr & (1 << i)) != 0;
            pin_states[pin_idx].high_impedance = !(via->port_b_regs.ddr & (1 << i));
        }
    }

    // IRQ pin (pin 21, index 20) — VIA drives IRQ as output
    if (20 < total_pins) {
        pin_states[20].signal_level = !via->interrupt_active; // Active low
        pin_states[20].drive_direction = true;
        pin_states[20].high_impedance = false;
    }

    return pin_states;
}

static ChipLayout& get_via_layout() {
    static ChipLayout layout = create_mos6522_layout();
    return layout;
}

// ============================================================================
// MOS6522 VIA GUI SETTINGS
// ============================================================================

void mos6522_t::render_settings_content() {
    mos6522_t* via = this;

#ifdef CERMU_HAS_GUI
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
            bool is_output = (via->port_a_regs.ddr & (1 << i)) != 0;
            bool pin_level = (via->port_a_regs.pins & (1 << i)) != 0;
            ImGui::Text("  PA%d: %s  Dir: %s", i, pin_level ? "HIGH" : "LOW",
                        is_output ? "OUT" : "IN");
        }

        ImGui::Separator();

        // Port B pin-by-pin
        ImGui::Text("Port B (pin by pin):");
        for (int i = 0; i < 8; i++) {
            bool is_output = (via->port_b_regs.ddr & (1 << i)) != 0;
            bool pin_level = (via->port_b_regs.pins & (1 << i)) != 0;
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
    if (via->interrupt_bit == BUS_NMI_BIT) {
        return "VIA 1 ($9110)";  // VIA1 at $9110 drives NMI
    } else if (via->interrupt_bit == BUS_IRQ_BIT) {
        return "VIA 2 ($9120)";  // VIA2 at $9120 drives IRQ
    }
    return "VIA";
}

// ============================================================================
// MOS6522 VIA LAYOUT WINDOW (standalone pinout diagram)
// ============================================================================

void mos6522_t::render_layout_content() {
    mos6522_t* via = this;

#ifdef CERMU_HAS_GUI
    const char* via_name = mos6522_get_via_name(via);

    ChipLayout& layout = get_via_layout();
    std::vector<PinSignalState> pin_states = get_via_pin_states(via, &layout, via->bus_snapshot_);
    render_chip_layout(layout, pin_states, via_name);
#endif
}
