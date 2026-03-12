/*
 * pia6820_gui.cpp — Motorola 6820 PIA Debug/Layout GUI
 *
 * 40-pin DIP pinout based on the Motorola MC6820 / MC6821 datasheet.
 * The 6820 provides two 8-bit bidirectional I/O ports with handshake
 * control lines.  Used in the Apple 1, various 6800/6502 systems.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/io/pia6820.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

// ============================================================================
// 40-pin DIP Layout — Motorola 6820 PIA
// ============================================================================
//
//   Pin  1: VSS           Pin 40: CA1
//   Pin  2: PA0           Pin 39: CA2
//   Pin  3: PA1           Pin 38: /IRQA
//   Pin  4: PA2           Pin 37: /IRQB
//   Pin  5: PA3           Pin 36: RS0
//   Pin  6: PA4           Pin 35: RS1
//   Pin  7: PA5           Pin 34: /RES
//   Pin  8: PA6           Pin 33: D0
//   Pin  9: PA7           Pin 32: D1
//   Pin 10: PB0           Pin 31: D2
//   Pin 11: PB1           Pin 30: D3
//   Pin 12: PB2           Pin 29: D4
//   Pin 13: PB3           Pin 28: D5
//   Pin 14: PB4           Pin 27: D6
//   Pin 15: PB5           Pin 26: D7
//   Pin 16: PB6           Pin 25: E (Enable)
//   Pin 17: PB7           Pin 24: CS1
//   Pin 18: CB1           Pin 23: /CS2
//   Pin 19: CB2           Pin 22: CS0
//   Pin 20: VCC           Pin 21: R/W
//

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase layout virtuals
// ============================================================================

ChipLayout* pia6820_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();

        layout.markings.part_number  = "MC6821";
        layout.markings.manufacturer = "Motorola";
        layout.markings.custom_text  = "PIA";

        //                  LEFT                         RIGHT
        PIN_LR(layout,  1, VSS,         CA1,         40);
        PIN_LR(layout,  2, PA0,         CA2,         39);
        PIN_LR(layout,  3, PA1,         _IRQA,       38);
        PIN_LR(layout,  4, PA2,         _IRQB,       37);
        PIN_LR(layout,  5, PA3,         RS0,         36);
        PIN_LR(layout,  6, PA4,         RS1,         35);
        PIN_LR(layout,  7, PA5,         _RES,        34);
        PIN_LR(layout,  8, PA6,         D0,          33);
        PIN_LR(layout,  9, PA7,         D1,          32);
        PIN_LR(layout, 10, PB0,         D2,          31);
        PIN_LR(layout, 11, PB1,         D3,          30);
        PIN_LR(layout, 12, PB2,         D4,          29);
        PIN_LR(layout, 13, PB3,         D5,          28);
        PIN_LR(layout, 14, PB4,         D6,          27);
        PIN_LR(layout, 15, PB5,         D7,          26);
        PIN_LR(layout, 16, PB6,         ENABLE,      25);
        PIN_LR(layout, 17, PB7,         CS1,         24);
        PIN_LR(layout, 18, CB1,         _CS2,        23);
        PIN_LR(layout, 19, CB2,         CS0,         22);
        PIN_LR(layout, 20, VCC,         RW,          21);

        return layout;
    }();
    return &layout;
}

// Helper: derive PIA6820 pin states from bus snapshot + chip internals
static std::vector<PinSignalState> get_pia6820_pin_states(
        pia6820_t* pia, const ChipLayout* layout, bus_state_t bus_state) {
    if (!pia || !layout) return {};

    // Generic bus-derived states (data bus, power, R/W, /RES)
    auto ps = populate_pin_states_from_bus(*layout, bus_state);

    // --- PA0-PA7 (pins 2-9, indices 1-8) — Port A I/O ---
    for (int i = 0; i < 8; i++) {
        bool is_output = (pia->port_a_direction >> i) & 1;
        bool data_val  = (pia->port_a_data >> i) & 1;
        ps[1 + i].signal_level    = data_val;
        ps[1 + i].drive_direction = is_output;
        ps[1 + i].high_impedance  = false;
        ps[1 + i].signal_valid    = true;
    }

    // --- PB0-PB7 (pins 10-17, indices 9-16) — Port B I/O ---
    for (int i = 0; i < 8; i++) {
        bool is_output = (pia->port_b_direction >> i) & 1;
        bool data_val  = (pia->port_b_data >> i) & 1;
        ps[9 + i].signal_level    = data_val;
        ps[9 + i].drive_direction = is_output;
        ps[9 + i].high_impedance  = false;
        ps[9 + i].signal_valid    = true;
    }

    // --- CB1 (pin 18, idx 17) — always input ---
    ps[17].signal_level    = pia->cb1_state;
    ps[17].drive_direction = false;
    ps[17].high_impedance  = false;
    ps[17].signal_valid    = true;

    // --- CB2 (pin 19, idx 18) — direction depends on control register bit 5 ---
    bool cb2_is_output = (pia->port_b_control >> 5) & 1;
    ps[18].signal_level    = pia->cb2_state;
    ps[18].drive_direction = cb2_is_output;
    ps[18].high_impedance  = false;
    ps[18].signal_valid    = true;

    // --- CS0 (pin 22, idx 21) — chip select ---
    ps[21].signal_level    = true;
    ps[21].drive_direction = false;
    ps[21].high_impedance  = false;
    ps[21].signal_valid    = true;

    // --- /CS2 (pin 23, idx 22) — chip select (active low) ---
    ps[22].signal_level    = false; // asserted (selected)
    ps[22].drive_direction = false;
    ps[22].high_impedance  = false;
    ps[22].signal_valid    = true;

    // --- CS1 (pin 24, idx 23) — chip select ---
    ps[23].signal_level    = true;
    ps[23].drive_direction = false;
    ps[23].high_impedance  = false;
    ps[23].signal_valid    = true;

    // --- ENABLE (pin 25, idx 24) — clock enable input ---
    ps[24].signal_level    = true;
    ps[24].drive_direction = false;
    ps[24].high_impedance  = false;
    ps[24].signal_valid    = true;

    // --- RS0 (pin 36, idx 35), RS1 (pin 35, idx 34) — register select ---
    ps[35].signal_level    = BUS_GET_ADDR(bus_state) & 1;        // RS0 = A0
    ps[35].drive_direction = false;
    ps[35].high_impedance  = false;
    ps[35].signal_valid    = true;
    ps[34].signal_level    = (BUS_GET_ADDR(bus_state) >> 1) & 1; // RS1 = A1
    ps[34].drive_direction = false;
    ps[34].high_impedance  = false;
    ps[34].signal_valid    = true;

    // --- /IRQB (pin 37, idx 36) — active low, open-drain ---
    bool irqb_asserted = pia->irq_b1 || pia->irq_b2;
    ps[36].signal_level    = !irqb_asserted;
    ps[36].drive_direction = true;
    ps[36].high_impedance  = !irqb_asserted;
    ps[36].signal_valid    = true;

    // --- /IRQA (pin 38, idx 37) — active low, open-drain ---
    bool irqa_asserted = pia->irq_a1 || pia->irq_a2;
    ps[37].signal_level    = !irqa_asserted;
    ps[37].drive_direction = true;
    ps[37].high_impedance  = !irqa_asserted;
    ps[37].signal_valid    = true;

    // --- CA2 (pin 39, idx 38) — direction depends on control register bit 5 ---
    bool ca2_is_output = (pia->port_a_control >> 5) & 1;
    ps[38].signal_level    = pia->ca2_state;
    ps[38].drive_direction = ca2_is_output;
    ps[38].high_impedance  = false;
    ps[38].signal_valid    = true;

    // --- CA1 (pin 40, idx 39) — always input ---
    ps[39].signal_level    = pia->ca1_state;
    ps[39].drive_direction = false;
    ps[39].high_impedance  = false;
    ps[39].signal_valid    = true;

    return ps;
}

std::vector<PinSignalState> pia6820_t::get_layout_pin_states(ChipLayout& layout) {
    return get_pia6820_pin_states(this, &layout, bus_snapshot_);
}

#endif // CERMU_HAS_GUI
