#pragma once
/*
 * wdc65c816.h — WDC 65C816 CPU type (separation layer)
 *
 * Defines the WDC 65C816 CPU trait, type alias, and chip pin layout.
 * Consumer code includes just this header to get the WDC 65C816 variant.
 */

#include "fam65xx.hpp"
#ifdef CERMU_HAS_GUI
#include "fam65xx_pin_layout.h"
#include "../../../core/system_lines.h"
#endif

namespace fam65xx {

// WDC 65C816 — 16-bit CMOS extension of the 6502 (SNES CPU core)
inline constexpr CPUTraits WDC_65C816Traits = {
    "WDC",                                                 // vendor
    "65C816",                                              // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::C816_16BIT, // core_flags
    24,                                                    // address_bits
    0x00,                                                  // io_port_mask
    BankingType::NONE,                                     // banking
    {SoundChip::NONE, DMAController::NONE, false}          // peripheral
};

using WDC_65C816 = fam65xx_t<WDC_65C816Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using WDC_65C816 = fam65xx::WDC_65C816;

#ifdef CERMU_HAS_GUI
// ============================================================================
// WDC 65C816 PIN LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_wdc_65c816_layout() {
  ChipLayout layout = create_dip40_layout();

  layout.markings = {
      "W65C816S",              // part_number
      "Western Design Center", // manufacturer
      {},                 // package_variant
      {},                 // date_code
      {},                 // lot_number
      {},                 // custom_text
      true,                    // show_part_number
      true,                    // show_manufacturer
      false,                   // show_package_variant
      false                    // show_date_code
  };

  // Pin assignments for WDC 65C816 (40-pin DIP)
  PIN_LR(layout, 1, _VPB, VSS, 21)    // Vector Pull Bar (low during vector fetch)
  PIN_LR(layout, 2, RDY, A12, 22)     // Ready
  PIN_LR(layout, 3, _ABORT, A13, 23)  // Abort current instruction
  PIN_LR(layout, 4, _IRQ, A14, 24)    // Interrupt Request
  PIN_LR(layout, 5, _ML, A15, 25)     // Memory Lock (low during RMW)
  PIN_LR(layout, 6, _NMI, D7, 26)     // Non-Maskable Interrupt
  PIN_LR(layout, 7, VPA, D6, 27)      // Valid Program Address
  PIN_LR(layout, 8, VDD, D5, 28)
  PIN_LR(layout, 9, A0, D4, 29)
  PIN_LR(layout, 10, A1, D3, 30)
  PIN_LR(layout, 11, A2, D2, 31)
  PIN_LR(layout, 12, A3, D1, 32)
  PIN_LR(layout, 13, A4, D0, 33)
  PIN_LR(layout, 14, A5, RW, 34)      // Read/Write
  PIN_LR(layout, 15, A6, E, 35)       // Emulation mode status
  PIN_LR(layout, 16, A7, BE, 36)      // Bus Enable
  PIN_LR(layout, 17, A8, PHI0, 37)
  PIN_LR(layout, 18, A9, MX, 38)      // M/X status flags
  PIN_LR(layout, 19, A10, VDA, 39)    // Valid Data Address
  PIN_LR(layout, 20, A11, _RES, 40)   // Reset

  return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::WDC_65C816Traits>() {
  return create_wdc_65c816_layout();
}
#endif // CERMU_HAS_GUI
