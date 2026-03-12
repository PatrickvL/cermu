#pragma once
/*
 * fam65xx_pin_layout.h — Primary template for CPU pin layout creation
 *
 * Declares the create_cpu_pin_layout<Traits>() template with a DIP-40
 * default.  Per-CPU headers (mos6502.h, mos6510.h, …) provide explicit
 * specializations for chips with custom pin assignments.
 *
 * This header is included by per-CPU headers that define layouts, so it
 * MUST NOT include any per-CPU header itself (that would be circular).
 */

#ifdef CERMU_HAS_GUI

#include "core/chip_layout.hpp"
#include "chip/cpu/fam65xx/fam65xx_processor_traits.hpp"

// ============================================================================
// PRIMARY TEMPLATE — generic DIP-40 fallback
// ============================================================================

/// Per-CPU headers override this via explicit specialization.
/// CPUs without a custom layout get a generic DIP-40 package with the
/// vendor and chip-id strings from their traits.
template <const fam65xx::detail::CPUTraits &Traits>
inline ChipLayout create_cpu_pin_layout() {
  ChipLayout layout = create_dip40_layout();

  layout.markings.part_number    = Traits.get_chip_id();
  layout.markings.manufacturer   = Traits.get_vendor();
  layout.markings.show_part_number   = true;
  layout.markings.show_manufacturer  = true;

  return layout;
}

#endif // CERMU_HAS_GUI
