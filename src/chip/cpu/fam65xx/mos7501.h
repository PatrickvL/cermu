#pragma once
/*
 * mos7501.h — CSG 7501/8501 CPU type (separation layer)
 *
 * Includes the full fam65xx template and re-exports the CSG 7501 CPU type
 * and its traits constant outside the fam65xx namespace.  Consumer code
 * can include just this header to pull in only the CSG 7501 variant.
 */

#include "fam65xx.hpp"

// Re-export outside fam65xx namespace for convenience
using CSG7501 = fam65xx::CSG7501;
inline constexpr auto& CSG7501Traits = fam65xx::CSG7501Traits;
