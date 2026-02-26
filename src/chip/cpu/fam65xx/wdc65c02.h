#pragma once
/*
 * wdc65c02.h — WDC 65C02 CPU type (separation layer)
 *
 * Includes the full fam65xx template and re-exports the WDC 65C02 CPU type
 * and its traits constant outside the fam65xx namespace.  Consumer code
 * can include just this header to pull in only the WDC 65C02 variant.
 */

#include "fam65xx.hpp"

// Re-export outside fam65xx namespace for convenience
using WDC_65C02 = fam65xx::WDC_65C02;
inline constexpr auto& WDC_65C02_EARLYTraits = fam65xx::WDC_65C02_EARLYTraits;
