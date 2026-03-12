// =============================================================================
// io_registry.cpp — ChipRegistry registration for header-only I/O chips
// =============================================================================
//
// I/O chips without a non-GUI .cpp file need this translation unit to pull in
// their headers and register them in the ChipRegistry.
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/io/i8255.hpp"
#include "chip/io/z80_pio.hpp"
#include "chip/io/z80_ctc.hpp"
#include "chip/io/kc85_module_system.hpp"

REGISTER_CHIP_TYPE("8255",          i8255_t)
REGISTER_CHIP_TYPE("Z80 PIO",      z80_pio_t)
REGISTER_CHIP_TYPE("Z80 CTC",      z80_ctc_t)
REGISTER_CHIP_TYPE("KC85 Module System", kc85_module_system_t)
