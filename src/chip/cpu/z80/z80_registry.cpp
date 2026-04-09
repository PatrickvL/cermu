// =============================================================================
// z80_registry.cpp — ChipRegistry registration for Z80-family CPUs
// =============================================================================
//
// All Z80 CPU variants are header-only templates.  This translation unit
// forces instantiation and registers each variant in the ChipRegistry so
// that file-driven system declarations can resolve them by name.
//
// Registration names match the chip_id field in each variant's Z80Traits.
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/cpu/z80/zilog_z80.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/cpu/z80/zilog_z80b.hpp"
#include "chip/cpu/z80/u880.hpp"
#include "chip/cpu/z80/sharp_sm83.hpp"

REGISTER_CHIP_TYPE("Z80",  ZilogZ80)
REGISTER_CHIP_TYPE("Z80A", ZilogZ80A)
REGISTER_CHIP_TYPE("Z80B", ZilogZ80B)
REGISTER_CHIP_TYPE("U880", U880)
REGISTER_CHIP_TYPE("SM83", SharpSM83)
