// =============================================================================
// mmu_registry.cpp — ChipRegistry registration for MMU chips
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/mmu/mos8722.hpp"

REGISTER_CHIP_TYPE("MOS8722",      mos8722_t)
