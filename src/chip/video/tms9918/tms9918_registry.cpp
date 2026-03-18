// =============================================================================
// tms9918_registry.cpp — ChipRegistry registration for TMS9918 VDP family
// =============================================================================
//
// Explicit template instantiation + registration for all TMS9918 family
// variants. Each variant header pulls in the full template; including them
// here ensures the linker sees the instantiated symbols.
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

// All variant headers (each defines inline constexpr traits + using alias)
#include "chip/video/tms9918/tms9918_original.hpp"
#include "chip/video/tms9918/tms9918a.hpp"
#include "chip/video/tms9918/tms9928a.hpp"
#include "chip/video/tms9918/tms9929.hpp"
#include "chip/video/tms9918/tms9929a.hpp"
#include "chip/video/tms9918/v9938.hpp"
#include "chip/video/tms9918/v9958.hpp"
#include "chip/video/tms9918/sega_315_5124.hpp"
#include "chip/video/tms9918/sega_315_5246.hpp"

// --- Base TMS9918 variants ---
REGISTER_CHIP_TYPE("TMS9918",      TMS9918)
REGISTER_CHIP_TYPE("TMS9918A",     TMS9918A)
REGISTER_CHIP_TYPE("TMS9928A",     TMS9928A)
REGISTER_CHIP_TYPE("TMS9929",      TMS9929)
REGISTER_CHIP_TYPE("TMS9929A",     TMS9929A)

// --- Yamaha extensions ---
REGISTER_CHIP_TYPE("V9938",        V9938)
REGISTER_CHIP_TYPE("V9958",        V9958)

// --- Sega variants ---
REGISTER_CHIP_TYPE("315-5124",     SEGA_315_5124)
REGISTER_CHIP_TYPE("315-5246",     SEGA_315_5246)
