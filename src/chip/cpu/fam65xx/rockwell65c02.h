#pragma once
/*
 * rockwell65c02.h — Rockwell R65C02 CPU type (separation layer)
 *
 * Defines the Rockwell R65C02 CPU trait, type alias, and chip pin layout.
 * Consumer code includes just this header to get the Rockwell R65C02 variant.
 */

#include "fam65xx.hpp"
#ifdef CERMU_HAS_GUI
#include "fam65xx_pin_layout.h"
#include "../../../core/system_lines.h"
#endif

namespace fam65xx {

// Rockwell R65C02 — CMOS with Rockwell bit-manipulation extensions (BBR/BBS/RMB/SMB)
inline constexpr CPUTraits ROCKWELL_R65C02Traits = {
    "Rockwell",                                   // vendor
    "R65C02",                                     // chip_id
    CoreFlags::ROCKWELL_BASE,                     // core_flags
    16,                                           // address_bits
    0x00,                                         // io_port_mask
    BankingType::NONE,                            // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};

using ROCKWELL_R65C02 = fam65xx_t<ROCKWELL_R65C02Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using ROCKWELL_R65C02 = fam65xx::ROCKWELL_R65C02;

#ifdef CERMU_HAS_GUI
// ============================================================================
// ROCKWELL R65C02 PIN LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_rockwell_r65c02_layout() {
  ChipLayout layout = create_dip40_layout();

  layout.markings = {
      "R65C02",   // part_number
      "Rockwell", // manufacturer
      {},    // package_variant
      {},    // date_code
      {},    // lot_number
      {},    // custom_text
      true,       // show_part_number
      true,       // show_manufacturer
      false,      // show_package_variant
      false       // show_date_code
  };

  // Pin assignments for Rockwell R65C02 (40-pin DIP)
  PIN_LR(layout, 1, VSS, VSS, 21)
  PIN_LR(layout, 2, RDY, A12, 22)    // Ready
  PIN_LR(layout, 3, PHI1, A13, 23)
  PIN_LR(layout, 4, _IRQ, A14, 24)   // Interrupt Request
  PIN_LR(layout, 5, NC, A15, 25)
  PIN_LR(layout, 6, _NMI, D7, 26)    // Non-Maskable Interrupt
  PIN_LR(layout, 7, SYNC, D6, 27)    // Instruction fetch indicator
  PIN_LR(layout, 8, VDD, D5, 28)
  PIN_LR(layout, 9, A0, D4, 29)
  PIN_LR(layout, 10, A1, D3, 30)
  PIN_LR(layout, 11, A2, D2, 31)
  PIN_LR(layout, 12, A3, D1, 32)
  PIN_LR(layout, 13, A4, D0, 33)
  PIN_LR(layout, 14, A5, RW, 34)     // Read/Write
  PIN_LR(layout, 15, A6, NC, 35)
  PIN_LR(layout, 16, A7, BE, 36)     // Bus Enable
  PIN_LR(layout, 17, A8, PHI0, 37)
  PIN_LR(layout, 18, A9, _SO, 38)    // Set Overflow flag
  PIN_LR(layout, 19, A10, PHI2, 39)
  PIN_LR(layout, 20, A11, _RES, 40)  // Reset

  return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::ROCKWELL_R65C02Traits>() {
  return create_rockwell_r65c02_layout();
}
#endif // CERMU_HAS_GUI
