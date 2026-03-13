#pragma once

/**
 * TRD Format Handler — ZX Spectrum TR-DOS Raw Disk Image
 *
 * The .trd format is a sector-by-sector disk image of a TR-DOS floppy.
 * 16 sectors per track, 256 bytes per sector.  Track 0 holds the file
 * catalog (sectors 0-8) and disk info (sector 9).
 *
 * Disk types:
 *   0x16  80 tracks, double-sided  (2 × 80 × 16 × 256 = 655360 bytes)
 *   0x17  40 tracks, double-sided  (2 × 40 × 16 × 256 = 327680 bytes)
 *   0x18  80 tracks, single-sided  (80 × 16 × 256 = 327680 bytes)
 *   0x19  40 tracks, single-sided  (40 × 16 × 256 = 163840 bytes)
 *
 * File catalog: 128 entries × 16 bytes = sectors 0..7 of track 0.
 * Disk info sector: track 0, sector 9 (offset 0x08E1 = byte 2273).
 */

#include "core/formats/trdos_common.hpp"

extern const format_descriptor_t TRD_FORMAT_DESCRIPTOR;
