#pragma once

/**
 * SID Format Handler — PSID/RSID Music File Support
 *
 * The SID file format (PSID v1–v4, RSID v2–v4) is the standard container
 * for C64 music ripped from games/demos.  The file consists of a big-endian
 * header followed by the raw C64 payload (the player/music data).
 *
 * Header layout (all multi-byte fields are BIG-ENDIAN):
 *   $00  magic       4 bytes   "PSID" or "RSID"
 *   $04  version     uint16    1..4
 *   $06  data_offset uint16    offset from file start to C64 payload
 *   $08  load_addr   uint16    C64 load address (0 = first 2 bytes of payload)
 *   $0A  init_addr   uint16    address of init routine
 *   $0C  play_addr   uint16    address of play routine (0 = uses own IRQ)
 *   $0E  songs       uint16    number of subtunes
 *   $10  start_song  uint16    default subtune (1-based)
 *   $12  speed       uint32    speed flags (bit per subtune: 0=VBI, 1=CIA)
 *   $16  name        32 bytes  null-terminated ASCII
 *   $36  author      32 bytes  null-terminated ASCII
 *   $56  released    32 bytes  null-terminated ASCII
 *   --- version 2+ only ---
 *   $76  flags       uint16    SID model, video standard, PSID-specific
 *   $78  start_page  uint8     driver relocation page ($00 = default)
 *   $79  page_length uint8     number of relocation pages
 *   $7A  second_sid  uint8     second SID address (v3+: $D0, $D4, $D5...$DE)
 *   $7B  third_sid   uint8     third SID address (v4+)
 *
 * Flags ($76) bit layout:
 *   bits 1..0  — MUS data (PSID only): 0=built-in, 1=Compute!'s MUS
 *   bit  2     — PSID-specific: 0=C64 compatible, 1=PlaySID specific
 *   bits 5..4  — video standard: 0=unknown, 1=PAL, 2=NTSC, 3=PAL+NTSC
 *   bits 7..6  — SID model: 0=unknown, 1=6581, 2=8580, 3=6581+8580
 *   bits 9..8  — second SID model (v3+)
 *   bits 11..10 — third SID model (v4+)
 *
 * The format handler returns FORMAT_LOAD_PROGRAM with the C64 payload as
 * program data, plus the parsed SID header in the metadata blob. The C64
 * system wrapper detects this metadata and injects a 6502 player stub
 * instead of the standard BASIC auto-run sequence.
 *
 * References:
 *   - https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/SID_file_format.txt
 *   - HVSC (High Voltage SID Collection) — 50,000+ test cases
 */

#include "format_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SID Header — Parsed representation
// ============================================================================

/** SID file type: PSID (clean init/play) or RSID (needs full C64 environment) */
typedef enum {
    SID_TYPE_PSID = 0,   /**< PlaySID compatible — can use simple JSR init/play */
    SID_TYPE_RSID = 1    /**< RealSID — needs full KERNAL, CIA, VIC-II environment */
} sid_type_t;

/** Video standard hint from v2+ flags */
typedef enum {
    SID_VIDEO_UNKNOWN = 0,
    SID_VIDEO_PAL     = 1,
    SID_VIDEO_NTSC    = 2,
    SID_VIDEO_BOTH    = 3
} sid_video_t;

/** SID chip model hint from v2+ flags */
typedef enum {
    SID_MODEL_UNKNOWN = 0,
    SID_MODEL_6581    = 1,
    SID_MODEL_8580    = 2,
    SID_MODEL_BOTH    = 3
} sid_model_t;

/**
 * Parsed SID file header.
 * All addresses are ready-to-use (load_addr resolved from payload if zero).
 *
 * This struct is stored in the format_load_result_t metadata[] blob so the
 * C64 wrapper can extract it without SID-specific coupling in the load chain.
 */
typedef struct {
    sid_type_t  type;           /**< PSID or RSID */
    uint16_t    version;        /**< Header version (1..4) */
    uint16_t    data_offset;    /**< Byte offset to C64 payload in original file */

    uint16_t    load_addr;      /**< Resolved C64 load address */
    uint16_t    init_addr;      /**< Init routine entry point */
    uint16_t    play_addr;      /**< Play routine (0 = tune sets own IRQ) */

    uint16_t    num_songs;      /**< Number of subtunes */
    uint16_t    start_song;     /**< Default subtune (1-based) */
    uint32_t    speed_flags;    /**< Bit per subtune: 0=VBI, 1=CIA timer */

    char        name[33];       /**< Tune name (null-terminated) */
    char        author[33];     /**< Author name (null-terminated) */
    char        released[33];   /**< Release info (null-terminated) */

    /* Version 2+ fields (zero if version < 2) */
    uint16_t    flags;          /**< Raw flags word */
    sid_video_t video;          /**< Video standard hint */
    sid_model_t sid_model;      /**< Primary SID model hint */
    sid_model_t sid2_model;     /**< Second SID model (v3+) */
    sid_model_t sid3_model;     /**< Third SID model (v4+) */
    uint8_t     start_page;     /**< Driver relocation start page */
    uint8_t     page_length;    /**< Driver relocation length */
    uint8_t     second_sid_addr; /**< Second SID I/O address (v3+) */
    uint8_t     third_sid_addr;  /**< Third SID I/O address (v4+) */
} sid_header_t;

// ============================================================================
// SID Format API
// ============================================================================

/**
 * Parse a SID file header from raw file data.
 * Validates magic, reads all fields, resolves load_addr from payload if zero.
 * @param data      Raw file contents (at least data_offset bytes)
 * @param size      Total file size
 * @param out       Receives parsed header
 * @return true on success, false if not a valid SID file
 */
bool sid_parse_header(const uint8_t* data, size_t size, sid_header_t* out);

/**
 * Check if a format_load_result_t contains SID metadata.
 * @return Pointer to the sid_header_t in the metadata blob, or NULL.
 */
const sid_header_t* sid_get_metadata(const format_load_result_t* result);

// ============================================================================
// SID Metadata Tag — used to identify the metadata blob type
// ============================================================================

/** Magic tag stored at the beginning of the metadata blob to identify SID data.
 *  This is NOT part of the SID file — it's our internal tag for the metadata blob. */
#define SID_METADATA_TAG  0x53494448  /* "SIDH" in big-endian */

typedef struct {
    uint32_t    tag;        /**< Must be SID_METADATA_TAG */
    sid_header_t header;    /**< Parsed SID header */
} sid_metadata_blob_t;

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t SID_FORMAT_DESCRIPTOR;

#ifdef __cplusplus
}
#endif
