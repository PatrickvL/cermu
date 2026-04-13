// =============================================================================
// mmu_registry.cpp — ChipRegistry registration for MMU chips
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/mmu/mos8722.hpp"
#include "chip/mmu/gb_mbc/gb_mbc.hpp"

REGISTER_CHIP_TYPE("MOS8722",      mos8722_t)

REGISTER_CHIP_TYPE("GB_MBC_None",  GbMbcNone)
REGISTER_CHIP_TYPE("GB_MBC1",      GbMbc1)
REGISTER_CHIP_TYPE("GB_MBC2",      GbMbc2)
REGISTER_CHIP_TYPE("GB_MBC3",      GbMbc3)
REGISTER_CHIP_TYPE("GB_MBC5",      GbMbc5)
