#pragma once
/*
 * mc68000.hpp — Motorola MC68000 CPU Variant
 *
 * Trait definition and type alias for the original MC68000:
 *   - 16-bit data bus, 24-bit address bus (23 external lines + A0 implicit)
 *   - 8/16 MHz, 64-pin DIP
 */

#include "chip/cpu/m680x0/m680x0.hpp"

namespace m680x0 {

inline constexpr M680x0Traits MC68000Traits = {
    .vendor          = "Motorola",
    .chip_id         = "68000",
    .core_flags      = CoreFlags::MC68000_FLAGS,
    .address_bits    = 24,
    .data_bus_bits   = 16,
    .max_clock_mhz_x10 = 80,   // 8.0 MHz
    .pin_count       = 64,
};

using MC68000 = m680x0_t<MC68000Traits>;

} // namespace m680x0

using MC68000 = m680x0::MC68000;
