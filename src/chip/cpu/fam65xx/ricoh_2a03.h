#pragma once
/*
 * ricoh_2a03.h — Ricoh 2A03/2A07 CPU type (separation layer)
 *
 * Defines the Ricoh 2A03 (NTSC) / 2A07 (PAL) CPU traits, type alias,
 * and chip pin layout.  Consumer code includes just this header to get
 * the Ricoh 2A03 variant.
 */

#include "fam65xx.hpp"
#include "../../../core/chip_layout.h"
#include "../../../core/system_lines.h"

namespace fam65xx {

// Ricoh 2A03 — NES CPU (NMOS without BCD, built-in APU)
inline constexpr CPUTraits RICOH_2A03Traits = {
    "Ricoh", // vendor
    "2A03",  // chip_id
    CPUCoreFlags::ILLEGAL_OPCODES | CPUCoreFlags::JMP_INDIRECT_BUG |
        CPUCoreFlags::RMW_DUMMY_WRITE, // core_flags (NO HAS_DECIMAL_MODE!)
    16,                                 // address_bits
    0x00,                               // io_port_mask
    BankingType::NONE,                  // banking
    {SoundChip::RICOH_APU, DMAController::NONE, false} // peripheral
};

inline constexpr CPUTraits RICOH_2A07Traits = RICOH_2A03Traits; // PAL version

using RICOH_2A03 = fam65xx_t<RICOH_2A03Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using RICOH_2A03 = fam65xx::RICOH_2A03;

// ============================================================================
// RICOH 2A03 PIN LAYOUT (40-pin DIP, NES CPU)
// ============================================================================

inline ChipLayout create_ricoh_2a03_layout() {
  ChipLayout layout = create_dip40_layout();

  layout.markings = {
      "RP2A03", // part_number
      "Ricoh",  // manufacturer
      {},  // package_variant
      {},  // date_code
      {},  // lot_number
      {},  // custom_text
      true,     // show_part_number
      true,     // show_manufacturer
      false,    // show_package_variant
      false     // show_date_code
  };

  // Pin assignments for Ricoh 2A03 (40-pin DIP)
  PIN_LR(layout, 1, VSS, VSS, 21)
  PIN_LR(layout, 2, RDY, A12, 22)    // Ready (tied high in some revisions)
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
  PIN_LR(layout, 16, A7, NC, 36)     // No Bus Enable on 2A03
  PIN_LR(layout, 17, A8, PHI0, 37)
  PIN_LR(layout, 18, A9, _SO, 38)    // Set Overflow flag
  PIN_LR(layout, 19, A10, PHI2, 39)
  PIN_LR(layout, 20, A11, _RES, 40)  // Reset

  return layout;
}
