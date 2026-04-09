#pragma once

/**
 * Oric TAP Format Handler — Oric-1/Atmos Tape Files
 *
 * The Oric .tap format stores programs as they appear on tape:
 *
 *   $00  3   Sync bytes ($16 $16 $16)
 *   $03  1   Sync marker ($24 = 300 baud)
 *   $04  1   Unused ($00)
 *   $05  1   File type: $00 = BASIC, $80 = machine code
 *   $06  1   Autorun flag: $00 = no, $C7 = yes
 *   $07  2   End address + 1 (big-endian)
 *   $09  2   Start address (big-endian)
 *   $0B  1   Unused ($00)
 *   $0C  ..  Filename (NUL-terminated)
 *   ..   ..  Program data (start_addr..end_addr-1)
 *
 * Multiple programs can be concatenated within a single .tap file.
 *
 * Uses tape_common.hpp for shared loading logic.
 */

#include "core/formats/format_handler.hpp"

extern const format_descriptor_t ORIC_TAP_FORMAT_DESCRIPTOR;
