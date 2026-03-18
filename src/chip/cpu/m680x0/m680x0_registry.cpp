/*
 * m680x0_registry.cpp — MC68000 family chip registration
 */

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/cpu/m680x0/mc68000.hpp"
#include "chip/cpu/m680x0/mc68010.hpp"
#include "chip/cpu/m680x0/mc68020.hpp"

REGISTER_CHIP_TYPE("MC68000", MC68000)
REGISTER_CHIP_TYPE("MC68010", MC68010)
REGISTER_CHIP_TYPE("MC68020", MC68020)
