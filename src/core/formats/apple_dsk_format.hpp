#pragma once

/**
 * Apple II DSK Format Handler — Apple DOS / ProDOS Disc Images
 *
 * Supports three Apple II disc image formats:
 *
 * **DSK** (143,360 bytes):
 *   Raw sector dump, 35 tracks × 16 sectors × 256 bytes.
 *   May contain DOS 3.3 or ProDOS filesystem.
 *   Sector ordering: DOS-order (default) or ProDOS-order.
 *
 * **NIB** (232,960 bytes):
 *   Raw nibblized track data, 35 tracks × 6656 bytes.
 *   Contains GCR-encoded data including sync bytes and address marks.
 *   Must be decoded (6-and-2) to extract sector data.
 *
 * **2MG** (variable size):
 *   Universal disk image with 64-byte header:
 *     $00  4   Magic: "2IMG"
 *     $04  4   Creator ID
 *     $08  2   Header size (always 64)
 *     $0A  2   Version (1)
 *     $0C  4   Image format: 0=DOS order, 1=ProDOS order, 2=NIB
 *     $10  4   Flags
 *     $14  4   ProDOS blocks or NIB blocks
 *     $18  4   Data offset
 *     $1C  4   Data length
 *     Followed by disc image data in the specified format.
 *
 * Uses disk_image_common.hpp for shared loading logic.
 */

#include "core/formats/format_handler.hpp"

extern const format_descriptor_t APPLE_DSK_FORMAT_DESCRIPTOR;
