#pragma once
/*
 * wdc65c816.h — WDC 65C816 CPU type (separation layer)
 *
 * Includes the full fam65xx template and re-exports the WDC 65C816 CPU type
 * and its traits constant outside the fam65xx namespace.  Consumer code
 * can include just this header to pull in only the WDC 65C816 variant.
 */

#include "fam65xx.hpp"

// Re-export outside fam65xx namespace for convenience
using WDC_65C816 = fam65xx::WDC_65C816;
inline constexpr auto& WDC_65C816Traits = fam65xx::WDC_65C816Traits;
