#pragma once
/*
 * ricoh_2a03.h — Ricoh 2A03/2A07 CPU type (separation layer)
 *
 * Defines the Ricoh 2A03 (NTSC) / 2A07 (PAL) CPU traits, type alias,
 * and chip pin layout.  Consumer code includes just this header to get
 * the Ricoh 2A03 variant.
 */

#include "chip/cpu/fam65xx/fam65xx.hpp"
#ifdef CERMU_HAS_GUI
#include "chip/cpu/fam65xx/fam65xx_pin_layout.h"
#include "core/system_lines.h"
#endif

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

#ifdef CERMU_HAS_GUI
// ============================================================================
// RICOH 2A03 PIN LAYOUT (40-pin DIP, NES CPU)
// ============================================================================

inline ChipLayout create_ricoh_2a03_layout() {
  ChipLayout layout = create_dip40_layout();

  layout.markings = {
      "RP2A03",   // part_number
      "Ricoh",    // manufacturer
      {},  // package_variant
      {},  // date_code
      {},  // lot_number
      {},  // custom_text
      true,       // show_part_number
      true,       // show_manufacturer
      false,      // show_package_variant
      false       // show_date_code
  };

  // Hardware-accurate Ricoh RP2A03G pinout (40-pin DIP)
  // The 2A03 integrates a modified 6502 CPU core (no BCD) with an APU,
  // DMA controller, and controller I/O ports on a single die.
  PIN_LR(layout,  1, AD1,   VCC,   40);  // audio delta-sigma 1 / +5V
  PIN_LR(layout,  2, AD2,   CLK,   39);  // audio delta-sigma 2 / master clock in
  PIN_LR(layout,  3, _RES,  _NMI,  38);  // reset / NMI
  PIN_LR(layout,  4, A0,    _IRQ,  37);  // address bus / interrupt
  PIN_LR(layout,  5, A1,    M2,    36);  // address bus / CPU clock out
  PIN_LR(layout,  6, A2,    SND1,  35);  // address bus / sound output 1
  PIN_LR(layout,  7, A3,    SND2,  34);  // address bus / sound output 2
  PIN_LR(layout,  8, A4,    IN0,   33);  // address bus / controller 1 data
  PIN_LR(layout,  9, A5,    IN1,   32);  // address bus / controller 2 data
  PIN_LR(layout, 10, A6,    D0,    31);  // address bus / data bus
  PIN_LR(layout, 11, A7,    D1,    30);
  PIN_LR(layout, 12, A8,    D2,    29);
  PIN_LR(layout, 13, A9,    D3,    28);
  PIN_LR(layout, 14, A10,   D4,    27);
  PIN_LR(layout, 15, A11,   D5,    26);
  PIN_LR(layout, 16, A12,   D6,    25);
  PIN_LR(layout, 17, A13,   D7,    24);  // address hi / data hi
  PIN_LR(layout, 18, A14,   OUT0,  23);  // address bus / ctrl latch strobe
  PIN_LR(layout, 19, RW,    OUT1,  22);  // R/W / ctrl strobe 1
  PIN_LR(layout, 20, VSS,   OUT2,  21);  // GND / ctrl strobe 2

  return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::RICOH_2A03Traits>() {
  return create_ricoh_2a03_layout();
}
#endif // CERMU_HAS_GUI
