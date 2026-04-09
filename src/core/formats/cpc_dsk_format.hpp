#pragma once

/**
 * CPC DSK Format Handler — CPCEMU / Extended Disc Images
 *
 * The .dsk format for Amstrad CPC uses two variants:
 *
 * **Standard (CPCEMU) format:**
 *   Magic: "MV - CPCEMU Disk-File\r\nDisk-Info\r\n"
 *   256-byte disc info header (tracks, sides, track size)
 *   Track info blocks with sector data
 *
 * **Extended format:**
 *   Magic: "EXTENDED CPC DSK File\r\nDisk-Info\r\n"
 *   256-byte disc info header with per-track size table
 *   Variable-length track info blocks
 *
 * Tracks contain sector info records + sector data.
 * Sector sizes are typically 512 bytes (size code 2).
 *
 * File extraction requires AMSDOS header parsing:
 *   AMSDOS header (128 bytes at start of each file):
 *     $00  1    User number
 *     $01  8+3  Filename (8.3 format)
 *     $15  2    File type ($00=BASIC, $01=protected, $02=binary)
 *     $18  2    Load address (little-endian)
 *     $1A  1    First block number
 *     $1B  2    Logical length (little-endian)
 *     $40  2    File length (little-endian)
 *     $43  2    Checksum ($00-$42)
 *
 * Uses disk_image_common.hpp for shared loading logic.
 */

#include "core/formats/format_handler.hpp"

extern const format_descriptor_t CPC_DSK_FORMAT_DESCRIPTOR;
