#pragma once
/*
 * mos6510.h — MOS 6510 CPU type (separation layer)
 *
 * Defines the MOS 6510 / 6510T CPU traits, type alias, and chip pin layout.
 * Consumer code includes just this header to get the MOS 6510 variant.
 */

#include "fam65xx.hpp"
#include "fam65xx_pin_layout.h"
#include "../../../core/system_lines.h"

namespace fam65xx {

// MOS 6510 — NMOS with built-in I/O port (C64 CPU)
inline constexpr CPUTraits MOS6510Traits = {
    "MOS Technology",                                 // vendor
    "6510",                                           // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_IO_PORT, // core_flags
    16,                                               // address_bits
    0x3F,                                             // io_port_mask (Pins 0-5)
    BankingType::NONE,                                // banking
    {SoundChip::NONE, DMAController::NONE, false}     // peripheral
};
// MOS 6510 I/O Port bit assignments (mask 0x3F):
//   Bit 0: LORAM  — BASIC ROM enable (active high, output)
//   Bit 1: HIRAM  — KERNAL ROM enable (active high, output)
//   Bit 2: CHAREN — Character ROM / I/O select (active high, output)
//   Bit 3: Cassette data output (directly drives tape write)
//   Bit 4: Cassette sense (active low, input — tape button pressed)
//   Bit 5: Cassette motor control (active low, output)
//   Bits 6-7: Not connected (absent from mask)

inline constexpr CPUTraits MOS6510TTraits = MOS6510Traits; // Identical

using MOS6510 = fam65xx_t<MOS6510Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using MOS6510 = fam65xx::MOS6510;

// ============================================================================
// MOS 6510 PIN LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_mos6510_layout() {
  ChipLayout layout = create_dip40_layout();

  layout.markings = {
      "MOS6510",        // part_number
      "MOS Technology", // manufacturer
      {},          // package_variant
      {},          // date_code
      {},          // lot_number
      {},          // custom_text
      true,             // show_part_number
      true,             // show_manufacturer
      false,            // show_package_variant
      false             // show_date_code
  };

  // Pin assignments for MOS 6510 (40-pin DIP)
  PIN_LR(layout, 1, PHI0, VSS, 21)
  PIN_LR(layout, 2, RDY, A12, 22)    // Ready
  PIN_LR(layout, 3, _IRQ, A13, 23)   // Interrupt Request
  PIN_LR(layout, 4, _NMI, P0, 24)    // Non-Maskable Interrupt / I/O Port bit 0
  PIN_LR(layout, 5, AEC, P1, 25)     // Address Enable Control / I/O Port bit 1
  PIN_LR(layout, 6, VDD, P2, 26)     // I/O Port bit 2
  PIN_LR(layout, 7, A0, P3, 27)      // I/O Port bit 3
  PIN_LR(layout, 8, A1, P4, 28)      // I/O Port bit 4
  PIN_LR(layout, 9, A2, P5, 29)      // I/O Port bit 5
  PIN_LR(layout, 10, A3, D7, 30)
  PIN_LR(layout, 11, A4, D6, 31)
  PIN_LR(layout, 12, A5, D5, 32)
  PIN_LR(layout, 13, A6, D4, 33)
  PIN_LR(layout, 14, A7, D3, 34)
  PIN_LR(layout, 15, A8, D2, 35)
  PIN_LR(layout, 16, A9, D1, 36)
  PIN_LR(layout, 17, A10, D0, 37)
  PIN_LR(layout, 18, A11, RW, 38)    // Read/Write
  PIN_LR(layout, 19, A14, PHI2, 39)
  PIN_LR(layout, 20, A15, _RES, 40)  // Reset

  return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::MOS6510Traits>() {
  return create_mos6510_layout();
}
