/*
 * fam65xx_layouts.h — CPU trait-based pin layout dispatch + pin state extraction
 *
 * The per-CPU wrapper headers (mos6502.h, mos6510.h, …) each define their own
 * inline create_xxx_layout() function.  This header provides:
 *   - create_cpu_pin_layout<Traits>()  — compile-time dispatch to the right one
 *   - get_cpu_pin_states<Traits>()     — overlays CPU-specific signal directions
 */

#pragma once

#include "../../../core/chip_layout.h"
#include "../../../core/system_lines.h"

// Per-CPU headers supply trait constants, type aliases, and layout functions.
// Each transitively includes fam65xx.hpp.
#include "mos6502.h"
#include "mos6510.h"
#include "mos7501.h"
#include "ricoh_2a03.h"
#include "wdc65c02.h"
#include "synertek65c02.h"
#include "rockwell65c02.h"
#include "wdc_w65c02s.h"
#include "wdc65c816.h"

#include <type_traits>

// ============================================================================
// COMPILE-TIME CPU PIN LAYOUT DISPATCH
// ============================================================================

template <const fam65xx::CPUTraits &Traits> ChipLayout create_cpu_pin_layout() {
  ChipLayout layout = {};

  // MOS 6502 (NMOS) PIN LAYOUT
  if constexpr (Traits == fam65xx::MOS6502Traits) {
    layout = create_mos6502_layout();

    // MOS 6510 (C64/C128) PIN LAYOUT
  } else if constexpr (Traits == fam65xx::MOS6510Traits) {
    layout = create_mos6510_layout();

    // CSG 7501/8501 (C16/Plus4) PIN LAYOUT
  } else if constexpr (Traits == fam65xx::CSG7501Traits) {
    layout = create_csg7501_layout();

    // WDC W65C02S (modern CMOS) PIN LAYOUT
  } else if constexpr (Traits == fam65xx::WDC_W65C02STraits) {
    layout = create_wdc_w65c02s_layout();

    // WDC 65C816 (16-BIT) PIN LAYOUT
  } else if constexpr (Traits == fam65xx::WDC_65C816Traits) {
    layout = create_wdc_65c816_layout();

    // NES 6502 (RICOH 2A03) PIN LAYOUT
  } else if constexpr (Traits == fam65xx::RICOH_2A03Traits) {
    layout = create_ricoh_2a03_layout();

    // ROCKWELL R65C02 PIN LAYOUT
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
