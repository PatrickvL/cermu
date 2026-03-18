#pragma once

#include "chip/video/vic_ii/vicii_common.hpp"

// MOS6567 NTSC VIC-II — NTTP instantiation with NTSC R8 traits
using mos6567_t = vicii_t<MOS6567R8_traits>;
