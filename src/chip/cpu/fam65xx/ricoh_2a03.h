#pragma once
/*
 * ricoh_2a03.h — Ricoh 2A03 CPU type (separation layer)
 *
 * Includes the full fam65xx template and re-exports the Ricoh 2A03 CPU type
 * and its traits constant outside the fam65xx namespace.  Consumer code
 * can include just this header to pull in only the Ricoh 2A03 variant.
 */

#include "fam65xx.hpp"

// Re-export outside fam65xx namespace for convenience
using RICOH_2A03 = fam65xx::RICOH_2A03;
inline constexpr auto& RICOH_2A03Traits = fam65xx::RICOH_2A03Traits;
