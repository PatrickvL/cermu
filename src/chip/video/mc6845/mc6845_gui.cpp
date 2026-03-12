/*
 * mc6845_gui.cpp — Motorola MC6845 CRT Controller Debug/Layout GUI
 *
 * 40-pin DIP pinout based on the Motorola MC6845 datasheet.
 * The MC6845 generates timing and addressing for raster-scanned CRT
 * displays.  Used in Commodore PET/CBM, BBC Micro, Amstrad CPC, etc.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/video/mc6845/mc6845.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

// ============================================================================
// 40-pin DIP Layout — Motorola MC6845
// ============================================================================
//
//   Pin  1: VSS           Pin 40: VCC
//   Pin  2: /RES          Pin 39: CLK
//   Pin  3: LPSTB         Pin 38: /CS
//   Pin  4: MA0           Pin 37: RS
//   Pin  5: MA1           Pin 36: E (Enable)
//   Pin  6: MA2           Pin 35: R/W
//   Pin  7: MA3           Pin 34: VSYNC
//   Pin  8: MA4           Pin 33: HSYNC
//   Pin  9: MA5           Pin 32: DE
//   Pin 10: MA6           Pin 31: CURSOR
//   Pin 11: MA7           Pin 30: D7
//   Pin 12: MA8           Pin 29: D6
//   Pin 13: MA9           Pin 28: D5
//   Pin 14: MA10          Pin 27: D4
//   Pin 15: MA11          Pin 26: D3
//   Pin 16: MA12          Pin 25: D2
//   Pin 17: MA13          Pin 24: D1
//   Pin 18: RA0           Pin 23: D0
//   Pin 19: RA1           Pin 22: RA4
//   Pin 20: RA2           Pin 21: RA3
//

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase layout virtuals
// ============================================================================

ChipLayout* mc6845_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();

        layout.markings.part_number  = "MC6845";
        layout.markings.manufacturer = "Motorola";
        layout.markings.custom_text  = "CRT Controller";

        //                  LEFT                         RIGHT
        PIN_LR(layout,  1, VSS,         VCC,         40);
        PIN_LR(layout,  2, _RES,        CLK,         39);
        PIN_LR(layout,  3, LPSTB,       _CS,         38);
        PIN_LR(layout,  4, MA0,         RS,          37);
        PIN_LR(layout,  5, MA1,         ENABLE,      36);
        PIN_LR(layout,  6, MA2,         RW,          35);
        PIN_LR(layout,  7, MA3,         VSYNC,       34);
        PIN_LR(layout,  8, MA4,         HSYNC,       33);
        PIN_LR(layout,  9, MA5,         DE,          32);
        PIN_LR(layout, 10, MA6,         CURSOR,      31);
        PIN_LR(layout, 11, MA7,         D7,          30);
        PIN_LR(layout, 12, MA8,         D6,          29);
        PIN_LR(layout, 13, MA9,         D5,          28);
        PIN_LR(layout, 14, MA10,        D4,          27);
        PIN_LR(layout, 15, MA11,        D3,          26);
        PIN_LR(layout, 16, MA12,        D2,          25);
        PIN_LR(layout, 17, MA13,        D1,          24);
        PIN_LR(layout, 18, RA0,         D0,          23);
        PIN_LR(layout, 19, RA1,         RA4,         22);
        PIN_LR(layout, 20, RA2,         RA3,         21);

        return layout;
    }();
    return &layout;
}

// Helper: derive MC6845 pin states from bus snapshot + chip internals
static std::vector<PinSignalState> get_mc6845_pin_states(
        mc6845_t* crtc, const ChipLayout* layout, bus_state_t bus_state) {
    if (!crtc || !layout) return {};

    // Generic bus-derived states (data bus, power, clock, R/W, /RES)
    auto ps = populate_pin_states_from_bus(*layout, bus_state);

    // --- LPSTB (pin 3, idx 2) — light pen strobe, input ---
    ps[2].signal_level    = crtc->light_pen_latched;
    ps[2].drive_direction = false;
    ps[2].high_impedance  = false;
    ps[2].signal_valid    = true;

    // --- MA0-MA13 (pins 4-17, indices 3-16) — CRTC-generated output addresses ---
    // These are OUTPUT address lines from the CRTC, NOT bus address inputs.
    // Override the generic bus-address handler with the actual linear_address.
    for (int i = 0; i < 14; i++) {
        ps[3 + i].signal_level    = (crtc->linear_address >> i) & 1;
        ps[3 + i].drive_direction = true;  // output
        ps[3 + i].high_impedance  = false;
        ps[3 + i].signal_valid    = true;
    }

    // --- RA0-RA4 (pins 18-22, indices 17-21) ---
    // Row address outputs from scanline counter.
    // DIP-40: pin N → index N-1, so RA0=pin18→idx17 .. RA4=pin22→idx21.
    for (int i = 0; i < 5; i++) {
        int idx = 17 + i;
        ps[idx].signal_level    = (crtc->v_scanline_counter >> i) & 1;
        ps[idx].drive_direction = true;
        ps[idx].high_impedance  = false;
        ps[idx].signal_valid    = true;
    }

    // --- CURSOR (pin 31, idx 30) — cursor output ---
    ps[30].signal_level    = crtc->cursor_visible;
    ps[30].drive_direction = true;
    ps[30].high_impedance  = false;
    ps[30].signal_valid    = true;

    // --- DE (pin 32, idx 31) — display enable output ---
    ps[31].signal_level    = crtc->h_display_active && crtc->v_display_active;
    ps[31].drive_direction = true;
    ps[31].high_impedance  = false;
    ps[31].signal_valid    = true;

    // --- HSYNC (pin 33, idx 32) — horizontal sync output ---
    ps[32].signal_level    = crtc->h_sync_active;
    ps[32].drive_direction = true;
    ps[32].high_impedance  = false;
    ps[32].signal_valid    = true;

    // --- VSYNC (pin 34, idx 33) — vertical sync output ---
    ps[33].signal_level    = crtc->v_sync_active;
    ps[33].drive_direction = true;
    ps[33].high_impedance  = false;
    ps[33].signal_valid    = true;

    // --- ENABLE (pin 36, idx 35) — clock enable input ---
    ps[35].signal_level    = true; // typically connected to system clock
    ps[35].drive_direction = false;
    ps[35].high_impedance  = false;
    ps[35].signal_valid    = true;

    // --- RS (pin 37, idx 36) — register select input ---
    ps[36].signal_level    = BUS_GET_ADDR(bus_state) & 1;
    ps[36].drive_direction = false;
    ps[36].high_impedance  = false;
    ps[36].signal_valid    = true;

    // --- /CS (pin 38, idx 37) — chip select (active low) input ---
    ps[37].signal_level    = true;  // default: deselected
    ps[37].drive_direction = false;
    ps[37].high_impedance  = false;
    ps[37].signal_valid    = true;

    return ps;
}

std::vector<PinSignalState> mc6845_t::get_layout_pin_states(ChipLayout& layout) {
    return get_mc6845_pin_states(this, &layout, bus_snapshot_);
}

#endif // CERMU_HAS_GUI
