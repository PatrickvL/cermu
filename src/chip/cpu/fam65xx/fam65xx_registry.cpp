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

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

// 6502 family — NMOS
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/cpu/fam65xx/mos6510.hpp"
#include "chip/cpu/fam65xx/mos6507.hpp"
#include "chip/cpu/fam65xx/mos7501.hpp"
#include "chip/cpu/fam65xx/ricoh_2a03.hpp"
#include "chip/cpu/fam65xx/csg8502.hpp"

// 6502 family — CMOS (extended)
#include "chip/cpu/fam65xx/hudson_huc6280.hpp"

// 6502 family — NMOS (pin-reduced / banking variants)
#include "chip/cpu/fam65xx/mos6504.hpp"
#include "chip/cpu/fam65xx/mos6509.hpp"

// 6502 family — CMOS
#include "chip/cpu/fam65xx/wdc65c02.hpp"
#include "chip/cpu/fam65xx/wdc_w65c02s.hpp"
#include "chip/cpu/fam65xx/wdc65c816.hpp"
#include "chip/cpu/fam65xx/ricoh_5a22.hpp"
#include "chip/cpu/fam65xx/rockwell65c02.hpp"
#include "chip/cpu/fam65xx/synertek65c02.hpp"

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

REGISTER_CHIP_TYPE("MOS6504",        MOS6504)
REGISTER_CHIP_TYPE("MOS6509",        MOS6509)
REGISTER_CHIP_TYPE("RICOH_5A22",     RICOH_5A22)
REGISTER_CHIP_TYPE("CSG8502",         CSG8502)

REGISTER_CHIP_TYPE("HuC6280",         HUDSON_HUC6280)
