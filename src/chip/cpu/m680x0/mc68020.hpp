#pragma once
/*
 * mc68020.hpp — Motorola MC68020 CPU Variant
 *
 * Trait definition and type alias for the MC68020:
 *   - 32-bit data bus, 32-bit address bus
 *   - Instruction cache, barrel shifter, bit field ops,
 *     CAS/CAS2, 32×32→64 multiply, 64÷32 divide
 *   - 16/25/33 MHz, 114-pin PGA
 */

#include "chip/cpu/m680x0/m680x0.hpp"

namespace m680x0 {

inline constexpr M680x0Traits MC68020Traits = {
    .vendor          = "Motorola",
    .chip_id         = "68020",
    .display_name    = "Motorola 68020",
    .core_flags      = CoreFlags::MC68020_FLAGS,
    .address_bits    = 32,
    .data_bus_bits   = 32,
    .max_clock_mhz_x10 = 330,  // 33.0 MHz
    .pin_count       = 114,
};

using MC68020 = m680x0_t<MC68020Traits>;

} // namespace m680x0

using MC68020 = m680x0::MC68020;
