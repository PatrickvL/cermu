#pragma once
/**
 * FDS Format Handler — Famicom Disk System disk images
 *
 * Supports both headered (.fds with "FDS\x1a" 16-byte header) and
 * raw/headerless FDS disk images.  Each disk side is 65500 bytes.
 *
 * FDS disk block structure:
 *   Block 1 ($01): Disk info      — 56 bytes (manufacturer, game name, etc.)
 *   Block 2 ($02): File amount    — 2 bytes (number of files on this side)
 *   Block 3 ($03): File header    — 16 bytes per file (ID, name, addr, size)
 *   Block 4 ($04): File data      — variable length payload
 *   (blocks 3+4 repeat for each file)
 */

#include "core/formats/format_handler.hpp"

// ============================================================================
// FDS Constants
// ============================================================================

#define FDS_HEADER_SIZE         16      /**< "FDS\x1a" + disk side count + padding */
#define FDS_SIDE_SIZE           65500   /**< Bytes per disk side */
#define FDS_MAX_SIDES           4       /**< Maximum disk sides (most games: 1-2) */
#define FDS_BIOS_SIZE           8192    /**< FDS BIOS ROM size (disksys.rom) */

// ============================================================================
// FDS Metadata — stored in format_load_result_t::metadata
// ============================================================================

/** Metadata passed from the format handler to the system layer. */
struct fds_metadata_t {
    uint8_t  num_sides;                 /**< Number of disk sides */
    char     game_name[16];             /**< Game name from disk info block */
};

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t FDS_FORMAT_DESCRIPTOR;
