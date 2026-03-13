// =============================================================================
// video_registry.cpp — ChipRegistry registration for header-only video chips
// =============================================================================
//
// Video chips without a non-GUI .cpp file need this translation unit to pull in
// their headers and register them in the ChipRegistry.
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/video/mc6847/mc6847.hpp"
#include "chip/video/mc6845/mc6845.hpp"

REGISTER_CHIP_TYPE("MC6847", mc6847_t)
REGISTER_CHIP_TYPE("MC6845", mc6845_t)
