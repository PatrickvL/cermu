#pragma once
/*
 * mos6502.h — MOS 6502 CPU type (separation layer)
 *
 * Includes the full fam65xx template and re-exports the MOS 6502 CPU type
 * and its traits constant outside the fam65xx namespace.  Consumer code
 * can include just this header to pull in only the MOS 6502 variant.
 */

#include "fam65xx.hpp"

// Re-export outside fam65xx namespace for convenience
using MOS6502 = fam65xx::MOS6502;
inline constexpr auto& MOS6502Traits = fam65xx::MOS6502Traits;
