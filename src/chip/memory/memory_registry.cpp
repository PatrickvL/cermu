// =============================================================================
// memory_registry.cpp — ChipRegistry registration for header-only memory chips
// =============================================================================
//
// Memory chips without a non-GUI .cpp file need this translation unit to pull
// in their headers and register them in the ChipRegistry.
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"
#include "chip/memory/er2055.hpp"

REGISTER_CHIP_TYPE("RAMChip", RAMChip)
REGISTER_CHIP_TYPE("ROMChip", ROMChip)
REGISTER_CHIP_TYPE("ER2055",  ER2055)
