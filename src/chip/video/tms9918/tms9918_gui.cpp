/*
 * tms9918_gui.cpp — TMS9918 VDP family chip layout
 *
 * 40-pin DIP pinout for TMS9918/A, TMS9928A, TMS9929/A.
 * V9938/V9958 and Sega variants use different packages but share
 * the same stub pin-state path for now.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 * Non-GUI builds compile this to empty stubs (explicit instantiation only).
 */

// All variant headers (for explicit template instantiation)
#include "chip/video/tms9918/tms9918_original.hpp"
#include "chip/video/tms9918/tms9918a.hpp"
#include "chip/video/tms9918/tms9928a.hpp"
#include "chip/video/tms9918/tms9929.hpp"
#include "chip/video/tms9918/tms9929a.hpp"
#include "chip/video/tms9918/v9938.hpp"
#include "chip/video/tms9918/v9958.hpp"
#include "chip/video/tms9918/sega_315_5124.hpp"
#include "chip/video/tms9918/sega_315_5246.hpp"

#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

// ============================================================================
// Chip layout and signal mapping
// ============================================================================

#ifdef CERMU_HAS_GUI

// ── TMS9918/A / TMS9928A / TMS9929/A — 40-pin DIP ─────────────────────────
//
// Pinout from TI TMS9918A datasheet (DIP-40):

template <const tms9918::VDPTraits& Traits>
ChipLayout* tms9918::tms9918_t<Traits>::create_chip_layout() const {
    if constexpr (!Traits.is_v9938_class() && !Traits.is_sega()) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_dip40_layout();

            PIN_LR(layout,  1, _RAS,     VCC,      40);
            PIN_LR(layout,  2, _CAS,     EXTVDP,   39);
            PIN_LR(layout,  3, AD7,      _CSR,     38);
            PIN_LR(layout,  4, AD6,      _CSW,     37);
            PIN_LR(layout,  5, AD5,      _INT,     36);
            PIN_LR(layout,  6, AD4,      D7,       35);
            PIN_LR(layout,  7, AD3,      D6,       34);
            PIN_LR(layout,  8, AD2,      D5,       33);
            PIN_LR(layout,  9, AD1,      D4,       32);
            PIN_LR(layout, 10, AD0,      D3,       31);
            PIN_LR(layout, 11, RW,       D2,       30);
            PIN_LR(layout, 12, GND,      D1,       29);
            PIN_LR(layout, 13, MODE,     D0,       28);
            PIN_LR(layout, 14, GND,      GND,      27);
            PIN_LR(layout, 15, XTAL1,    COMVID,   26);
            PIN_LR(layout, 16, XTAL2,    VCC,      25);
            PIN_LR(layout, 17, CPUCLK,   _RES,     24);
            PIN_LR(layout, 18, GND,      GROMCLK,  23);
            PIN_LR(layout, 19, NC,       NC,       22);
            PIN_LR(layout, 20, NC,       NC,       21);

            return layout;
        }();
        return &layout;
    } else if constexpr (Traits.is_v9938_class() && !Traits.is_sega()) {
        // ── V9938 / V9958 — 64-pin SDIP ────────────────────────────────────
        //
        // Pinout from Yamaha V9938 datasheet (SDIP-64).
        // V9958 is pin-compatible (same pinout).
        static ChipLayout layout = [] {
            ChipLayout layout = create_dip64_layout();

            PIN_LR(layout,  1, VCC,       _HSYNC,   64);
            PIN_LR(layout,  2, _WAIT,     _CSYNC,   63);
            PIN_LR(layout,  3, _INT,      CLK,      62);
            PIN_LR(layout,  4, GROMCLK,   VDD,      61);
            PIN_LR(layout,  5, _CSW,      _DHCLK,   60);
            PIN_LR(layout,  6, _CSR,      _DLCLK,   59);
            PIN_LR(layout,  7, A0,        R_VID,    58);
            PIN_LR(layout,  8, MODE,      G_VID,    57);
            PIN_LR(layout,  9, D7,        B_VID,    56);
            PIN_LR(layout, 10, D6,        VCC,      55);
            PIN_LR(layout, 11, D5,        YS,       54);
            PIN_LR(layout, 12, D4,        CBDR,     53);
            PIN_LR(layout, 13, D3,        VDD,      52);
            PIN_LR(layout, 14, D2,        NC,       51);
            PIN_LR(layout, 15, D1,        NC,       50);
            PIN_LR(layout, 16, D0,        NC,       49);
            PIN_LR(layout, 17, VCC,       NC,       48);
            PIN_LR(layout, 18, _RESET,    AD7,      47);
            PIN_LR(layout, 19, XTAL2,     AD6,      46);
            PIN_LR(layout, 20, XTAL1,     AD5,      45);
            PIN_LR(layout, 21, VDD,       AD4,      44);
            PIN_LR(layout, 22, _RAS,      AD3,      43);
            PIN_LR(layout, 23, _CAS0,     AD2,      42);
            PIN_LR(layout, 24, _CAS1,     AD1,      41);
            PIN_LR(layout, 25, RW,        AD0,      40);
            PIN_LR(layout, 26, VCC,       GND,      39);
            PIN_LR(layout, 27, _VSYNC,    NC,       38);
            PIN_LR(layout, 28, _CPUCLK,   VCC,      37);
            PIN_LR(layout, 29, GND,       NC,       36);
            PIN_LR(layout, 30, COLOR_BUS, NC,       35);
            PIN_LR(layout, 31, NC,        VDD,      34);
            PIN_LR(layout, 32, NC,        NC,       33);

            return layout;
        }();
        return &layout;
    } else if constexpr (Traits.is_sega()) {
        // ── Sega 315-5124 / 315-5246 — 68-pin QFP ─────────────────────────
        //
        // Sega Master System / Mark III VDP.  The 315-5124 and 315-5246 are
        // QFP-68 flat-packs.  No standard 68-pin layout exists in the factory
        // functions, so we use the closest match (QFP-64) as a placeholder.
        // Pin labels are representative; exact Sega pinouts are not fully
        // documented in public datasheets.
        static ChipLayout layout = [] {
            ChipLayout layout = create_qfp64_layout();

            // Left pins 1-16 / Right pins 48-33
            PIN_LR(layout,  1, VCC,      AD0,         48);
            PIN_LR(layout,  2, _INT,     AD1,         47);
            PIN_LR(layout,  3, _CSW,     AD2,         46);
            PIN_LR(layout,  4, _CSR,     AD3,         45);
            PIN_LR(layout,  5, A0,       AD4,         44);
            PIN_LR(layout,  6, D7,       AD5,         43);
            PIN_LR(layout,  7, D6,       AD6,         42);
            PIN_LR(layout,  8, D5,       AD7,         41);
            PIN_LR(layout,  9, D4,       _CAS,        40);
            PIN_LR(layout, 10, D3,       _RAS,        39);
            PIN_LR(layout, 11, D2,       _WE,         38);
            PIN_LR(layout, 12, D1,       GND,         37);
            PIN_LR(layout, 13, D0,       _OE,         36);
            PIN_LR(layout, 14, GND,      CSYNC,       35);
            PIN_LR(layout, 15, XTAL1,    B_VID,       34);
            PIN_LR(layout, 16, XTAL2,    G_VID,       33);

            // Top pins 64-49 / Bottom pins 17-32
            PIN_TB(layout, 64, VCC,      R_VID,       17);
            PIN_TB(layout, 63, HL,       Y_VID,       18);
            PIN_TB(layout, 62, HSYNC,    VCC,         19);
            PIN_TB(layout, 61, VSYNC,    GND,         20);
            PIN_TB(layout, 60, _RES,     NC,          21);
            PIN_TB(layout, 59, NC,       NC,          22);
            PIN_TB(layout, 58, NC,       NC,          23);
            PIN_TB(layout, 57, NC,       NC,          24);
            PIN_TB(layout, 56, NC,       NC,          25);
            PIN_TB(layout, 55, NC,       NC,          26);
            PIN_TB(layout, 54, NC,       NC,          27);
            PIN_TB(layout, 53, NC,       NC,          28);
            PIN_TB(layout, 52, NC,       NC,          29);
            PIN_TB(layout, 51, NC,       NC,          30);
            PIN_TB(layout, 50, NC,       NC,          31);
            PIN_TB(layout, 49, NC,       NC,          32);

            return layout;
        }();
        return &layout;
    } else {
        return nullptr;
    }
}

template <const tms9918::VDPTraits& Traits>
std::vector<PinSignalState> tms9918::tms9918_t<Traits>::get_layout_pin_states(ChipLayout& layout) {
    // TMS9918 does not use bus_snapshot_ — return default-initialised states
    return build_pin_states(layout, 0);
}

// ── Explicit template instantiation — GUI methods ──────────────────────────

template ChipLayout* tms9918::tms9918_t<tms9918::TMS9918Traits>::create_chip_layout() const;
template std::vector<PinSignalState> tms9918::tms9918_t<tms9918::TMS9918Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* tms9918::tms9918_t<tms9918::TMS9918ATraits>::create_chip_layout() const;
template std::vector<PinSignalState> tms9918::tms9918_t<tms9918::TMS9918ATraits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* tms9918::tms9918_t<tms9918::TMS9928ATraits>::create_chip_layout() const;
template std::vector<PinSignalState> tms9918::tms9918_t<tms9918::TMS9928ATraits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* tms9918::tms9918_t<tms9918::TMS9929Traits>::create_chip_layout() const;
template std::vector<PinSignalState> tms9918::tms9918_t<tms9918::TMS9929Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* tms9918::tms9918_t<tms9918::TMS9929ATraits>::create_chip_layout() const;
template std::vector<PinSignalState> tms9918::tms9918_t<tms9918::TMS9929ATraits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* tms9918::tms9918_t<tms9918::V9938Traits>::create_chip_layout() const;
template std::vector<PinSignalState> tms9918::tms9918_t<tms9918::V9938Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* tms9918::tms9918_t<tms9918::V9958Traits>::create_chip_layout() const;
template std::vector<PinSignalState> tms9918::tms9918_t<tms9918::V9958Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* tms9918::tms9918_t<tms9918::SEGA_315_5124Traits>::create_chip_layout() const;
template std::vector<PinSignalState> tms9918::tms9918_t<tms9918::SEGA_315_5124Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* tms9918::tms9918_t<tms9918::SEGA_315_5246Traits>::create_chip_layout() const;
template std::vector<PinSignalState> tms9918::tms9918_t<tms9918::SEGA_315_5246Traits>::get_layout_pin_states(ChipLayout&);

#endif // CERMU_HAS_GUI
