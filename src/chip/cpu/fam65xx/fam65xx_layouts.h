/*
 * cpu_pin_layouts.h - CPU trait-specific pin layout definitions
 *
 * This header defines pin layouts for different CPU variants based on their
 * specific features and capabilities using CPU traits.
 */

#pragma once

#include "../../../core/chip_layout.h"
#include "../../../core/system_lines.h"
#include "fam65xx_processor_traits.hpp"

// Forward declaration of the CPU template class
namespace fam65xx {
template <const CPUTraits &Traits> class fam65xx_t;
}

// ============================================================================
// CPU TRAIT-BASED PIN LAYOUT FACTORY
// ============================================================================

// CPU-specific pin layout functions - using reference template parameters like
// fam65xx_t
template <const fam65xx::CPUTraits &Traits> ChipLayout create_cpu_pin_layout();

// Specific CPU layout functions
inline ChipLayout create_mos6502_layout();
inline ChipLayout create_mos6510_layout();
inline ChipLayout create_csg7501_layout();
inline ChipLayout create_wdc_w65c02s_layout();
inline ChipLayout create_wdc_65c816_layout();
inline ChipLayout create_ricoh_2a03_layout();
inline ChipLayout create_rockwell_r65c02_layout();

// CPU pin state functions - get pin states from CPU and bus state
template <const fam65xx::CPUTraits &Traits>
std::vector<PinSignalState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits> *cpu,
                                               const ChipLayout *layout,
                                               bus_state_t bus_state);

// ============================================================================
// TEMPLATE FUNCTION IMPLEMENTATIONS (must be in header for templates)
// ============================================================================

#include "fam65xx.hpp"
#include <type_traits>

// Generic CPU pin layout function with compile-time CPU selection
template <const fam65xx::CPUTraits &Traits> ChipLayout create_cpu_pin_layout() {
  ChipLayout layout = {};

  // Compare by vendor and model strings instead of types
  // MOS 6502 (NMOS) PIN LAYOUT - use create_mos6502_layout()
  if constexpr (Traits == fam65xx::MOS6502Traits) {
    layout = create_mos6502_layout();

    // MOS 6510 (C64/C128) PIN LAYOUT - use create_mos6510_layout()
  } else if constexpr (Traits == fam65xx::MOS6510Traits) {
    layout = create_mos6510_layout();

    // CSG 7501/8501 (C16/Plus4) PIN LAYOUT - use create_csg7501_layout()
  } else if constexpr (Traits == fam65xx::CSG7501Traits) {
    layout = create_csg7501_layout();

    // WDC 65C02 (CMOS) PIN LAYOUT - use create_wdc_w65c02s_layout()
  } else if constexpr (Traits == fam65xx::WDC_W65C02STraits) {
    layout = create_wdc_w65c02s_layout();

    // WDC 65C816 (16-BIT) PIN LAYOUT - use create_wdc_65c816_layout()
  } else if constexpr (Traits == fam65xx::WDC_65C816Traits) {
    layout = create_wdc_65c816_layout();

    // NES 6502 (RICOH 2A03) PIN LAYOUT - use create_ricoh_2a03_layout()
  } else if constexpr (Traits == fam65xx::RICOH_2A03Traits) {
    layout = create_ricoh_2a03_layout();

    // ROCKWELL R65C02 PIN LAYOUT - use create_rockwell_r65c02_layout()
  } else if constexpr (Traits == fam65xx::ROCKWELL_R65C02Traits) {
    layout = create_rockwell_r65c02_layout();

  } else {
    // Fallback for unknown CPU type - set fields individually
    layout.package.width = 600.0f;   // DIP-40 width
    layout.package.height = 2000.0f; // DIP-40 height
    layout.package.package_type = PackageType::DIP;
    layout.package.marker = OrientationMarker::NOTCH;
    layout.package.pin_pitch = 100.0f;
    layout.package.has_thermal_pad = false;
    layout.package.has_center_slug = false;
    layout.package.thermal_pad_size = 0.0f;

    layout.markings.part_number = "Unknown";
    layout.markings.manufacturer = "65xx Family";
  }

  return layout;
}

// ============================================================================
// CPU PIN STATE EXTRACTION WITH BUS STATE
// ============================================================================

// CPU pin state function with compile-time CPU selection
template <const fam65xx::CPUTraits &Traits>
std::vector<PinSignalState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits> *cpu,
                                               const ChipLayout *layout,
                                               bus_state_t bus_state) {
  // Start with generic bus-derived pin states
  auto states = populate_pin_states_from_bus(*layout, bus_state);

  if (!cpu) {
    // Mark all states as invalid if no CPU
    for (auto &state : states) {
      state.signal_valid = false;
    }
    return states;
  }

  // CPU-specific overlays: address pins are always driven by the CPU,
  // and clock pin direction depends on whether it's PHI0 (input) or
  // PHI1/PHI2 (output).
  auto overlay_pin = [&](const ChipPin &pin) {
    if (pin.pin_number == 0 || pin.pin_number > states.size())
      return;
    PinSignalState &state = states[pin.pin_number - 1];

    switch (pin.get_pin_type()) {
    case PinType::ADDRESS:
      state.drive_direction = true; // CPU always drives address bus
      break;
    case PinType::CLOCK:
      state.drive_direction = (pin.label != PinLabel::PHI0);
      break;
    default:
      break;
    }
  };

  for (const auto &pin : layout->left_pins)
    overlay_pin(pin);
  for (const auto &pin : layout->right_pins)
    overlay_pin(pin);
  for (const auto &pin : layout->top_pins)
    overlay_pin(pin);
  for (const auto &pin : layout->bottom_pins)
    overlay_pin(pin);
  for (const auto &pin : layout->grid_pins)
    overlay_pin(pin);

  return states;
}

// ============================================================================
// MOS 6502 SPECIFIC LAYOUT IMPLEMENTATION
// ============================================================================

inline ChipLayout create_mos6502_layout() {
  // Start with DIP-40 base layout from core system
  ChipLayout layout = create_dip40_layout();

  // Customize for MOS 6502 - assign pin labels and types
  // The create_dip40_layout() provides the physical package structure

  // Update package info for MOS 6502 - properly initialize all fields
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

inline ChipLayout create_mos6510_layout() {
  // Start with DIP-40 base layout from core system
  ChipLayout layout = create_dip40_layout();

  // Update package info for MOS 6510 - properly initialize all fields
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

// ============================================================================
// CSG 7501/8501 SPECIFIC LAYOUT IMPLEMENTATION (C16/Plus4 CPU)
// ============================================================================

inline ChipLayout create_csg7501_layout() {
  // Start with DIP-40 base layout from core system
  ChipLayout layout = create_dip40_layout();

  // Update package info for CSG 7501/8501
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

inline ChipLayout create_wdc_w65c02s_layout() {
  // Start with DIP-40 base layout from core system
  ChipLayout layout = create_dip40_layout();

  // Update package info for WDC W65C02S - properly initialize all fields
  layout.markings = {
      "W65C02S",               // part_number
      "Western Design Center", // manufacturer
      {},                 // package_variant
      {},                 // date_code
      {},                 // lot_number
      {},                 // custom_text
      true,                    // show_part_number
      true,                    // show_manufacturer
      false,                   // show_package_variant
      false                    // show_date_code
  };

  // Pin assignments for WDC W65C02S (40-pin DIP)
  PIN_LR(layout, 1, _VP, VSS, 21)    // Vector Pull (low during vector fetch)
  PIN_LR(layout, 2, RDY, A12, 22)    // Ready (bidirectional on 65C02)
  PIN_LR(layout, 3, PHI1, A13, 23)
  PIN_LR(layout, 4, _IRQ, A14, 24)   // Interrupt Request
  PIN_LR(layout, 5, _ML, A15, 25)    // Memory Lock (low during RMW)
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
  PIN_LR(layout, 16, A7, BE, 36)     // Bus Enable
  PIN_LR(layout, 17, A8, PHI0, 37)
  PIN_LR(layout, 18, A9, _SO, 38)    // Set Overflow flag
  PIN_LR(layout, 19, A10, PHI2, 39)
  PIN_LR(layout, 20, A11, _RES, 40)  // Reset

  return layout;
}

inline ChipLayout create_wdc_65c816_layout() {
  // Start with DIP-40 base layout from core system
  ChipLayout layout = create_dip40_layout();

  // Update package info for WDC 65C816 - properly initialize all fields
  layout.markings = {
      "W65C816S",              // part_number
      "Western Design Center", // manufacturer
      {},                 // package_variant
      {},                 // date_code
      {},                 // lot_number
      {},                 // custom_text
      true,                    // show_part_number
      true,                    // show_manufacturer
      false,                   // show_package_variant
      false                    // show_date_code
  };

  // Pin assignments for WDC 65C816 (40-pin DIP)
  PIN_LR(layout, 1, _VPB, VSS, 21)    // Vector Pull Bar (low during vector fetch)
  PIN_LR(layout, 2, RDY, A12, 22)     // Ready
  PIN_LR(layout, 3, _ABORT, A13, 23)  // Abort current instruction
  PIN_LR(layout, 4, _IRQ, A14, 24)    // Interrupt Request
  PIN_LR(layout, 5, _ML, A15, 25)     // Memory Lock (low during RMW)
  PIN_LR(layout, 6, _NMI, D7, 26)     // Non-Maskable Interrupt
  PIN_LR(layout, 7, VPA, D6, 27)      // Valid Program Address
  PIN_LR(layout, 8, VDD, D5, 28)
  PIN_LR(layout, 9, A0, D4, 29)
  PIN_LR(layout, 10, A1, D3, 30)
  PIN_LR(layout, 11, A2, D2, 31)
  PIN_LR(layout, 12, A3, D1, 32)
  PIN_LR(layout, 13, A4, D0, 33)
  PIN_LR(layout, 14, A5, RW, 34)      // Read/Write
  PIN_LR(layout, 15, A6, E, 35)       // Emulation mode status
  PIN_LR(layout, 16, A7, BE, 36)      // Bus Enable
  PIN_LR(layout, 17, A8, PHI0, 37)
  PIN_LR(layout, 18, A9, MX, 38)      // M/X status flags
  PIN_LR(layout, 19, A10, VDA, 39)    // Valid Data Address
  PIN_LR(layout, 20, A11, _RES, 40)   // Reset

  return layout;
}

inline ChipLayout create_ricoh_2a03_layout() {
  // Start with DIP-40 base layout from core system
  ChipLayout layout = create_dip40_layout();

  // Update package info for RICOH 2A03 (NES processor) - properly initialize
  // all fields
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

inline ChipLayout create_rockwell_r65c02_layout() {
  // Start with DIP-40 base layout from core system
  ChipLayout layout = create_dip40_layout();

  // Update package info for Rockwell R65C02 - properly initialize all fields
  layout.markings = {
      "R65C02",   // part_number
      "Rockwell", // manufacturer
      {},    // package_variant
      {},    // date_code
      {},    // lot_number
      {},    // custom_text
      true,       // show_part_number
      true,       // show_manufacturer
      false,      // show_package_variant
      false       // show_date_code
  };

  // Pin assignments for Rockwell R65C02 (40-pin DIP)
  PIN_LR(layout, 1, VSS, VSS, 21)
  PIN_LR(layout, 2, RDY, A12, 22)    // Ready
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
  PIN_LR(layout, 16, A7, BE, 36)     // Bus Enable
  PIN_LR(layout, 17, A8, PHI0, 37)
  PIN_LR(layout, 18, A9, _SO, 38)    // Set Overflow flag
  PIN_LR(layout, 19, A10, PHI2, 39)
  PIN_LR(layout, 20, A11, _RES, 40)  // Reset

  return layout;
}
