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
#include "chip/video/fam6845/mc6845.hpp"
#include "chip/video/fam6845/mos8563.hpp"
#include "chip/video/fam6845/mos8568.hpp"
#include "chip/video/dvg/dvg.hpp"
#include "chip/video/bbc_vidproc/bbc_vidproc.hpp"
#include "chip/video/spectrum_ula/ferranti_ula.hpp"
#include "chip/video/amstrad_gate_array/amstrad_gate_array.hpp"
#include "chip/video/vic_ii/mos8564.hpp"
#include "chip/video/vic_ii/mos8566.hpp"

REGISTER_CHIP_TYPE("MC6847", mc6847_t)
REGISTER_CHIP_TYPE("MC6845", mc6845_t)
REGISTER_CHIP_TYPE("MOS8563", mos8563_t)
REGISTER_CHIP_TYPE("MOS8568", mos8568_t)
REGISTER_CHIP_TYPE("DVG",    dvg_t)
REGISTER_CHIP_TYPE("Video ULA", bbc_vidproc_t)
REGISTER_CHIP_TYPE("6C001E-7",  ferranti_ula_t)
REGISTER_CHIP_TYPE("Amstrad Gate Array", amstrad_gate_array_t)
REGISTER_CHIP_TYPE("MOS8564", mos8564_t)
REGISTER_CHIP_TYPE("MOS8566", mos8566_t)
