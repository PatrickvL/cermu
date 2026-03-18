#pragma once
/*
 * mc68010.hpp — Motorola MC68010 CPU Variant
 *
 * Trait definition and type alias for the MC68010:
 *   - 16-bit data bus, 24-bit address bus
 *   - VBR, loop mode, virtual memory support
 *   - 10/12.5 MHz, 64-pin DIP
 */

#include "chip/cpu/m680x0/m680x0.hpp"

namespace m680x0 {

inline constexpr M680x0Traits MC68010Traits = {
    .vendor          = "Motorola",
    .chip_id         = "68010",
    .core_flags      = CoreFlags::MC68010_FLAGS,
    .address_bits    = 24,
    .data_bus_bits   = 16,
    .max_clock_mhz_x10 = 125,  // 12.5 MHz
    .pin_count       = 64,
};

using MC68010 = m680x0_t<MC68010Traits>;

} // namespace m680x0

using MC68010 = m680x0::MC68010;
