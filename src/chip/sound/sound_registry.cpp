// =============================================================================
// sound_registry.cpp — ChipRegistry registration for header-only sound chips
// =============================================================================
//
// Sound chips without a non-GUI .cpp file need this translation unit to pull in
// their headers and register them in the ChipRegistry.
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/sound/sn76489/sn76489.hpp"
#include "chip/sound/ay_psg/ay_3_8910.hpp"
#include "chip/sound/ay_psg/ay_3_8912.hpp"
#include "chip/sound/ay_psg/ay_3_8913.hpp"
#include "chip/sound/ay_psg/ay_3_8914.hpp"
#include "chip/sound/ay_psg/ym2149.hpp"
#include "chip/sound/ay_psg/ym3439.hpp"
#include "chip/sound/ay_psg/ay8930.hpp"
#include "chip/sound/pokey/c012294.hpp"
#include "chip/sound/pokey/c012294b.hpp"
#include "chip/sound/pokey/c014795.hpp"
#include "chip/sound/namco_wsg.hpp"
#include "chip/sound/nes_apu.hpp"
#include "chip/sound/ym_fm/ym2151.hpp"
#include "chip/sound/ym_fm/ym2203.hpp"
#include "chip/sound/ym_fm/ym2413.hpp"
#include "chip/sound/ym_fm/ym2608.hpp"
#include "chip/sound/ym_fm/ym2610.hpp"
#include "chip/sound/ym_fm/ym2612.hpp"
#include "chip/sound/ym_fm/ym3438.hpp"
#include "chip/sound/ym_fm/ym3526.hpp"
#include "chip/sound/ym_fm/ym3812.hpp"

REGISTER_CHIP_TYPE("SN76489", sn76489_t)
REGISTER_CHIP_TYPE("AY-3-8910", AY_3_8910)
REGISTER_CHIP_TYPE("AY-3-8912", AY_3_8912)
REGISTER_CHIP_TYPE("AY-3-8913", AY_3_8913)
REGISTER_CHIP_TYPE("AY-3-8914", AY_3_8914)
REGISTER_CHIP_TYPE("YM2149",    YM2149)
REGISTER_CHIP_TYPE("YM3439",    YM3439)
REGISTER_CHIP_TYPE("AY8930",    AY8930)
REGISTER_CHIP_TYPE("C012294",  pokey::C012294)
REGISTER_CHIP_TYPE("C012294B", pokey::C012294B)
REGISTER_CHIP_TYPE("C014795",  pokey::C014795)
REGISTER_CHIP_TYPE("Namco WSG", namco_wsg_t)
REGISTER_CHIP_TYPE("APU",       nes6502_apu::APU)
REGISTER_CHIP_TYPE("YM2151",    YM2151)
REGISTER_CHIP_TYPE("YM2203",    YM2203)
REGISTER_CHIP_TYPE("YM2413",    YM2413)
REGISTER_CHIP_TYPE("YM2608",    YM2608)
REGISTER_CHIP_TYPE("YM2610",    YM2610)
REGISTER_CHIP_TYPE("YM2612",    YM2612)
REGISTER_CHIP_TYPE("YM3438",    YM3438)
REGISTER_CHIP_TYPE("YM3526",    YM3526)
REGISTER_CHIP_TYPE("YM3812",    YM3812)
