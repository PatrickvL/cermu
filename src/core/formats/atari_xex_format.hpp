#pragma once
/*
 * atari_xex_format.hpp — Atari 8-bit XEX/COM/BIN executable format
 *
 * Atari executables use segmented loading: each segment has a $FFFF header,
 * start address, end address, then data.  The last loaded segment's run
 * address is determined by the RUNAD vector ($02E0-$02E1).
 */
#include "core/formats/format_handler.hpp"

extern const format_descriptor_t ATARI_XEX_FORMAT_DESCRIPTOR;
