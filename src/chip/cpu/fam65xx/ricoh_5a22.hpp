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

  // Left pins 1-25 (top→bottom) paired with Right pins 75-51 (top→bottom)
  //           Left                          Right
  PIN_LR(layout,  1, VCC,       VSS,      75);
  PIN_LR(layout,  2, A0,        JOYRD,    74);
  PIN_LR(layout,  3, A1,        JOYLAT,   73);
  PIN_LR(layout,  4, A2,        JOYCLK,   72);
  PIN_LR(layout,  5, A3,        VCC,      71);
  PIN_LR(layout,  6, VSS,       JOY2,     70);
  PIN_LR(layout,  7, A4,        JOY1,     69);
  PIN_LR(layout,  8, A5,        VBLANK,   68);
  PIN_LR(layout,  9, A6,        HBLANK,   67);
  PIN_LR(layout, 10, A7,        VSS,      66);
  PIN_LR(layout, 11, VCC,       REFRESH,  65);
  PIN_LR(layout, 12, A8,        CPUCLK,   64);
  PIN_LR(layout, 13, A9,        SYSCLK,   63);
  PIN_LR(layout, 14, A10,       _RES,     62);
  PIN_LR(layout, 15, A11,       VCC,      61);
  PIN_LR(layout, 16, VSS,       _NMI,     60);
  PIN_LR(layout, 17, A12,       _IRQ,     59);
  PIN_LR(layout, 18, A13,       _ROMSEL,  58);
  PIN_LR(layout, 19, A14,       _WRAM,    57);
  PIN_LR(layout, 20, A15,       VSS,      56);
  PIN_LR(layout, 21, VCC,       _PAWR,    55);
  PIN_LR(layout, 22, A16,       _PARD,    54);
  PIN_LR(layout, 23, A17,       PA7,      53);
  PIN_LR(layout, 24, A18,       PA6,      52);
  PIN_LR(layout, 25, A19,       VCC,      51);

  // Top pins 100-76 (left→right) paired with Bottom pins 26-50 (left→right)
  //           Top                           Bottom
  PIN_TB(layout, 100, VCC,      VSS,      26);
  PIN_TB(layout,  99, NC,       A20,      27);
  PIN_TB(layout,  98, VSS,      A21,      28);
  PIN_TB(layout,  97, NC,       A22,      29);
  PIN_TB(layout,  96, NC,       A23,      30);
  PIN_TB(layout,  95, VCC,      VCC,      31);
  PIN_TB(layout,  94, NC,       _RD,      32);
  PIN_TB(layout,  93, NC,       _WR,      33);
  PIN_TB(layout,  92, VSS,      D0,       34);
  PIN_TB(layout,  91, NC,       D1,       35);
  PIN_TB(layout,  90, NC,       VSS,      36);
  PIN_TB(layout,  89, VCC,      D2,       37);
  PIN_TB(layout,  88, NC,       D3,       38);
  PIN_TB(layout,  87, NC,       D4,       39);
  PIN_TB(layout,  86, VSS,      D5,       40);
  PIN_TB(layout,  85, NC,       VCC,      41);
  PIN_TB(layout,  84, VCC,      D6,       42);
  PIN_TB(layout,  83, NC,       D7,       43);
  PIN_TB(layout,  82, NC,       PA0,      44);
  PIN_TB(layout,  81, NC,       PA1,      45);
  PIN_TB(layout,  80, VSS,      VSS,      46);
  PIN_TB(layout,  79, NC,       PA2,      47);
  PIN_TB(layout,  78, NC,       PA3,      48);
  PIN_TB(layout,  77, NC,       PA4,      49);
  PIN_TB(layout,  76, VCC,      PA5,      50);

  return layout;
}

template<> inline ChipLayout create_cpu_pin_layout<fam65xx::RICOH_5A22Traits>() {
  return create_ricoh_5a22_layout();
}
#endif // CERMU_HAS_GUI
