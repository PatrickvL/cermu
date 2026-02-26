#pragma once
/*
 * mos6510.h — MOS 6510 CPU type (separation layer)
 *
 * Includes the full fam65xx template and re-exports the MOS 6510 CPU type
 * and its traits constant outside the fam65xx namespace.  Consumer code
 * can include just this header to pull in only the MOS 6510 variant.
 */

#include "fam65xx.hpp"

// Re-export outside fam65xx namespace for convenience
using MOS6510 = fam65xx::MOS6510;
inline constexpr auto& MOS6510Traits = fam65xx::MOS6510Traits;
