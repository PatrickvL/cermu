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
    } else {
        // V9938/V9958 (64-pin) and Sega custom — no layout yet
        return nullptr;
    }
}

template <const tms9918::VDPTraits& Traits>
std::vector<PinSignalState> tms9918::tms9918_t<Traits>::get_layout_pin_states(ChipLayout& layout) {
    // TMS9918 does not use bus_snapshot_ — return default-initialised states
    return populate_pin_states_from_bus(layout, 0);
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
