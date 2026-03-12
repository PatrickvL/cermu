#pragma once
/*
 * mos6502.h — MOS 6502 CPU type (separation layer)
 *
 * Defines the MOS 6502 CPU trait, type alias, and chip pin layout.
 * Consumer code includes just this header to get the MOS 6502 variant.
 */

#include "chip/cpu/fam65xx/fam65xx.hpp"
#ifdef CERMU_HAS_GUI
#include "chip/cpu/fam65xx/fam65xx_pin_layout.hpp"
#include "core/system_lines.hpp"
#endif

namespace fam65xx {

// MOS 6502 — Original NMOS with illegal opcodes
inline constexpr CPUTraits MOS6502Traits = {
    "MOS Technology",                             // vendor
    "6502",                                       // chip_id
    CoreFlags::NMOS_BASE,                         // core_flags
    16,                                           // address_bits
    0x00,                                         // io_port_mask
    BankingType::NONE,                            // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};

using MOS6502 = fam65xx_t<MOS6502Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using MOS6502 = fam65xx::MOS6502;

#ifdef CERMU_HAS_GUI
// ============================================================================
// MOS 6502 PIN LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_mos6502_layout() {
  ChipLayout layout = create_dip40_layout();

  layout.markings = {
      "MOS6502",        // part_number
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

  // Pin assignments for MOS 6502 (40-pin DIP)
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

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::MOS6502Traits>() {
  return create_mos6502_layout();
}
#endif // CERMU_HAS_GUI
