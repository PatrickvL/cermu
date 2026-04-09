#pragma once

/**
 * VZ Format Handler — VTech VZ200/VZ300 Tape Files
 *
 * The .vz format is the standard tape image for VTech Laser 110/210/310
 * (aka VZ200/VZ300) computers.
 *
 * File structure:
 *   $00  4   Magic: "VZF0" (BASIC) or "VZF1" (machine code)
 *   $04  17  Filename (NUL-padded)
 *   $15  1   File type: $F0 = BASIC, $F1 = machine code
 *   $16  2   Start address (little-endian)
 *   $18  ..  Program data (to end of file)
 *
 * Total header size: 24 bytes ($18)
 *
 * Uses tape_common.hpp for shared loading logic.
 */

#include "core/formats/format_handler.hpp"

extern const format_descriptor_t VZ_FORMAT_DESCRIPTOR;
