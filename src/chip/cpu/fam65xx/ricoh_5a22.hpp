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
#include "chip/cpu/fam65xx/fam65xx_pin_layout.hpp"
#include "core/system_lines.hpp"
#endif

namespace fam65xx {

// Ricoh 5A22 — SNES CPU (65C816-based, integrated DMA/HDMA)
inline constexpr CPUTraits RICOH_5A22Traits = {
    "Ricoh",                                                // vendor
    "5A22",                                                 // chip_id
    "Ricoh 5A22",                                           // display_name
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

  // ---- Left side: pins 1-25 (top to bottom) — address bus A0-A19 + power ----
  layout.left_pins = {
      CHIP_PIN( 1, VCC),   CHIP_PIN( 2, A0),    CHIP_PIN( 3, A1),    CHIP_PIN( 4, A2),    CHIP_PIN( 5, A3),
      CHIP_PIN( 6, VSS),   CHIP_PIN( 7, A4),    CHIP_PIN( 8, A5),    CHIP_PIN( 9, A6),    CHIP_PIN(10, A7),
      CHIP_PIN(11, VCC),   CHIP_PIN(12, A8),    CHIP_PIN(13, A9),    CHIP_PIN(14, A10),   CHIP_PIN(15, A11),
      CHIP_PIN(16, VSS),   CHIP_PIN(17, A12),   CHIP_PIN(18, A13),   CHIP_PIN(19, A14),   CHIP_PIN(20, A15),
      CHIP_PIN(21, VCC),   CHIP_PIN(22, A16),   CHIP_PIN(23, A17),   CHIP_PIN(24, A18),   CHIP_PIN(25, A19),
  };

  // ---- Bottom side: pins 26-50 (left to right) — A20-A23, /RD, /WR, data, B-bus addr ----
  layout.bottom_pins = {
      CHIP_PIN(26, VSS),   CHIP_PIN(27, A20),   CHIP_PIN(28, A21),   CHIP_PIN(29, A22),   CHIP_PIN(30, A23),
      CHIP_PIN(31, VCC),   CHIP_PIN(32, _RD),   CHIP_PIN(33, _WR),   CHIP_PIN(34, D0),    CHIP_PIN(35, D1),
      CHIP_PIN(36, VSS),   CHIP_PIN(37, D2),    CHIP_PIN(38, D3),    CHIP_PIN(39, D4),    CHIP_PIN(40, D5),
      CHIP_PIN(41, VCC),   CHIP_PIN(42, D6),    CHIP_PIN(43, D7),    CHIP_PIN(44, PA0),   CHIP_PIN(45, PA1),
      CHIP_PIN(46, VSS),   CHIP_PIN(47, PA2),   CHIP_PIN(48, PA3),   CHIP_PIN(49, PA4),   CHIP_PIN(50, PA5),
  };

  // ---- Right side: pins 51-75 (bottom to top) — B-bus cont, chip selects, clk, int, joypad ----
  layout.right_pins = {
      CHIP_PIN(51, VCC),   CHIP_PIN(52, PA6),   CHIP_PIN(53, PA7),   CHIP_PIN(54, _PARD), CHIP_PIN(55, _PAWR),
      CHIP_PIN(56, VSS),   CHIP_PIN(57, _WRAM), CHIP_PIN(58, _ROMSEL),CHIP_PIN(59, _IRQ), CHIP_PIN(60, _NMI),
      CHIP_PIN(61, VCC),   CHIP_PIN(62, _RES),  CHIP_PIN(63, SYSCLK),CHIP_PIN(64, CPUCLK),CHIP_PIN(65, REFRESH),
      CHIP_PIN(66, VSS),   CHIP_PIN(67, HBLANK),CHIP_PIN(68, VBLANK),CHIP_PIN(69, JOY1),  CHIP_PIN(70, JOY2),
      CHIP_PIN(71, VCC),   CHIP_PIN(72, JOYCLK),CHIP_PIN(73, JOYLAT),CHIP_PIN(74, JOYRD), CHIP_PIN(75, VSS),
  };

  // ---- Top side: pins 76-100 (right to left) — power/ground distribution + NC/test ----
  layout.top_pins = {
      CHIP_PIN(76, VCC),   CHIP_PIN(77, NC),    CHIP_PIN(78, NC),    CHIP_PIN(79, NC),    CHIP_PIN(80, VSS),
      CHIP_PIN(81, NC),    CHIP_PIN(82, NC),    CHIP_PIN(83, NC),    CHIP_PIN(84, VCC),   CHIP_PIN(85, NC),
      CHIP_PIN(86, VSS),   CHIP_PIN(87, NC),    CHIP_PIN(88, NC),    CHIP_PIN(89, VCC),   CHIP_PIN(90, NC),
      CHIP_PIN(91, NC),    CHIP_PIN(92, VSS),   CHIP_PIN(93, NC),    CHIP_PIN(94, NC),    CHIP_PIN(95, VCC),
      CHIP_PIN(96, NC),    CHIP_PIN(97, NC),    CHIP_PIN(98, VSS),   CHIP_PIN(99, NC),    CHIP_PIN(100, VCC),
  };

  return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::RICOH_5A22Traits>() {
  return create_ricoh_5a22_layout();
}
#endif // CERMU_HAS_GUI
