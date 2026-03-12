#pragma once
/*
 * mos6509.h — MOS 6509 CPU type (separation layer)
 *
 * Defines the MOS 6509 CPU trait, type alias, and chip pin layout.
 * Consumer code includes just this header to get the MOS 6509 variant.
 *
 * The 6509 extends the 6502 with a 20-bit address space via internal
 * bank registers at $0000 (indirection bank) and $0001 (execution bank),
 * used in the Commodore CBM-II (B-series / P500).
 *
 * Physically the 6509 is pin-identical to the MOS 6502 (40-pin DIP).
 * The bank byte is communicated to external latch hardware during
 * specific bus phases — no additional address pins are exposed.
 */

#include "chip/cpu/fam65xx/fam65xx.hpp"
#ifdef CERMU_HAS_GUI
#include "chip/cpu/fam65xx/fam65xx_pin_layout.hpp"
#include "core/system_lines.hpp"
#endif

namespace fam65xx {

inline constexpr CPUTraits MOS6509Traits = {
    "MOS Technology",                                 // vendor
    "6509",                                           // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_BANKING, // core_flags
    20,                                               // address_bits
    0x00,                                             // io_port_mask
    BankingType::MOS6509,                             // banking
    {SoundChip::NONE, DMAController::NONE, false}     // peripheral
};
// MOS 6509 Bank Register assignments (no I/O port — banking only):
//   $0000: Indirection bank register (4-bit, selects bank for (zp),Y)
//   $0001: Execution bank register (4-bit, selects bank for all other access)
//   Banks 0-15 map 16 × 64 KB = 1 MB address space (20-bit)
//   External hardware latches the bank register output during PHI1
//   and combines it with A0-A15 to form the full 20-bit address.

using MOS6509 = fam65xx_t<MOS6509Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using MOS6509 = fam65xx::MOS6509;

#ifdef CERMU_HAS_GUI
// ============================================================================
// MOS 6509 PIN LAYOUT (40-pin DIP)
// ============================================================================
// The 6509 is pin-identical to the MOS 6502.  Banking register output
// is multiplexed onto the data bus during PHI1 and latched externally
// by the CBM-II board logic — no extra address pins exist.

inline ChipLayout create_mos6509_layout() {
  ChipLayout layout = create_dip40_layout();

  layout.markings = {
      "MOS6509",        // part_number
      "MOS Technology", // manufacturer
      {},               // package_variant
      {},               // date_code
      {},               // lot_number
      {},               // custom_text
      true,             // show_part_number
      true,             // show_manufacturer
      false,            // show_package_variant
      false             // show_date_code
  };

  // Pin assignments for MOS 6509 (40-pin DIP) — identical to MOS 6502
  PIN_LR(layout, 1, VSS, VSS, 21)
  PIN_LR(layout, 2, RDY, A12, 22)   // Ready
  PIN_LR(layout, 3, PHI1, A13, 23)
  PIN_LR(layout, 4, _IRQ, A14, 24)  // Interrupt Request
  PIN_LR(layout, 5, NC, A15, 25)
  PIN_LR(layout, 6, _NMI, D7, 26)   // Non-Maskable Interrupt
  PIN_LR(layout, 7, SYNC, D6, 27)   // Instruction fetch indicator
  PIN_LR(layout, 8, VDD, D5, 28)
  PIN_LR(layout, 9, A0, D4, 29)
  PIN_LR(layout, 10, A1, D3, 30)
  PIN_LR(layout, 11, A2, D2, 31)
  PIN_LR(layout, 12, A3, D1, 32)
  PIN_LR(layout, 13, A4, D0, 33)
  PIN_LR(layout, 14, A5, RW, 34)    // Read/Write
  PIN_LR(layout, 15, A6, NC, 35)
  PIN_LR(layout, 16, A7, NC, 36)
  PIN_LR(layout, 17, A8, PHI0, 37)
  PIN_LR(layout, 18, A9, _SO, 38)   // Set Overflow flag
  PIN_LR(layout, 19, A10, PHI2, 39)
  PIN_LR(layout, 20, A11, _RES, 40) // Reset

  return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::MOS6509Traits>() {
  return create_mos6509_layout();
}
#endif // CERMU_HAS_GUI
