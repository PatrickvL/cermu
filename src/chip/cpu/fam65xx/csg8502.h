#pragma once
/*
 * csg8502.h — CSG 8502 CPU type (separation layer)
 *
 * Defines the CSG 8502 CPU trait, type alias, and chip pin layout.
 * Consumer code includes just this header to get the CSG 8502 variant.
 *
 * The 8502 is the Commodore 128 CPU — an NMOS 65xx with a 7-bit I/O
 * port (bits 0-6 active, bit 7 always reads 1), and support for
 * variable clock speed (1 MHz / 2 MHz switching via port bit 6).
 *
 * Physically the 8502 is pin-identical to the MOS 6510 (40-pin DIP).
 * Port bit 6 (clock speed select) is internal-only and does not appear
 * on an external pin — it directly controls the clock divider logic
 * inside the chip.
 */

#include "chip/cpu/fam65xx/fam65xx.hpp"
#ifdef CERMU_HAS_GUI
#include "chip/cpu/fam65xx/fam65xx_pin_layout.h"
#include "core/system_lines.h"
#endif

namespace fam65xx {

// CSG 8502 — C128 CPU (NMOS with I/O port and variable clock)
inline constexpr CPUTraits CSG8502Traits = {
    "Commodore", // vendor
    "CSG8502",   // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_IO_PORT |
        CPUCoreFlags::VARIABLE_CLOCK, // core_flags
    16,                               // address_bits
    0x7F,                             // io_port_mask (Pins 0-6, no pin 7)
    BankingType::NONE,                // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};
// CSG 8502 I/O Port bit assignments (mask 0x7F):
//   Bit 0: LORAM  — BASIC ROM enable (active high, output)
//   Bit 1: HIRAM  — KERNAL ROM enable (active high, output)
//   Bit 2: CHAREN — Character ROM / I/O select (active high, output)
//   Bit 3: Cassette data output (directly drives tape write)
//   Bit 4: Cassette motor control (active low, output)
//   Bit 5: Cassette sense (active low, input — tape button pressed)
//   Bit 6: Processor clock rate (0 = 1 MHz, 1 = 2 MHz) — INTERNAL ONLY
//   Bit 7: Always reads 1 (absent from mask — no physical pin)
//
// Bits 0-5 have dedicated external pins (P0-P5), identical to the 6510.
// Bit 6 has no external pin — it controls the internal clock divider.

using CSG8502 = fam65xx_t<CSG8502Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using CSG8502 = fam65xx::CSG8502;

#ifdef CERMU_HAS_GUI
// ============================================================================
// CSG 8502 PIN LAYOUT (40-pin DIP)
// ============================================================================
// The 8502 is pin-identical to the MOS 6510.  Port bit 6 (clock speed)
// is internal-only with no dedicated external pin.  Physical pins P0-P5
// on pins 24-29 match the 6510 exactly.

inline ChipLayout create_csg8502_layout() {
  ChipLayout layout = create_dip40_layout();

  layout.markings = {
      "CSG8502",    // part_number
      "Commodore",  // manufacturer
      {},           // package_variant
      {},           // date_code
      {},           // lot_number
      {},           // custom_text
      true,         // show_part_number
      true,         // show_manufacturer
      false,        // show_package_variant
      false         // show_date_code
  };

  // Pin assignments for CSG 8502 (40-pin DIP) — identical to MOS 6510
  // Port bits 0-5 on pins 24-29; port bit 6 is internal (clock speed).
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

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::CSG8502Traits>() {
  return create_csg8502_layout();
}
#endif // CERMU_HAS_GUI
