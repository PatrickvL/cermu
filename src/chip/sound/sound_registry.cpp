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
#include "chip/sound/ay_psg/ay_3_8910.hpp"
#include "chip/sound/ay_psg/ay_3_8912.hpp"
#include "chip/sound/pokey/c012294.hpp"
#include "chip/sound/pokey/c012294b.hpp"
#include "chip/sound/pokey/c014795.hpp"

REGISTER_CHIP_TYPE("SN76489", sn76489_t)
REGISTER_CHIP_TYPE("AY-3-8910", AY_3_8910)
REGISTER_CHIP_TYPE("AY-3-8912", AY_3_8912)
REGISTER_CHIP_TYPE("C012294",  pokey::C012294)
REGISTER_CHIP_TYPE("C012294B", pokey::C012294B)
REGISTER_CHIP_TYPE("C014795",  pokey::C014795)
