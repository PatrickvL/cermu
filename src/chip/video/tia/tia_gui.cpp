/*
 * tia_gui.cpp — Atari TIA (CO10444) Debug/Layout GUI
 *
 * 40-pin DIP pinout based on the Atari CO10444 (TIA) datasheet.
 * The TIA generates video, audio, and handles player input for the
 * Atari 2600 VCS.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "tia.h"
#include "../../core/chip_layout.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif

// ============================================================================
// 40-pin DIP Layout — Atari TIA (CO10444)
// ============================================================================
//
//   Pin  1: VSS           Pin 40: Vtia
//   Pin  2: COLOR CLK     Pin 39: Φ2
//   Pin  3: /CS1          Pin 38: RDY
//   Pin  4: CS0           Pin 37: DUMP
//   Pin  5: CS3           Pin 36: INPT0
//   Pin  6: R/W           Pin 35: INPT1
//   Pin  7: Φ0            Pin 34: INPT2
//   Pin  8: D0            Pin 33: INPT3
//   Pin  9: D1            Pin 32: INPT4
//   Pin 10: D2            Pin 31: INPT5
//   Pin 11: D3            Pin 30: OSC IN
//   Pin 12: D4            Pin 29: OSC OUT
//   Pin 13: D5            Pin 28: A0
//   Pin 14: D6            Pin 27: A1
//   Pin 15: D7            Pin 26: A2
//   Pin 16: AUD0          Pin 25: A3
//   Pin 17: AUD1          Pin 24: A4
//   Pin 18: COLU          Pin 23: A5
//   Pin 19: LUMA          Pin 22: VCC
//   Pin 20: BLK           Pin 21: CSYNC
//

#ifdef CERMU_HAS_GUI

static ChipLayout create_tia_layout() {
    ChipLayout layout = create_dip40_layout();

    layout.markings.part_number  = "CO10444";
    layout.markings.manufacturer = "Atari";
    layout.markings.custom_text  = "TIA";

    //                  LEFT                         RIGHT
    PIN_LR(layout,  1, VSS,         VTIA,        40);
    PIN_LR(layout,  2, COLOR_CLK,   PHI2,        39);
    PIN_LR(layout,  3, _CS1,        RDY,         38);
    PIN_LR(layout,  4, CS0,         DUMP,        37);
    PIN_LR(layout,  5, CS3,         INPT0,       36);
    PIN_LR(layout,  6, RW,          INPT1,       35);
    PIN_LR(layout,  7, PHI0,        INPT2,       34);
    PIN_LR(layout,  8, D0,          INPT3,       33);
    PIN_LR(layout,  9, D1,          INPT4,       32);
    PIN_LR(layout, 10, D2,          INPT5,       31);
    PIN_LR(layout, 11, D3,          OSC_IN,      30);
    PIN_LR(layout, 12, D4,          OSC_OUT,     29);
    PIN_LR(layout, 13, D5,          A0,          28);
    PIN_LR(layout, 14, D6,          A1,          27);
    PIN_LR(layout, 15, D7,          A2,          26);
    PIN_LR(layout, 16, AUD0,        A3,          25);
    PIN_LR(layout, 17, AUD1,        A4,          24);
    PIN_LR(layout, 18, COLU,        A5,          23);
    PIN_LR(layout, 19, LUMA,        VCC,         22);
    PIN_LR(layout, 20, COMP_BLK,    CSYNC,       21);

    return layout;
}

static ChipLayout& get_tia_layout() {
    static ChipLayout layout = create_tia_layout();
    return layout;
}

// Helper: derive TIA pin states from bus snapshot + chip internals
static std::vector<PinSignalState> get_tia_pin_states(
        tia_t* tia, const ChipLayout* layout, bus_state_t bus_state) {
    if (!tia || !layout) return {};

    // Generic bus-derived states (address, data, power, clock, R/W, RDY)
    auto ps = populate_pin_states_from_bus(*layout, bus_state);

    // --- Chip select lines (directly from bus address decoding) ---
    // _CS1 (pin 3, idx 2) — active low
    ps[2].signal_level    = true;  // default: deselected
    ps[2].drive_direction = false; // input to TIA
    ps[2].high_impedance  = false;
    ps[2].signal_valid    = true;
    // CS0 (pin 4, idx 3)
    ps[3].signal_level    = false;
    ps[3].drive_direction = false;
    ps[3].high_impedance  = false;
    ps[3].signal_valid    = true;
    // CS3 (pin 5, idx 4)
    ps[4].signal_level    = false;
    ps[4].drive_direction = false;
    ps[4].high_impedance  = false;
    ps[4].signal_valid    = true;

    // --- RDY (pin 38, idx 37) — TIA drives this LOW during WSYNC ---
    ps[37].signal_level    = !tia->wsync_pending; // low = halted
    ps[37].drive_direction = true;                 // TIA drives RDY
    ps[37].high_impedance  = false;
    ps[37].signal_valid    = true;

    // --- Audio outputs (pin 16 = AUD0, pin 17 = AUD1) ---
    ps[15].signal_level    = tia->audio[0].output;
    ps[15].drive_direction = true;
    ps[15].high_impedance  = false;
    ps[15].signal_valid    = true;
    ps[15].is_pwm          = true;
    ps[15].pwm_duty_cycle  = tia->audio[0].volume / 15.0f;

    ps[16].signal_level    = tia->audio[1].output;
    ps[16].drive_direction = true;
    ps[16].high_impedance  = false;
    ps[16].signal_valid    = true;
    ps[16].is_pwm          = true;
    ps[16].pwm_duty_cycle  = tia->audio[1].volume / 15.0f;

    // --- Video outputs ---
    // COLU (pin 18, idx 17) — color/luminance
    ps[17].signal_level    = true;
    ps[17].drive_direction = true;
    ps[17].high_impedance  = false;
    ps[17].signal_valid    = true;
    ps[17].signal_value    = tia->colubk;

    // LUMA (pin 19, idx 18)
    ps[18].signal_level    = true;
    ps[18].drive_direction = true;
    ps[18].high_impedance  = false;
    ps[18].signal_valid    = true;

    // COMP_BLK (pin 20, idx 19) — blanking during VBLANK/HBLANK
    bool blanking = tia->vblank_active || (tia->h_counter < 68);
    ps[19].signal_level    = blanking;
    ps[19].drive_direction = true;
    ps[19].high_impedance  = false;
    ps[19].signal_valid    = true;

    // CSYNC (pin 21, idx 20) — composite sync
    bool hsync = (tia->h_counter >= 4 && tia->h_counter < 8);
    ps[20].signal_level    = tia->vsync_active || hsync;
    ps[20].drive_direction = true;
    ps[20].high_impedance  = false;
    ps[20].signal_valid    = true;

    // --- Input ports (active-high readback) ---
    // INPT0 (pin 36, idx 35) through INPT5 (pin 31, idx 30)
    // Pin numbering: pin 36=INPT0 .. pin 31=INPT5 → indices 35..30
    ps[35].signal_level = tia->inpt0; ps[35].drive_direction = false;
    ps[35].high_impedance = false;    ps[35].signal_valid = true;
    ps[34].signal_level = tia->inpt1; ps[34].drive_direction = false;
    ps[34].high_impedance = false;    ps[34].signal_valid = true;
    ps[33].signal_level = tia->inpt2; ps[33].drive_direction = false;
    ps[33].high_impedance = false;    ps[33].signal_valid = true;
    ps[32].signal_level = tia->inpt3; ps[32].drive_direction = false;
    ps[32].high_impedance = false;    ps[32].signal_valid = true;
    ps[31].signal_level = tia->inpt4; ps[31].drive_direction = false;
    ps[31].high_impedance = false;    ps[31].signal_valid = true;
    ps[30].signal_level = tia->inpt5; ps[30].drive_direction = false;
    ps[30].high_impedance = false;    ps[30].signal_valid = true;

    // --- DUMP (pin 37, idx 36) — paddle discharge control ---
    ps[36].signal_level    = tia->vblank_active; // DUMP is gated by VBLANK bit 7
    ps[36].drive_direction = true;
    ps[36].high_impedance  = false;
    ps[36].signal_valid    = true;

    // --- VTIA (pin 40, idx 39) — analog supply, always driven ---
    ps[39].signal_level    = true;
    ps[39].drive_direction = true;
    ps[39].high_impedance  = false;
    ps[39].signal_valid    = true;
    ps[39].analog_voltage  = 5.0f;

    return ps;
}

#endif // CERMU_HAS_GUI (layout/pin helpers)

// ============================================================================
// ChipBase layout virtuals
// ============================================================================

#ifdef CERMU_HAS_GUI

ChipLayout* tia_t::get_chip_layout() const {
    return &get_tia_layout();
}

std::vector<PinSignalState> tia_t::get_layout_pin_states(ChipLayout& layout) {
    return get_tia_pin_states(this, &layout, bus_snapshot_);
}

#endif // CERMU_HAS_GUI
