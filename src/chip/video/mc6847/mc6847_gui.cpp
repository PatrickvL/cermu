/*
 * mc6847_gui.cpp — Motorola MC6847 VDG Debug/Layout GUI
 *
 * 40-pin DIP pinout based on the Motorola MC6847 datasheet.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/video/mc6847/mc6847.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase layout virtuals
// ============================================================================

ChipLayout* mc6847_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();

        // MC6847 40-pin DIP pinout (Motorola datasheet)
        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, D6,         VCC,         40);
        PIN_LR(layout,  2, D5,         D7,          39);
        PIN_LR(layout,  3, D4,         AG,          38);  // Alpha/Graphics
        PIN_LR(layout,  4, D3,         AS,          37);  // Alpha/Semigraphics
        PIN_LR(layout,  5, D2,         INTEXT,      36);  // Internal/External
        PIN_LR(layout,  6, D1,         INV,         35);  // Invert
        PIN_LR(layout,  7, D0,         GM2,         34);  // Graphics Mode 2
        PIN_LR(layout,  8, A0,         GM1,         33);  // Graphics Mode 1
        PIN_LR(layout,  9, A1,         GM0,         32);  // Graphics Mode 0
        PIN_LR(layout, 10, A2,         CSS,         31);  // Color Set Select
        PIN_LR(layout, 11, A3,         VOUT,        30);  // Composite video out
        PIN_LR(layout, 12, A4,         CHROMA,      29);  // Chroma output
        PIN_LR(layout, 13, A5,         LUMA,        28);  // Luminance output
        PIN_LR(layout, 14, A6,         CLK,         27);  // Clock input
        PIN_LR(layout, 15, A7,         _RES,        26);  // /RESET
        PIN_LR(layout, 16, A8,         FS,          25);  // Field sync output
        PIN_LR(layout, 17, A9,         HSYNC,       24);  // Horiz sync output
        PIN_LR(layout, 18, A10,        RW,          23);  // DA/R̅W̅ (DMA address/R/W)
        PIN_LR(layout, 19, A11,        A12,         22);
        PIN_LR(layout, 20, VSS,        NC,          21);  // VSS / MS (mode select)

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> mc6847_t::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, 0);

    // Mode input pins — reflect current mode settings
    // AG (pin 38, idx 37)
    ps[37].signal_level = mode_ag_; ps[37].signal_valid = true;
    // AS (pin 37, idx 36)
    ps[36].signal_level = mode_as_; ps[36].signal_valid = true;
    // INTEXT (pin 36, idx 35)
    ps[35].signal_level = mode_intext_; ps[35].signal_valid = true;
    // INV (pin 35, idx 34)
    ps[34].signal_level = mode_inv_; ps[34].signal_valid = true;
    // GM2 (pin 34, idx 33)
    ps[33].signal_level = (mode_gm_ >> 2) & 1; ps[33].signal_valid = true;
    // GM1 (pin 33, idx 32)
    ps[32].signal_level = (mode_gm_ >> 1) & 1; ps[32].signal_valid = true;
    // GM0 (pin 32, idx 31)
    ps[31].signal_level = (mode_gm_ >> 0) & 1; ps[31].signal_valid = true;
    // CSS (pin 31, idx 30)
    ps[30].signal_level = mode_css_; ps[30].signal_valid = true;

    // Sync outputs
    // FS (pin 25, idx 24)
    ps[24].signal_level    = fs_;
    ps[24].drive_direction = true;
    ps[24].signal_valid    = true;
    // HS (pin 24, idx 23)
    ps[23].signal_level    = hs_;
    ps[23].drive_direction = true;
    ps[23].signal_valid    = true;

    return ps;
}

#endif // CERMU_HAS_GUI
