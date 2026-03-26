// =============================================================================
// logic_registry.cpp — ChipRegistry registration for header-only logic chips
// =============================================================================
//
// Logic chips without a non-GUI .cpp file need this translation unit to pull
// in their headers and register them in the ChipRegistry.
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/logic/ls259.hpp"
#include "chip/logic/pla.hpp"

REGISTER_CHIP_TYPE("74LS259", LS259)
REGISTER_CHIP_TYPE("906114-01", PLA906114)
