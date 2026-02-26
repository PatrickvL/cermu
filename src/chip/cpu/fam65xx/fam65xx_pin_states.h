/*
 * fam65xx_pin_states.h — CPU pin state extraction
 *
 * Provides get_cpu_pin_states<Traits>() which overlays CPU-specific signal
 * directions (address-bus driven, clock direction) on top of generic
 * bus-derived pin states.
 *
 * Pin layouts are resolved via create_cpu_pin_layout<Traits>() from
 * fam65xx_pin_layout.h.  Per-CPU headers (mos6502.h, …) supply explicit
 * specializations; the caller is responsible for including those it needs.
 */

#pragma once

#include "../../../core/chip_layout.h"
#include "../../../core/system_lines.h"
#include "fam65xx.hpp"

// ============================================================================
// CPU PIN STATE EXTRACTION WITH BUS STATE
// ============================================================================

template <const fam65xx::detail::CPUTraits &Traits>
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
