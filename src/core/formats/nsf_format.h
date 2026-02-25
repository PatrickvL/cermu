#pragma once
/**
 * NSF (NES Sound Format) File Parser
 *
 * Parses NSF / NSFe music files for the NES/Famicom.  The NSF header is
 * a 128-byte fixed structure containing metadata, load/init/play addresses,
 * bankswitch setup, and playback speed information.
 *
 * Reference: https://www.nesdev.org/wiki/NSF
 */

#include "format_handler.h"
#include <cstdint>

// ============================================================================
// NSF HEADER TYPES
// ============================================================================

/** PAL/NTSC region flags (byte 0x7A) */
enum nsf_region_t : uint8_t {
    NSF_REGION_NTSC   = 0,   ///< NTSC only
    NSF_REGION_PAL    = 1,   ///< PAL only
    NSF_REGION_DUAL   = 2,   ///< Dual-compatible
};

/** Extra sound chip flags (byte 0x7B) */
enum nsf_chip_flags_t : uint8_t {
    NSF_CHIP_NONE      = 0x00,
    NSF_CHIP_VRC6      = 0x01,   ///< Konami VRC6
    NSF_CHIP_VRC7      = 0x02,   ///< Konami VRC7
    NSF_CHIP_FDS       = 0x04,   ///< Famicom Disk System
    NSF_CHIP_MMC5      = 0x08,   ///< Nintendo MMC5
    NSF_CHIP_NAMCO163  = 0x10,   ///< Namco 163
    NSF_CHIP_SUNSOFT5B = 0x20,   ///< Sunsoft 5B
};

/**
 * Parsed NSF file header (128 bytes on disk).
 *
 * All multi-byte fields are little-endian in the file.
 */
struct nsf_header_t {
    // Identity
    uint8_t  magic[5];          ///< "NESM\x1A"
    uint8_t  version;           ///< Format version (usually 1)

    // Song info
    uint8_t  num_songs;         ///< Total number of songs (1-based)
    uint8_t  start_song;        ///< Starting song number (1-based)

    // Addresses
    uint16_t load_addr;         ///< Where to load NSF data in CPU address space
    uint16_t init_addr;         ///< Address of init routine
    uint16_t play_addr;         ///< Address of play routine

    // Metadata strings (null-terminated, padded, UTF-8 after parsing)
    char     name[65];          ///< Song title (UTF-8)
    char     artist[65];        ///< Artist name (UTF-8)
    char     copyright[65];     ///< Copyright string (UTF-8)

    // Raw metadata — original bytes from file (ASCII/Latin-1) for NES display
    char     name_raw[33];      ///< Song title — original bytes from file
    char     artist_raw[33];    ///< Artist name — original bytes from file
    char     copyright_raw[33]; ///< Copyright string — original bytes from file

    // Timing
    uint16_t ntsc_speed;        ///< NTSC play speed in microseconds (usually 16666 = 60Hz)
    uint16_t pal_speed;         ///< PAL play speed in microseconds (usually 20000 = 50Hz)

    // Bankswitch init values ($5FF8-$5FFF)
    uint8_t  bankswitch[8];     ///< Initial bank values (all zero = no bankswitching)

    // Flags
    uint8_t  region_flags;      ///< PAL/NTSC flags
    uint8_t  chip_flags;        ///< Extra sound chip flags

    // Reserved
    uint8_t  expansion[4];      ///< Reserved for future use (set to 0)

    // Computed fields (not in file)
    bool     uses_bankswitching; ///< True if any bankswitch byte is non-zero
};

// ============================================================================
// NSF METADATA BLOB
// ============================================================================

/** Magic tag for NSF metadata in format_load_result_t::metadata[] */
static constexpr uint32_t NSF_METADATA_TAG = 0x4E534648;  // "NSFH"

/**
 * Metadata blob stored inside format_load_result_t::metadata[].
 * The format system stores this so that the NES system can detect
 * an NSF file and launch the player instead of loading a cartridge.
 */
struct nsf_metadata_blob_t {
    uint32_t     tag;       ///< Must be NSF_METADATA_TAG
    nsf_header_t header;    ///< Parsed NSF header
};

static_assert(sizeof(nsf_metadata_blob_t) <= FORMAT_METADATA_MAX_SIZE,
              "nsf_metadata_blob_t exceeds FORMAT_METADATA_MAX_SIZE");

// ============================================================================
// NSF FORMAT API
// ============================================================================

/**
 * Parse a 128-byte NSF header from raw file data.
 * Returns true on success, false if magic doesn't match or data is too short.
 */
bool nsf_parse_header(const uint8_t* data, size_t size, nsf_header_t* out);

/**
 * Extract the NSF metadata blob from a format_load_result_t.
 * Returns a pointer to the header if the metadata tag matches, else nullptr.
 */
const nsf_header_t* nsf_get_metadata(const format_load_result_t* result);

/**
 * Format descriptor for NSF files — used by the format registry and
 * by NES's SystemDescriptor::supported_formats list.
 */
extern const format_descriptor_t NSF_FORMAT_DESCRIPTOR;
