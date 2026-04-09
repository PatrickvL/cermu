#pragma once

/**
 * KC TAP Format Handler — KC85 Tape Files
 *
 * The KC85 .tap format (also called K7 or KCC) stores programs as blocks:
 *
 * **KCC format** (headerless):
 *   $00  1   Block number (starts at $01)
 *            $00 = header block, $01-$FE = data blocks, $FF = end block
 *   $01  128 Block data
 *   $82  1   Checksum (sum of bytes $00-$81)
 *
 * **KC TAP format** (with header):
 *   The first block ($00) is a header block containing:
 *     $01  8   Filename (space-padded)
 *     $09  1   File type byte
 *              $C3 = machine code (COM), $D3 = BASIC
 *     $0A  2   Reserved
 *     $0C  2   Reserved
 *     $0E  1   Number of arguments
 *   Following blocks contain file metadata (load/end/exec addr)
 *   then data blocks.
 *
 *   For COM files ($C3 type):
 *     Block 1: $01 + load_addr(2) + end_addr(2) + exec_addr(2) + ...
 *     Blocks 2+: pure data
 *
 * These formats are used by the KC85/2, KC85/3, KC85/4, KC87, and Z9001.
 *
 * Uses tape_common.hpp for shared loading logic.
 */

#include "core/formats/format_handler.hpp"

extern const format_descriptor_t KC_TAP_FORMAT_DESCRIPTOR;
