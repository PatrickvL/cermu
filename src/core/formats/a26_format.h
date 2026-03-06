#pragma once

/**
 * A26 Format Handler — Atari 2600 ROM Image
 *
 * The .a26 extension is the standard naming convention for Atari 2600 ROM
 * dumps. These are headerless raw binary images — the file IS the ROM data,
 * with no metadata. Banking scheme must be auto-detected from ROM size and
 * content analysis (handled by the A2600 system's mapper factory).
 *
 * Typical sizes: 2KB, 4KB, 8KB, 12KB, 16KB, 32KB (up to 512KB for
 * Tigervision 3F carts).
 *
 * This handler provides identification only — the Atari 2600 system
 * loads cartridge data directly, like the NES/iNES handler.
 */

#include "format_handler.h"

extern const format_descriptor_t A26_FORMAT_DESCRIPTOR;
