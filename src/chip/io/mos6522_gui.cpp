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

#ifdef CERMU_HAS_GUI

// ============================================================================
// MOS6522 VIA layout virtuals
// ============================================================================

ChipLayout* mos6522_t::create_chip_layout() const {
    static ChipLayout layout = [] {
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
        // Per MOS Technology MOS 6522 Versatile Interface Adapter datasheet (1977)
        PIN_LR(layout,  1, VSS,     CA1, 40)        // gnd / handshake in
        PIN_LR(layout,  2, PA0,     CA2, 39)        // port A lo / handshake I/O
        PIN_LR(layout,  3, PA1,     A3, 38)         // / RS3
        PIN_LR(layout,  4, PA2,     A2, 37)         // / RS2
        PIN_LR(layout,  5, PA3,     A1, 36)         // / RS1
        PIN_LR(layout,  6, PA4,     A0, 35)         // / RS0
        PIN_LR(layout,  7, PA5,     _RES, 34)       // / reset
        PIN_LR(layout,  8, PA6,     D0, 33)         // / data lo
        PIN_LR(layout,  9, PA7,     D1, 32)         // port A hi
        PIN_LR(layout, 10, PB0,     D2, 31)         // port B lo
        PIN_LR(layout, 11, PB1,     D3, 30)
        PIN_LR(layout, 12, PB2,     D4, 29)
        PIN_LR(layout, 13, PB3,     D5, 28)
        PIN_LR(layout, 14, PB4,     D6, 27)
        PIN_LR(layout, 15, PB5,     D7, 26)         // / data hi
        PIN_LR(layout, 16, PB6,     PHI2, 25)       // / clock
        PIN_LR(layout, 17, PB7,     CS1, 24)        // port B hi / chip sel 1
        PIN_LR(layout, 18, CB1,     _CS2, 23)       // handshake / chip sel 2
        PIN_LR(layout, 19, CB2,     RW, 22)         // handshake / R/W
        PIN_LR(layout, 20, VDD,     _IRQ, 21)       // +5V / interrupt

        return layout;
    }();
    return &layout;
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
            pin_states[pin_idx].signal_level = (via->port_a_pins_ & (1 << i)) != 0;
            pin_states[pin_idx].drive_direction = (via->regs_[MOS6522_DDRA] & (1 << i)) != 0;
            pin_states[pin_idx].high_impedance = !(via->regs_[MOS6522_DDRA] & (1 << i));
        }
    }

    // Port B pins (PB0-PB7, pins 10-17)
    for (int i = 0; i < 8; i++) {
        int pin_idx = i + 9; // PB0 is pin 10, index 9
        if (pin_idx < total_pins) {
            pin_states[pin_idx].signal_level = (via->port_b_pins_ & (1 << i)) != 0;
            pin_states[pin_idx].drive_direction = (via->regs_[MOS6522_DDRB] & (1 << i)) != 0;
            pin_states[pin_idx].high_impedance = !(via->regs_[MOS6522_DDRB] & (1 << i));
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

std::vector<PinSignalState> mos6522_t::get_layout_pin_states(ChipLayout& layout) {
    return get_via_pin_states(this, &layout, bus_snapshot_);
}

// ============================================================================
// HELPER: VIA IDENTIFICATION
// ============================================================================

static const char* mos6522_get_via_name(const mos6522_t* via) {
    // The VIC-20 has two VIAs distinguished by their interrupt line
    if (via->interrupt_bit == BUS_NMI_BIT) {
        return "VIA 1 ($9110)";  // VIA1 at $9110 drives NMI
    } else if (via->interrupt_bit == BUS_IRQ_BIT) {
        return "VIA 2 ($9120)";  // VIA2 at $9120 drives IRQ
    }
    return "VIA";
}

const char* mos6522_t::get_layout_chip_name() const {
    return mos6522_get_via_name(this);
}

// ============================================================================
// MOS6522 VIA GUI SETTINGS
// ============================================================================

void mos6522_t::render_settings_content() {
    mos6522_t* via = this;
    const char* via_name = mos6522_get_via_name(via);

    ImGui::Text("Versatile Interface Adapter - %s Configuration", via_name);
    ImGui::Separator();
    ImGui::Text("Chip Type: MOS 6522 VIA");

    if (ImGui::CollapsingHeader("Port Details")) {
        // Port A pin-by-pin
        ImGui::Text("Port A (pin by pin):");
        for (int i = 0; i < 8; i++) {
            bool is_output = (via->regs_[MOS6522_DDRA] & (1 << i)) != 0;
            bool pin_level = (via->port_a_pins_ & (1 << i)) != 0;
            ImGui::Text("  PA%d: %s  Dir: %s", i, pin_level ? "HIGH" : "LOW",
                        is_output ? "OUT" : "IN");
        }

        ImGui::Separator();

        // Port B pin-by-pin
        ImGui::Text("Port B (pin by pin):");
        for (int i = 0; i < 8; i++) {
            bool is_output = (via->regs_[MOS6522_DDRB] & (1 << i)) != 0;
            bool pin_level = (via->port_b_pins_ & (1 << i)) != 0;
            ImGui::Text("  PB%d: %s  Dir: %s", i, pin_level ? "HIGH" : "LOW",
                        is_output ? "OUT" : "IN");
        }
    }
}

#endif // CERMU_HAS_GUI
