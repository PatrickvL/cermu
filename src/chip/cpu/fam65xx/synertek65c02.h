#pragma once
/*
 * synertek65c02.h — Synertek 65C02 CPU type (separation layer)
 *
 * Defines the Synertek 65C02 CPU trait and type alias.
 * Pin-compatible with the NMOS 6502 (40-pin DIP, same pinout).
 */

#include "fam65xx.hpp"
#ifdef CERMU_HAS_GUI
#include "fam65xx_pin_layout.h"
#endif

namespace fam65xx {

// Synertek 65C02 — CMOS base (same flags as early WDC, different vendor)
inline constexpr CPUTraits SYNERTEK_65C02Traits = {
    "Synertek",                                   // vendor
    "65C02",                                      // chip_id
    CoreFlags::CMOS_BASE_FLAGS,                   // core_flags
    16,                                           // address_bits
    0x00,                                         // io_port_mask
    BankingType::NONE,                            // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};

using SYNERTEK_65C02 = fam65xx_t<SYNERTEK_65C02Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using SYNERTEK_65C02 = fam65xx::SYNERTEK_65C02;

#ifdef CERMU_HAS_GUI
// ============================================================================
// SYNERTEK 65C02 PIN LAYOUT (40-pin DIP, 6502-compatible)
// ============================================================================

inline ChipLayout create_synertek65c02_layout() {
    ChipLayout layout = create_dip40_layout();

    layout.left_pins.clear();
    layout.right_pins.clear();

    layout.markings = {
        "SY65C02",          // part_number
        "Synertek",         // manufacturer
        {},                 // package_variant
        {},                 // date_code
        {},                 // lot_number
        {},                 // custom_text
        true,               // show_part_number
        true,               // show_manufacturer
        false,              // show_package_variant
        false               // show_date_code
    };

    // Same pinout as MOS 6502 (40-pin DIP, pin-compatible)
    PIN_LR(layout,  1, VSS,   VSS,  21);
    PIN_LR(layout,  2, RDY,   A12,  22);
    PIN_LR(layout,  3, PHI1,  A13,  23);
    PIN_LR(layout,  4, _IRQ,  A14,  24);
    PIN_LR(layout,  5, NC,    A15,  25);
    PIN_LR(layout,  6, _NMI,  D7,   26);
    PIN_LR(layout,  7, SYNC,  D6,   27);
    PIN_LR(layout,  8, VDD,   D5,   28);
    PIN_LR(layout,  9, A0,    D4,   29);
    PIN_LR(layout, 10, A1,    D3,   30);
    PIN_LR(layout, 11, A2,    D2,   31);
    PIN_LR(layout, 12, A3,    D1,   32);
    PIN_LR(layout, 13, A4,    D0,   33);
    PIN_LR(layout, 14, A5,    RW,   34);
    PIN_LR(layout, 15, A6,    NC,   35);
    PIN_LR(layout, 16, A7,    NC,   36);
    PIN_LR(layout, 17, A8,    PHI0, 37);
    PIN_LR(layout, 18, A9,    _SO,  38);
    PIN_LR(layout, 19, A10,   PHI2, 39);
    PIN_LR(layout, 20, A11,   _RES, 40);

    return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::SYNERTEK_65C02Traits>() {
    return create_synertek65c02_layout();
}
#endif // CERMU_HAS_GUI
