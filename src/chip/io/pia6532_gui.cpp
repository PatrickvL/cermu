/*
 * pia6532_gui.cpp — MOS 6532 RIOT Debug/Layout GUI
 *
 * 40-pin DIP pinout based on the MOS 6532 (RIOT) datasheet.
 * The 6532 combines 128 bytes RAM, two 8-bit I/O ports, and a
 * programmable interval timer.  Used in the Atari 2600 VCS.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/io/pia6532.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

// ============================================================================
// 40-pin DIP Layout — MOS 6532 RIOT
// ============================================================================
//
//   Pin  1: VSS           Pin 40: VCC
//   Pin  2: A6            Pin 39: /CS2
//   Pin  3: Φ2            Pin 38: CS1
//   Pin  4: RS            Pin 37: PB7
//   Pin  5: R/W           Pin 36: PB6
//   Pin  6: /RES          Pin 35: PB5
//   Pin  7: /IRQ          Pin 34: PB4
//   Pin  8: D7            Pin 33: PB3
//   Pin  9: D6            Pin 32: PB2
//   Pin 10: D5            Pin 31: PB1
//   Pin 11: D4            Pin 30: PB0
//   Pin 12: D3            Pin 29: PA7
//   Pin 13: D2            Pin 28: PA6
//   Pin 14: D1            Pin 27: PA5
//   Pin 15: D0            Pin 26: PA4
//   Pin 16: A0            Pin 25: PA3
//   Pin 17: A1            Pin 24: PA2
//   Pin 18: A2            Pin 23: PA1
//   Pin 19: A3            Pin 22: PA0
//   Pin 20: A4            Pin 21: A5
//

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase layout virtuals
// ============================================================================

ChipLayout* pia6532_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();

        layout.markings.part_number  = "MOS 6532";
        layout.markings.manufacturer = "MOS Technology";
        layout.markings.custom_text  = "RIOT";

        //                  LEFT                         RIGHT
        PIN_LR(layout,  1, VSS,         VCC,         40);
        PIN_LR(layout,  2, A6,          _CS2,        39);
        PIN_LR(layout,  3, PHI2,        CS1,         38);
        PIN_LR(layout,  4, RS,          PB7,         37);
        PIN_LR(layout,  5, RW,          PB6,         36);
        PIN_LR(layout,  6, _RES,        PB5,         35);
        PIN_LR(layout,  7, _IRQ,        PB4,         34);
        PIN_LR(layout,  8, D7,          PB3,         33);
        PIN_LR(layout,  9, D6,          PB2,         32);
        PIN_LR(layout, 10, D5,          PB1,         31);
        PIN_LR(layout, 11, D4,          PB0,         30);
        PIN_LR(layout, 12, D3,          PA7,         29);
        PIN_LR(layout, 13, D2,          PA6,         28);
        PIN_LR(layout, 14, D1,          PA5,         27);
        PIN_LR(layout, 15, D0,          PA4,         26);
        PIN_LR(layout, 16, A0,          PA3,         25);
        PIN_LR(layout, 17, A1,          PA2,         24);
        PIN_LR(layout, 18, A2,          PA1,         23);
        PIN_LR(layout, 19, A3,          PA0,         22);
        PIN_LR(layout, 20, A4,          A5,          21);

        return layout;
    }();
    return &layout;
}

// Helper: derive PIA6532 pin states from bus snapshot + chip internals
static std::vector<PinSignalState> get_pia6532_pin_states(
        pia6532_t* riot, const ChipLayout* layout, bus_state_t bus_state) {
    if (!riot || !layout) return {};

    // Generic bus-derived states (address, data, power, clock, R/W, /RES, /IRQ)
    auto ps = populate_pin_states_from_bus(*layout, bus_state);

    // --- RS (pin 4, idx 3) — RAM/IO select from address decoding ---
    ps[3].signal_level    = (BUS_GET_ADDR(bus_state) >> 9) & 1;
    ps[3].drive_direction = false;
    ps[3].high_impedance  = false;
    ps[3].signal_valid    = true;

    // --- /IRQ (pin 7, idx 6) — RIOT drives this when timer interrupt fires ---
    bool irq_asserted = riot->timer_underflow && riot->timer_interrupt_enabled;
    ps[6].signal_level    = !irq_asserted; // active low
    ps[6].drive_direction = true;
    ps[6].high_impedance  = !irq_asserted; // open-drain: hi-Z when not asserted
    ps[6].signal_valid    = true;

    // --- PA0-PA7 (pins 22-29, indices 21-28) — Port A I/O ---
    for (int i = 0; i < 8; i++) {
        bool is_output = (riot->port_a_ddr >> i) & 1;
        bool out_val   = (riot->port_a_data >> i) & 1;
        bool in_val    = (riot->port_a_input >> i) & 1;
        ps[21 + i].signal_level    = is_output ? out_val : in_val;
        ps[21 + i].drive_direction = is_output;
        ps[21 + i].high_impedance  = false;
        ps[21 + i].signal_valid    = true;
    }

    // --- PB0-PB7 (pins 30-37, indices 29-36) — Port B I/O ---
    for (int i = 0; i < 8; i++) {
        bool is_output = (riot->port_b_ddr >> i) & 1;
        bool out_val   = (riot->port_b_data >> i) & 1;
        bool in_val    = (riot->port_b_input >> i) & 1;
        ps[29 + i].signal_level    = is_output ? out_val : in_val;
        ps[29 + i].drive_direction = is_output;
        ps[29 + i].high_impedance  = false;
        ps[29 + i].signal_valid    = true;
    }

    // --- CS1 (pin 38, idx 37) — chip select ---
    ps[37].signal_level    = true;
    ps[37].drive_direction = false;
    ps[37].high_impedance  = false;
    ps[37].signal_valid    = true;

    // --- /CS2 (pin 39, idx 38) — chip select (active low) ---
    ps[38].signal_level    = false; // asserted (selected) by default
    ps[38].drive_direction = false;
    ps[38].high_impedance  = false;
    ps[38].signal_valid    = true;

    return ps;
}

std::vector<PinSignalState> pia6532_t::get_layout_pin_states(ChipLayout& layout) {
    return get_pia6532_pin_states(this, &layout, bus_snapshot_);
}

#endif // CERMU_HAS_GUI
