// =============================================================================
// sound_registry.cpp — ChipRegistry registration for header-only sound chips
// =============================================================================
//
// Sound chips without a non-GUI .cpp file need this translation unit to pull in
// their headers and register them in the ChipRegistry.
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/sound/sn76489/sn76489.hpp"
#include "chip/sound/ay_3_8910.hpp"

REGISTER_CHIP_TYPE("SN76489", sn76489_t)
REGISTER_CHIP_TYPE("AY-3-8910", ay_3_8910_t)
