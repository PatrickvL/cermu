#pragma once

/**
 * TAP Format Handler — Raw Tape Pulse Data
 *
 * Parses TAP file headers for identification and metadata.
 * TAP files contain raw pulse-width data; full decoding requires
 * cycle-accurate tape emulation (not handled here).
 */

#include "core/formats/format_handler.h"
// ============================================================================
// TAP Types
// ============================================================================

/** TAP file header (first 20 bytes) */
struct commodore_tap_header_t {
    char     signature[12];         /**< "C64-TAPE-RAW" or "C16-TAPE-RAW" */
    uint8_t  version;               /**< 0 or 1 */
    uint8_t  platform;              /**< 0=C64, 1=VIC-20, 2=C16 */
    uint8_t  video_standard;        /**< 0=PAL, 1=NTSC */
    uint8_t  reserved;
    uint32_t data_size;             /**< Size of pulse data */
};

// ============================================================================
// TAP Format API
// ============================================================================

/** Read TAP header from an already-loaded buffer. */
bool commodore_tap_read_header_mem(const uint8_t* data, size_t size,
                                   commodore_tap_header_t* out_header);

/** Identify TAP platform from an already-loaded buffer.
 * @return 0=C64, 1=VIC-20, 2=C16, -1=unknown/error
 */
int commodore_tap_identify_platform_mem(const uint8_t* data, size_t size);

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t TAP_FORMAT_DESCRIPTOR;

