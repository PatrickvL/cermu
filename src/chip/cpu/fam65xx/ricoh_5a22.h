#pragma once
/*
 * ricoh_5a22.h — Ricoh 5A22 CPU type (separation layer)
 *
 * Defines the Ricoh 5A22 CPU trait, type alias, and chip pin layout.
 * Consumer code includes just this header to get the Ricoh 5A22 variant.
 *
 * The 5A22 (also known as S-CPU) is the SNES main processor — a custom
 * Ricoh ASIC containing a 65C816-based 16-bit CPU core with integrated:
 *   - 8-channel DMA controller
 *   - 8-channel HDMA controller
 *   - Hardware multiplication/division unit
 *   - NMI/IRQ management with H/V counter triggering
 *   - Joypad auto-read controller
 *   - WRAM refresh timing
 *   - A-bus (system) and B-bus (peripheral) arbitration
 *
 * The 5A22 is packaged in a 100-pin QFP (14×14 mm, 0.65 mm pitch).
 *
 * NOTE: Pin numbering below is reconstructed from SNES PCB analysis and
 * reverse-engineering documentation (fullsnes.txt, SNES Dev Wiki). Signal
 * names are well-verified; exact QFP pin numbers may have minor deviations
 * from silicon reality on a handful of power/NC assignments.
 */

#include "chip/cpu/fam65xx/fam65xx.hpp"
#ifdef CERMU_HAS_GUI
#include "chip/cpu/fam65xx/fam65xx_pin_layout.h"
#include "core/system_lines.h"
#endif

namespace fam65xx {

// Ricoh 5A22 — SNES CPU (65C816-based, integrated DMA/HDMA)
inline constexpr CPUTraits RICOH_5A22Traits = {
    "Ricoh",                                                // vendor
    "5A22",                                                 // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::C816_16BIT,  // core_flags
    24,                                                     // address_bits
    0x00,                                                   // io_port_mask
    BankingType::NONE,                                      // banking
    {SoundChip::NONE, DMAController::RICOH_5A22_DMA, false} // peripheral
};

using RICOH_5A22 = fam65xx_t<RICOH_5A22Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using RICOH_5A22 = fam65xx::RICOH_5A22;

#ifdef CERMU_HAS_GUI
// ============================================================================
// RICOH 5A22 PIN LAYOUT (100-pin QFP, SNES S-CPU)
// ============================================================================
// Signal groups and their SNES-schematic names:
//   A-bus address : A0-A23   (24 pins — directly drives ROM/SRAM/WRAM)
//   A-bus data    : D0-D7    (8 pins  — shared system data bus)
//   B-bus address : PA0-PA7  (8 pins  — PPU/APU regs $2100-$21FF, CPU I/O $4200-$43FF)
//   A-bus control : /RD, /WR (2 pins  — active-low read/write to A-bus)
//   B-bus control : /PARD, /PAWR (2 pins — active-low read/write to B-bus)
//   Chip selects  : /WRAM, /ROMSEL (2 pins — active-low)
//   Clocks        : SYSCLK (21.477 MHz in), CPUCLK (~3.58 MHz out)
//   Interrupts    : /NMI (PPU VBlank), /IRQ (H/V counter / cartridge)
//   Reset         : /RES
//   Timing        : REFRESH (WRAM refresh), HBLANK, VBLANK
//   Joypad        : JOY1, JOY2 (data in), JOYCLK (clock out), JOYLAT (latch out)
//   Power         : VCC, VSS (distributed for decoupling)
//
// Pin numbering: QFP convention — pin 1 at top-left, counter-clockwise.
//   Left   : pins 1-25  (top to bottom)
//   Bottom : pins 26-50 (left to right)
//   Right  : pins 51-75 (bottom to top)
//   Top    : pins 76-100 (right to left)

inline ChipLayout create_ricoh_5a22_layout() {
  ChipLayout layout = create_qfp100_layout();

  layout.markings = {
      "S-CPU (5A22)", // part_number
      "Ricoh",        // manufacturer
      {},             // package_variant
      {},             // date_code
      {},             // lot_number
      {},             // custom_text
      true,           // show_part_number
      true,           // show_manufacturer
      false,          // show_package_variant
      false           // show_date_code
  };

  // ---- Left side: pins 1-25 (top to bottom) — address bus A0-A19 + power ----
  layout.left_pins = {
      PIN( 1, VCC),   PIN( 2, A0),    PIN( 3, A1),    PIN( 4, A2),    PIN( 5, A3),
      PIN( 6, VSS),   PIN( 7, A4),    PIN( 8, A5),    PIN( 9, A6),    PIN(10, A7),
      PIN(11, VCC),   PIN(12, A8),    PIN(13, A9),    PIN(14, A10),   PIN(15, A11),
      PIN(16, VSS),   PIN(17, A12),   PIN(18, A13),   PIN(19, A14),   PIN(20, A15),
      PIN(21, VCC),   PIN(22, A16),   PIN(23, A17),   PIN(24, A18),   PIN(25, A19),
  };

  // ---- Bottom side: pins 26-50 (left to right) — A20-A23, /RD, /WR, data, B-bus addr ----
  layout.bottom_pins = {
      PIN(26, VSS),   PIN(27, A20),   PIN(28, A21),   PIN(29, A22),   PIN(30, A23),
      PIN(31, VCC),   PIN(32, _RD),   PIN(33, _WR),   PIN(34, D0),    PIN(35, D1),
      PIN(36, VSS),   PIN(37, D2),    PIN(38, D3),    PIN(39, D4),    PIN(40, D5),
      PIN(41, VCC),   PIN(42, D6),    PIN(43, D7),    PIN(44, PA0),   PIN(45, PA1),
      PIN(46, VSS),   PIN(47, PA2),   PIN(48, PA3),   PIN(49, PA4),   PIN(50, PA5),
  };

  // ---- Right side: pins 51-75 (bottom to top) — B-bus cont, chip selects, clk, int, joypad ----
  layout.right_pins = {
      PIN(51, VCC),   PIN(52, PA6),   PIN(53, PA7),   PIN(54, _PARD), PIN(55, _PAWR),
      PIN(56, VSS),   PIN(57, _WRAM), PIN(58, _ROMSEL),PIN(59, _IRQ), PIN(60, _NMI),
      PIN(61, VCC),   PIN(62, _RES),  PIN(63, SYSCLK),PIN(64, CPUCLK),PIN(65, REFRESH),
      PIN(66, VSS),   PIN(67, HBLANK),PIN(68, VBLANK),PIN(69, JOY1),  PIN(70, JOY2),
      PIN(71, VCC),   PIN(72, JOYCLK),PIN(73, JOYLAT),PIN(74, JOYRD), PIN(75, VSS),
  };

  // ---- Top side: pins 76-100 (right to left) — power/ground distribution + NC/test ----
  layout.top_pins = {
      PIN(76, VCC),   PIN(77, NC),    PIN(78, NC),    PIN(79, NC),    PIN(80, VSS),
      PIN(81, NC),    PIN(82, NC),    PIN(83, NC),    PIN(84, VCC),   PIN(85, NC),
      PIN(86, VSS),   PIN(87, NC),    PIN(88, NC),    PIN(89, VCC),   PIN(90, NC),
      PIN(91, NC),    PIN(92, VSS),   PIN(93, NC),    PIN(94, NC),    PIN(95, VCC),
      PIN(96, NC),    PIN(97, NC),    PIN(98, VSS),   PIN(99, NC),    PIN(100, VCC),
  };

  return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::RICOH_5A22Traits>() {
  return create_ricoh_5a22_layout();
}
#endif // CERMU_HAS_GUI
