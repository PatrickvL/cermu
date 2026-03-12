// =============================================================================
// io_registry.cpp — ChipRegistry registration for header-only I/O chips
// =============================================================================
//
// I/O chips without a non-GUI .cpp file need this translation unit to pull in
// their headers and register them in the ChipRegistry.
// =============================================================================

#include "../../core/chip_manifest.hpp"
#include "../../core/chip_registry.h"

#include "i8255.h"
#include "z80_pio.h"
#include "z80_ctc.h"
#include "kc85_module_system.h"

REGISTER_CHIP_TYPE("8255",          i8255_t)
REGISTER_CHIP_TYPE("Z80 PIO",      z80_pio_t)
REGISTER_CHIP_TYPE("Z80 CTC",      z80_ctc_t)
REGISTER_CHIP_TYPE("Module System", kc85_module_system_t)
