/*
 * cd4021_gui.cpp — CD4021 Shift Register Debug/Layout GUI
 *
 * 16-pin DIP pinout based on the TI CD4021B datasheet (SCHS054E).
 * The CD4021 is a CMOS 8-stage static shift register used as a
 * parallel-in/serial-out (PISO) interface in NES/SNES controllers.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "cd4021.h"
#include "../../core/chip_layout.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif

#ifdef CERMU_HAS_GUI

static ChipLayout create_cd4021_layout() {
    ChipLayout layout = create_dip16_layout();

    layout.markings.part_number  = "CD4021B";
    layout.markings.manufacturer = "Texas Instruments";
    layout.markings.custom_text  = "CMOS Shift Register";

    //                  LEFT                        RIGHT
    PIN_LR(layout,  1, P0,         VDD,         16);
    PIN_LR(layout,  2, Q6,         _Q7,         15);  // stage 6 out / Q̅7 complement
    PIN_LR(layout,  3, P4,         DS,          14);  // serial data in
    PIN_LR(layout,  4, P3,         P5,          13);
    PIN_LR(layout,  5, P6,         P7,          12);
    PIN_LR(layout,  6, P2,         Q7,          11);  // serial out
    PIN_LR(layout,  7, P1,         CLK,         10);
    PIN_LR(layout,  8, VSS,        P_S,          9);  // P/S̅ (latch control)

    return layout;
}

// Helper: derive CD4021 pin states from shift register internals.
// The CD4021 is not on the main system bus — it lives in the controller.
// bus_state_t is unused; all signals come from the shift register state.
static std::vector<PinSignalState> get_cd4021_pin_states(
        CD4021* chip, const ChipLayout* layout) {
    if (!chip || !layout) return {};

    // Start with generic handler (sets POWER, CLOCK, NC correctly)
    auto ps = populate_pin_states_from_bus(*layout, 0);

    uint8_t sr = chip->get_shift_register();

    // --- Parallel inputs P0-P7 (directly from shift register latched state) ---
    // P0 (pin 1, idx 0)
    ps[0].signal_level = (sr >> 0) & 1; ps[0].drive_direction = false;
    ps[0].high_impedance = false;       ps[0].signal_valid = true;
    // P4 (pin 3, idx 2)
    ps[2].signal_level = (sr >> 4) & 1; ps[2].drive_direction = false;
    ps[2].high_impedance = false;       ps[2].signal_valid = true;
    // P3 (pin 4, idx 3)
    ps[3].signal_level = (sr >> 3) & 1; ps[3].drive_direction = false;
    ps[3].high_impedance = false;       ps[3].signal_valid = true;
    // P6 (pin 5, idx 4)
    ps[4].signal_level = (sr >> 6) & 1; ps[4].drive_direction = false;
    ps[4].high_impedance = false;       ps[4].signal_valid = true;
    // P2 (pin 6, idx 5)
    ps[5].signal_level = (sr >> 2) & 1; ps[5].drive_direction = false;
    ps[5].high_impedance = false;       ps[5].signal_valid = true;
    // P1 (pin 7, idx 6)
    ps[6].signal_level = (sr >> 1) & 1; ps[6].drive_direction = false;
    ps[6].high_impedance = false;       ps[6].signal_valid = true;
    // P5 (pin 13, idx 11)
    ps[11].signal_level = (sr >> 5) & 1; ps[11].drive_direction = false;
    ps[11].high_impedance = false;       ps[11].signal_valid = true;
    // P7 (pin 12, idx 12)
    ps[12].signal_level = (sr >> 7) & 1; ps[12].drive_direction = false;
    ps[12].high_impedance = false;       ps[12].signal_valid = true;

    // --- Q7 serial output (pin 11, idx 13) — MSB of shift register ---
    ps[13].signal_level    = (sr >> 7) & 1;
    ps[13].drive_direction = true;
    ps[13].high_impedance  = false;
    ps[13].signal_valid    = true;

    // --- /Q7 complement output (pin 15, idx 9) ---
    ps[9].signal_level    = !((sr >> 7) & 1);
    ps[9].drive_direction = true;
    ps[9].high_impedance  = false;
    ps[9].signal_valid    = true;

    // --- Q6 stage 6 output (pin 2, idx 1) ---
    ps[1].signal_level    = (sr >> 6) & 1;
    ps[1].drive_direction = true;
    ps[1].high_impedance  = false;
    ps[1].signal_valid    = true;

    // --- DS serial data input (pin 14, idx 10) — grounded in NES ---
    ps[10].signal_level    = false;
    ps[10].drive_direction = false;
    ps[10].high_impedance  = false;
    ps[10].signal_valid    = true;

    // --- P/S latch control (pin 9, idx 15) — input ---
    ps[15].signal_level    = false; // LOW = serial mode (default readout state)
    ps[15].drive_direction = false;
    ps[15].high_impedance  = false;
    ps[15].signal_valid    = true;

    return ps;
}

#endif // CERMU_HAS_GUI (layout/pin helpers)

// ============================================================================
// ChipBase layout virtuals
// ============================================================================

#ifdef CERMU_HAS_GUI

ChipLayout* CD4021::create_chip_layout() const {
    static ChipLayout layout = create_cd4021_layout();
    return &layout;
}

std::vector<PinSignalState> CD4021::get_layout_pin_states(ChipLayout& layout) {
    return get_cd4021_pin_states(this, &layout);
}

#endif // CERMU_HAS_GUI
