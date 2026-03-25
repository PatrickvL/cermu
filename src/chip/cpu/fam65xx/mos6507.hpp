#pragma once
/*
 * mos6507.h — MOS 6507 CPU type (separation layer)
 *
 * Defines the MOS 6507 CPU trait.  The 6507 is a pin-reduced 6502 with
 * only 13 address lines and no IRQ/NMI lines (used in the Atari 2600).
 * 28-pin DIP package.
 */

#include "chip/cpu/fam65xx/fam65xx.hpp"
#ifdef CERMU_HAS_GUI
#include "chip/cpu/fam65xx/fam65xx_pin_layout.hpp"
#endif

namespace fam65xx {

inline constexpr CPUTraits MOS6507Traits = {
    "MOS Technology",                                 // vendor
    "6507",                                           // chip_id
    "MOS 6507",                                       // display_name
    CoreFlags::NMOS_BASE | CPUCoreFlags::NO_IRQ_LINE | CPUCoreFlags::NO_NMI_LINE, // core_flags
    13,                                               // address_bits
    0x00,                                             // io_port_mask
    BankingType::NONE,                                // banking
    {SoundChip::NONE, DMAController::NONE, false}     // peripheral
};

} // namespace fam65xx

// Type alias — instantiate the MOS 6507 as a distinct type.
using MOS6507 = fam65xx::fam65xx_t<fam65xx::MOS6507Traits>;

#ifdef CERMU_HAS_GUI
// ============================================================================
// MOS 6507 PIN LAYOUT (28-pin DIP)
// ============================================================================

inline ChipLayout create_mos6507_layout() {
    ChipLayout layout = create_dip28_layout();

    layout.left_pins.clear();
    layout.right_pins.clear();

    layout.markings = {
        "MOS6507",          // part_number
        "MOS Technology",   // manufacturer
        {},                 // package_variant
        {},                 // date_code
        {},                 // lot_number
        {},                 // custom_text
        true,               // show_part_number
        true,               // show_manufacturer
        false,              // show_package_variant
        false               // show_date_code
    };

    // Hardware-accurate MOS 6507 pinout (28-pin DIP)
    PIN_LR(layout,  1, _RES,   PHI0,  28);  // Reset / Clock input
    PIN_LR(layout,  2, VSS,    RW,    27);  // GND / Read-Write
    PIN_LR(layout,  3, RDY,    D0,    26);  // Ready
    PIN_LR(layout,  4, PHI2,   D1,    25);  // Clock output
    PIN_LR(layout,  5, A0,     D2,    24);
    PIN_LR(layout,  6, A1,     D3,    23);
    PIN_LR(layout,  7, A2,     D4,    22);
    PIN_LR(layout,  8, A3,     D5,    21);
    PIN_LR(layout,  9, A4,     D6,    20);
    PIN_LR(layout, 10, A5,     D7,    19);
    PIN_LR(layout, 11, A6,     A12,   18);
    PIN_LR(layout, 12, A7,     A11,   17);
    PIN_LR(layout, 13, A8,     A10,   16);
    PIN_LR(layout, 14, VCC,    A9,    15);

    return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::MOS6507Traits>() {
    return create_mos6507_layout();
}
#endif // CERMU_HAS_GUI
