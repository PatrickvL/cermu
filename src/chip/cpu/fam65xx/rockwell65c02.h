#pragma once
/*
 * rockwell65c02.h — Rockwell R65C02 CPU type (separation layer)
 *
 * Includes the full fam65xx template and re-exports the Rockwell R65C02 CPU
 * type and its traits constant outside the fam65xx namespace.  Consumer code
 * can include just this header to pull in only the Rockwell R65C02 variant.
 */

#include "fam65xx.hpp"

// Re-export outside fam65xx namespace for convenience
using ROCKWELL_R65C02 = fam65xx::ROCKWELL_R65C02;
inline constexpr auto& ROCKWELL_R65C02Traits = fam65xx::ROCKWELL_R65C02Traits;
