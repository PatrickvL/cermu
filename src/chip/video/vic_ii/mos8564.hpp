#pragma once

#include "chip/video/vic_ii/vicii_common.hpp"

// MOS8564 NTSC VIC-IIe (C128) — NTTP instantiation with NTSC VIC-IIe traits
using mos8564_t = vicii_t<MOS8564_traits>;
