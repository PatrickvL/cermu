/*
 * crtc_gui.cpp — MC6845/VDC family chip layout and debug GUI
 *
 * Supports two package formats:
 *   - 40-pin DIP: MC6845, R6545, HD6845, UM6845, EF6845, MOS 6545
 *   - 48-pin DIP: MOS 8563, MOS 8568
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/video/fam6845/crtc_common.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

// ============================================================================
// 40-pin DIP Layout — MC6845 / R6545 / HD6845 / UM6845 / EF6845 / MOS 6545
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

// ============================================================================
// 48-pin DIP Layout — MOS 8563 / MOS 8568 VDC
// ============================================================================
//
//   Pin  1: VSS           Pin 48: VCC (DRAM)
//   Pin  2: /RAS          Pin 47: VCC
//   Pin  3: /CAS          Pin 46: D7 (DRAM)
//   Pin  4: /WE           Pin 45: D6 (DRAM)
//   Pin  5: MA0           Pin 44: D5 (DRAM)
//   Pin  6: MA1           Pin 43: D4 (DRAM)
//   Pin  7: MA2           Pin 42: D3 (DRAM)
//   Pin  8: MA3           Pin 41: D2 (DRAM)
//   Pin  9: MA4           Pin 40: D1 (DRAM)
//   Pin 10: MA5           Pin 39: CLK (16 MHz)
//   Pin 11: MA6           Pin 38: D0 (DRAM)
//   Pin 12: MA7           Pin 37: /OE
//   Pin 13: MA8           Pin 36: /RES
//   Pin 14: MA9           Pin 35: /CS
//   Pin 15: MA10          Pin 34: DB7
//   Pin 16: MA11          Pin 33: DB6
//   Pin 17: MA12          Pin 32: DB5
//   Pin 18: VSS (DRAM)    Pin 31: DB4
//   Pin 19: /INTR         Pin 30: DB3
//   Pin 20: LPEN          Pin 29: DB2
//   Pin 21: /INTR         Pin 28: DB1
//   Pin 22: LPEN          Pin 27: DB0
//   Pin 23: CURSOR        Pin 26: RS
//   Pin 24: VIDEO         Pin 25: VSYNC
//                  HSYNC = pin between 25 and 26 (actually pin 26 on datasheet)
//                  (exact pinout per MOS 8563 datasheet)

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase layout virtuals
// ============================================================================

ChipLayout* crtc_base_t::create_chip_layout() const {
    if (!traits_) {
        // Fallback: 40-pin DIP
        static ChipLayout layout40 = [] {
            ChipLayout l = create_dip40_layout();
            l.markings.custom_text = "CRT Controller";
            PIN_LR(l,  1, VSS,         VCC,         40);
            PIN_LR(l,  2, _RES,        CLK,         39);
            PIN_LR(l,  3, LPSTB,       _CS,         38);
            PIN_LR(l,  4, MA0,         RS,          37);
            PIN_LR(l,  5, MA1,         ENABLE,      36);
            PIN_LR(l,  6, MA2,         RW,          35);
            PIN_LR(l,  7, MA3,         VSYNC,       34);
            PIN_LR(l,  8, MA4,         HSYNC,       33);
            PIN_LR(l,  9, MA5,         DE,          32);
            PIN_LR(l, 10, MA6,         CURSOR,      31);
            PIN_LR(l, 11, MA7,         D7,          30);
            PIN_LR(l, 12, MA8,         D6,          29);
            PIN_LR(l, 13, MA9,         D5,          28);
            PIN_LR(l, 14, MA10,        D4,          27);
            PIN_LR(l, 15, MA11,        D3,          26);
            PIN_LR(l, 16, MA12,        D2,          25);
            PIN_LR(l, 17, MA13,        D1,          24);
            PIN_LR(l, 18, RA0,         D0,          23);
            PIN_LR(l, 19, RA1,         RA4,         22);
            PIN_LR(l, 20, RA2,         RA3,         21);
            return l;
        }();
        return &layout40;
    }

    if (traits_->pin_count == 48) {
        // 48-pin DIP: MOS 8563 / MOS 8568 VDC
        static ChipLayout layout48 = [] {
            ChipLayout l = {};
            l.package = {
                600.0f,                      // width (mil) - DIP48 wide body
                2400.0f,                     // height (mil) - DIP48 body length
                PackageType::DIP,
                OrientationMarker::NOTCH,
                100.0f,                      // pin_pitch (mil)
                false, false, 0.0f
            };
            l.markings.custom_text = "Video Display Controller";

            //                  LEFT                         RIGHT
            // MOS 8563/8568 VDC — 48-pin DIP
            //   DB0-DB7: CPU data bus      DD0-DD7: DRAM data bus
            //   MA0-MA12: DRAM row/col addr
            PIN_LR(l,  1, VSS,         VCC_DRAM,    48);
            PIN_LR(l,  2, _RAS,        VCC,         47);
            PIN_LR(l,  3, _CAS,        DD7,         46);
            PIN_LR(l,  4, _WE,         DD6,         45);
            PIN_LR(l,  5, MA0,         DD5,         44);
            PIN_LR(l,  6, MA1,         DD4,         43);
            PIN_LR(l,  7, MA2,         DD3,         42);
            PIN_LR(l,  8, MA3,         DD2,         41);
            PIN_LR(l,  9, MA4,         DD1,         40);
            PIN_LR(l, 10, MA5,         CLK,         39);
            PIN_LR(l, 11, MA6,         DD0,         38);
            PIN_LR(l, 12, MA7,         _OE,         37);
            PIN_LR(l, 13, MA8,         _RES,        36);
            PIN_LR(l, 14, MA9,         _CS,         35);
            PIN_LR(l, 15, MA10,        DB7,         34);
            PIN_LR(l, 16, MA11,        DB6,         33);
            PIN_LR(l, 17, MA12,        DB5,         32);
            PIN_LR(l, 18, VSS_DRAM,    DB4,         31);
            PIN_LR(l, 19, _INTR,       DB3,         30);
            PIN_LR(l, 20, LPEN,        DB2,         29);
            PIN_LR(l, 21, DE,          DB1,         28);
            PIN_LR(l, 22, DRDY,        DB0,         27);
            PIN_LR(l, 23, CURSOR,      RS,          26);
            PIN_LR(l, 24, RGBI,        VSYNC,       25);
            return l;
        }();
        return &layout48;
    }

    // Default: 40-pin DIP (same as fallback above)
    static ChipLayout layout40_default = [] {
        ChipLayout l = create_dip40_layout();
        l.markings.custom_text = "CRT Controller";
        PIN_LR(l,  1, VSS,         VCC,         40);
        PIN_LR(l,  2, _RES,        CLK,         39);
        PIN_LR(l,  3, LPSTB,       _CS,         38);
        PIN_LR(l,  4, MA0,         RS,          37);
        PIN_LR(l,  5, MA1,         ENABLE,      36);
        PIN_LR(l,  6, MA2,         RW,          35);
        PIN_LR(l,  7, MA3,         VSYNC,       34);
        PIN_LR(l,  8, MA4,         HSYNC,       33);
        PIN_LR(l,  9, MA5,         DE,          32);
        PIN_LR(l, 10, MA6,         CURSOR,      31);
        PIN_LR(l, 11, MA7,         D7,          30);
        PIN_LR(l, 12, MA8,         D6,          29);
        PIN_LR(l, 13, MA9,         D5,          28);
        PIN_LR(l, 14, MA10,        D4,          27);
        PIN_LR(l, 15, MA11,        D3,          26);
        PIN_LR(l, 16, MA12,        D2,          25);
        PIN_LR(l, 17, MA13,        D1,          24);
        PIN_LR(l, 18, RA0,         D0,          23);
        PIN_LR(l, 19, RA1,         RA4,         22);
        PIN_LR(l, 20, RA2,         RA3,         21);
        return l;
    }();
    return &layout40_default;
}

// ============================================================================
// Pin state population
// ============================================================================

static std::vector<PinSignalState> get_crtc_pin_states_40(
        crtc_base_t* crtc, const ChipLayout* layout, bus_state_t bus_state) {
    if (!crtc || !layout) return {};

    auto ps = populate_pin_states_from_bus(*layout, bus_state);

    // LPSTB (pin 3, idx 2) — light pen strobe
    ps[2].signal_level    = crtc->light_pen_latched;
    ps[2].drive_direction = false;
    ps[2].high_impedance  = false;
    ps[2].signal_valid    = true;

    // MA0-MA13 (pins 4-17, indices 3-16) — output addresses
    for (int i = 0; i < 14; i++) {
        ps[3 + i].signal_level    = (crtc->linear_address >> i) & 1;
        ps[3 + i].drive_direction = true;
        ps[3 + i].high_impedance  = false;
        ps[3 + i].signal_valid    = true;
    }

    // RA0-RA4 (pins 18-22, indices 17-21)
    for (int i = 0; i < 5; i++) {
        int idx = 17 + i;
        ps[idx].signal_level    = (crtc->v_scanline_counter >> i) & 1;
        ps[idx].drive_direction = true;
        ps[idx].high_impedance  = false;
        ps[idx].signal_valid    = true;
    }

    // CURSOR (pin 31, idx 30)
    ps[30].signal_level    = crtc->cursor_visible;
    ps[30].drive_direction = true;
    ps[30].high_impedance  = false;
    ps[30].signal_valid    = true;

    // DE (pin 32, idx 31)
    ps[31].signal_level    = crtc->h_display_active && crtc->v_display_active;
    ps[31].drive_direction = true;
    ps[31].high_impedance  = false;
    ps[31].signal_valid    = true;

    // HSYNC (pin 33, idx 32)
    ps[32].signal_level    = crtc->h_sync_active;
    ps[32].drive_direction = true;
    ps[32].high_impedance  = false;
    ps[32].signal_valid    = true;

    // VSYNC (pin 34, idx 33)
    ps[33].signal_level    = crtc->v_sync_active;
    ps[33].drive_direction = true;
    ps[33].high_impedance  = false;
    ps[33].signal_valid    = true;

    // ENABLE (pin 36, idx 35)
    ps[35].signal_level    = true;
    ps[35].drive_direction = false;
    ps[35].high_impedance  = false;
    ps[35].signal_valid    = true;

    // RS (pin 37, idx 36)
    ps[36].signal_level    = BUS_GET_ADDR(bus_state) & 1;
    ps[36].drive_direction = false;
    ps[36].high_impedance  = false;
    ps[36].signal_valid    = true;

    // /CS (pin 38, idx 37)
    ps[37].signal_level    = true;
    ps[37].drive_direction = false;
    ps[37].high_impedance  = false;
    ps[37].signal_valid    = true;

    return ps;
}

static std::vector<PinSignalState> get_vdc_pin_states_48(
        crtc_base_t* vdc, const ChipLayout* layout, bus_state_t bus_state) {
    if (!vdc || !layout) return {};

    auto ps = populate_pin_states_from_bus(*layout, bus_state);

    // MA0-MA12 (pins 5-17, indices 4-16) — DRAM addresses
    for (int i = 0; i < 13; i++) {
        ps[4 + i].signal_level    = (vdc->linear_address >> i) & 1;
        ps[4 + i].drive_direction = true;
        ps[4 + i].high_impedance  = false;
        ps[4 + i].signal_valid    = true;
    }

    // CURSOR (pin 23, idx 22)
    ps[22].signal_level    = vdc->cursor_visible && vdc->is_display_active();
    ps[22].drive_direction = true;
    ps[22].high_impedance  = false;
    ps[22].signal_valid    = true;

    // VIDEO (pin 24, idx 23) — analog luma (represent as on/off)
    ps[23].signal_level    = vdc->is_display_active();
    ps[23].drive_direction = true;
    ps[23].high_impedance  = false;
    ps[23].signal_valid    = true;

    // VSYNC (pin 25, idx 24)
    ps[24].signal_level    = vdc->v_sync_active;
    ps[24].drive_direction = true;
    ps[24].high_impedance  = false;
    ps[24].signal_valid    = true;

    // RS (pin 26, idx 25)
    ps[25].signal_level    = BUS_GET_ADDR(bus_state) & 1;
    ps[25].drive_direction = false;
    ps[25].high_impedance  = false;
    ps[25].signal_valid    = true;

    return ps;
}

std::vector<PinSignalState> crtc_base_t::get_layout_pin_states(ChipLayout& layout) {
    if (traits_ && traits_->pin_count == 48)
        return get_vdc_pin_states_48(this, &layout, bus_snapshot_);
    return get_crtc_pin_states_40(this, &layout, bus_snapshot_);
}

#endif // CERMU_HAS_GUI
