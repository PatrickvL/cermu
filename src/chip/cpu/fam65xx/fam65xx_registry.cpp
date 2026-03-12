// =============================================================================
// fam65xx_registry.cpp — ChipRegistry registration for 6502-family CPUs
// =============================================================================
//
// All fam65xx CPU variants are header-only templates.  This translation unit
// forces instantiation and registers each variant in the ChipRegistry so
// that file-driven system declarations can resolve them by name.
//
// Registration names match the chip_id field in each variant's CPUTraits.
// =============================================================================

#include "../../../core/chip_manifest.hpp"
#include "../../../core/chip_registry.h"

// 6502 family — NMOS
#include "mos6502.h"
#include "mos6510.h"
#include "mos6507.h"
#include "mos7501.h"
#include "ricoh_2a03.h"

// 6502 family — CMOS
#include "wdc65c02.h"
#include "wdc_w65c02s.h"
#include "wdc65c816.h"
#include "rockwell65c02.h"
#include "synertek65c02.h"

REGISTER_CHIP_TYPE("MOS6502",        MOS6502)
REGISTER_CHIP_TYPE("MOS6510",        MOS6510)
REGISTER_CHIP_TYPE("MOS6507",        MOS6507)
REGISTER_CHIP_TYPE("CSG7501",        CSG7501)
REGISTER_CHIP_TYPE("RICOH_2A03",     RICOH_2A03)
REGISTER_CHIP_TYPE("WDC_65C02",      WDC_65C02)
REGISTER_CHIP_TYPE("WDC_W65C02S",    WDC_W65C02S)
REGISTER_CHIP_TYPE("WDC_65C816",     WDC_65C816)
REGISTER_CHIP_TYPE("ROCKWELL_R65C02", ROCKWELL_R65C02)
REGISTER_CHIP_TYPE("SYNERTEK_65C02", SYNERTEK_65C02)

// NOTE: MOS6509, CSG8502, RICOH_5A22 are omitted — their GUI virtual
// methods are not yet instantiated in fam65xx_gui.cpp (no system uses
// them yet).  Register them when their systems are added.
