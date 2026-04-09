#pragma once

/**
 * SSD/DSD Format Handler — Acorn DFS Disc Images
 *
 * The .ssd (single-sided) and .dsd (double-sided) formats are raw sector
 * dumps of Acorn DFS (Disc Filing System) formatted floppy discs, as used
 * by the BBC Micro, Acorn Electron, and BBC Master.
 *
 * Disc geometry:
 *   SSD: 40 or 80 tracks × 10 sectors × 256 bytes = 100/200 KB
 *   DSD: same but interleaved (track 0 side 0, track 0 side 1, track 1 side 0, ...)
 *
 * DFS catalog (sectors 0-1 of each side):
 *   Sector 0 ($000-$0FF):
 *     $000-$007: 8 bytes per entry — filename (7 chars) + directory (1 char)
 *     Up to 31 entries ($008-$0FF), starting at $008
 *
 *   Sector 1 ($100-$1FF):
 *     $100-$103: Disc title (first 4 chars, continued from sector 0 title)
 *     $104:      Write count (BCD)
 *     $105:      Number of catalog entries × 8
 *     $106-$107: Boot option (bits 5-4 of $106) + sector count (10 bits)
 *     $108-$10F: 8 bytes per entry — load addr(2), exec addr(2), length(2), start sector(2)
 *     Up to 31 entries ($108-$1FF)
 *
 * Uses disk_image_common.hpp for shared loading logic.
 */

#include "core/formats/format_handler.hpp"

extern const format_descriptor_t SSD_FORMAT_DESCRIPTOR;
