/*
 * fam65xx_layouts.h — CPU pin layout dispatch + pin state extraction
 *
 * Per-CPU wrapper headers (mos6502.h, mos6510.h, …) each provide an explicit
 * specialization of create_cpu_pin_layout<Traits>().  The primary template
 * (with a DIP-40 fallback) lives in fam65xx_pin_layout.h.
 *
 * This header pulls in all per-CPU specializations and provides:
 *   - get_cpu_pin_states<Traits>()  — overlays CPU-specific signal directions
 */

#pragma once

#include "../../../core/chip_layout.h"
#include "../../../core/system_lines.h"
#include "fam65xx_pin_layout.h"

// Per-CPU headers supply trait constants, type aliases, layout functions,
// and create_cpu_pin_layout<> specializations.
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
