#pragma once

/**
 * MSX CAS Format Handler — MSX Cassette Files
 *
 * The .cas format stores MSX cassette data as a sequence of blocks,
 * each preceded by an 8-byte header signature.
 *
 * Block types (identified by the 10-byte header following the signature):
 *   - Binary header:  $EA $EA $EA $EA $EA $EA $EA $EA $EA $EA
 *     Followed by: start_addr(2), end_addr(2), exec_addr(2), then data
 *
 *   - BASIC header:   $D3 $D3 $D3 $D3 $D3 $D3 $D3 $D3 $D3 $D3
 *     Followed by: filename(6), then data blocks
 *
 *   - ASCII header:   $EA $EA $EA $EA $EA $EA $EA $EA $EA $EA
 *     (Distinguished by context — ASCII data follows in 256-byte blocks)
 *
 * CAS file signature (repeats before each logical block):
 *   8 bytes: $1F $A6 $DE $BA $CC $13 $7D $74
 *
 * Uses tape_common.hpp for shared loading logic.
 */

#include "core/formats/format_handler.hpp"

extern const format_descriptor_t MSX_CAS_FORMAT_DESCRIPTOR;
