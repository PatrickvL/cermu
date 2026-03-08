/*
 * ferranti_ula_gui.cpp — Ferranti ULA (6C001E-7) Debug/Layout GUI
 *
 * 40-pin DIP pinout based on the ZX Spectrum ULA.
 * Pin assignments are approximate — the Ferranti ULA is a custom gate array
 * and full pinouts vary across ASIC revisions (6C001E-6, 6C001E-7, etc.).
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "ferranti_ula.h"
#include "../../../core/chip_layout.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../../gui/chip_visualization.h"
#include "../../../gui/global_chip_style.h"
#endif

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase layout virtuals
// ============================================================================

ChipLayout* ferranti_ula_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();

        layout.markings.part_number  = "6C001E-7";
        layout.markings.manufacturer = "Ferranti";
        layout.markings.custom_text  = "ZX Spectrum ULA";

        // Approximate Ferranti ULA 40-pin DIP pinout
        // (based on PCB trace analysis and community documentation)
        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, CLK,         VCC,         40);  // 14 MHz crystal
        PIN_LR(layout,  2, D7,          _INT,        39);  // CPU /INT
        PIN_LR(layout,  3, D6,          _MREQ,       38);
        PIN_LR(layout,  4, D5,          _IORQ,       37);
        PIN_LR(layout,  5, D4,          _RD,         36);
        PIN_LR(layout,  6, D3,          _WR,         35);
        PIN_LR(layout,  7, D2,          _CS,         34);  // /ROMCS
        PIN_LR(layout,  8, D1,          A15,         33);
        PIN_LR(layout,  9, D0,          A14,         32);
        PIN_LR(layout, 10, VSS,         A13,         31);
        PIN_LR(layout, 11, CAS,         A12,         30);
        PIN_LR(layout, 12, RAS,         A11,         29);
        PIN_LR(layout, 13, A0,          A10,         28);
        PIN_LR(layout, 14, A1,          A9,          27);
        PIN_LR(layout, 15, A2,          A8,          26);
        PIN_LR(layout, 16, A3,          A7,          25);
        PIN_LR(layout, 17, A4,          A6,          24);
        PIN_LR(layout, 18, A5,          VOUT,        23);  // Composite video
        PIN_LR(layout, 19, EAR,         SPEAKER,     22);  // Tape in / Speaker
        PIN_LR(layout, 20, MIC,         PHI2,        21);  // Tape out / CPU clock

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> ferranti_ula_t::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, 0);
    // ULA is not on a standard bus — return bus-derived defaults
    return ps;
}

#endif // CERMU_HAS_GUI
