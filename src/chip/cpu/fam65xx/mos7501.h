#pragma once
/*
 * mos7501.h — CSG 7501/8501 CPU type (separation layer)
 *
 * Defines the CSG 7501/8501 CPU trait, type alias, and chip pin layout.
 * Consumer code includes just this header to get the CSG 7501 variant.
 */

#include "fam65xx.hpp"
#ifdef IMGUI_VERSION
#include "fam65xx_pin_layout.h"
#include "../../../core/system_lines.h"
#endif

namespace fam65xx {

// CSG 7501/8501 — C16/Plus4 CPU
// I/O Port bit assignments (active via DDR at $00/$01, mask 0x5F):
//   Bit 0: Cassette motor control (active low, output)
//   Bit 1: Serial bus SRQ IN (directly from IEC bus)
//   Bit 2: Serial bus DATA (directly from IEC bus)
//   Bit 3: Serial bus CLK  (directly from IEC bus)
//   Bit 4: Serial bus ATN  (directly from IEC bus)
//   Bit 5: Not connected   (absent from mask — no physical pin)
//   Bit 6: Cassette sense   (active low, input — directly samples tape data)
//
// Banking: ROM selection is driven jointly by bits 0-3 and TED registers,
// unlike the 6510 where LORAM/HIRAM/CHAREN in the CPU port drive PLA
// banking more directly. Here the TED participates in address decode.
//
// No NMI line: GATE IN (from TED) replaces NMI on this chip.
inline constexpr CPUTraits CSG7501Traits = {
    "Commodore", // vendor
    "7501",      // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_IO_PORT |
        CPUCoreFlags::NO_NMI_LINE, // core_flags
    16,                            // address_bits
    0x5F,                          // io_port_mask (Pins 0-4, 6 — no pin 5)
    BankingType::NONE,             // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};

using CSG7501 = fam65xx_t<CSG7501Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using CSG7501 = fam65xx::CSG7501;

#ifdef IMGUI_VERSION
// ============================================================================
// CSG 7501/8501 PIN LAYOUT (40-pin DIP, C16/Plus4 CPU)
// ============================================================================

inline ChipLayout create_csg7501_layout() {
  ChipLayout layout = create_dip40_layout();

  layout.markings = {
      "CSG7501",    // part_number (also branded 8501)
      "Commodore",  // manufacturer
      {},      // package_variant
      {},      // date_code
      {},      // lot_number
      {},      // custom_text
      true,         // show_part_number
      true,         // show_manufacturer
      false,        // show_package_variant
      false         // show_date_code
  };

  // Pin assignments for CSG 7501/8501 (40-pin DIP)
  // Similar to 6510 but with different I/O port mapping and no NMI line.
  // I/O port mask 0x5F = bits 0,1,2,3,4,6 (no bit 5).
  // The GATE IN pin replaces the NMI — directly managed by TED.
  PIN_LR(layout, 1, PHI0, VSS, 21)
  PIN_LR(layout, 2, RDY, A12, 22)    // Ready
  PIN_LR(layout, 3, _IRQ, A13, 23)   // Interrupt Request
  PIN_LR(layout, 4, AEC, A14, 24)    // Address Enable Control (from TED)
  PIN_LR(layout, 5, VDD, A15, 25)
  PIN_LR(layout, 6, A0, P0, 26)      // I/O Port bit 0
  PIN_LR(layout, 7, A1, P1, 27)      // I/O Port bit 1
  PIN_LR(layout, 8, A2, P2, 28)      // I/O Port bit 2
  PIN_LR(layout, 9, A3, P3, 29)      // I/O Port bit 3
  PIN_LR(layout, 10, A4, P4, 30)     // I/O Port bit 4
  PIN_LR(layout, 11, A5, D7, 31)
  PIN_LR(layout, 12, A6, D6, 32)
  PIN_LR(layout, 13, A7, D5, 33)
  PIN_LR(layout, 14, A8, D4, 34)
  PIN_LR(layout, 15, A9, D3, 35)
  PIN_LR(layout, 16, A10, D2, 36)
  PIN_LR(layout, 17, A11, D1, 37)
  PIN_LR(layout, 18, RW, D0, 38)     // Read/Write
  PIN_LR(layout, 19, P6, PHI2, 39)   // I/O Port bit 6 (no bit 5 — mask 0x5F)
  PIN_LR(layout, 20, A14, _RES, 40)  // Reset

  return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::CSG7501Traits>() {
  return create_csg7501_layout();
}
#endif // IMGUI_VERSION
