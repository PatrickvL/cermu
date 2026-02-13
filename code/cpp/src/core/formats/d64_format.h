#pragma once

/**
 * D64 Format Handler — 1541 Disk Image
 *
 * Parses Commodore 1541 disk images (35/40 track, with/without error bytes).
 * Provides directory listing, file extraction, and raw sector access —
 * designed for both direct file loading and future use as a storage volume
 * backend for 1541 drive emulation.
 */

#include "format_handler.h"
#include "prg_format.h"   /* commodore_prg_t used as extraction target */

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// D64 Constants
// ============================================================================

#define D64_STANDARD_SIZE       174848  /**< 35 tracks, no error bytes */
#define D64_STANDARD_SIZE_ERR   175531  /**< 35 tracks, with error bytes */
#define D64_EXTENDED_SIZE       196608  /**< 40 tracks, no error bytes */
#define D64_EXTENDED_SIZE_ERR   197376  /**< 40 tracks, with error bytes */
#define D64_MAX_DIR_ENTRIES     144     /**< Maximum directory entries */
#define D64_SECTOR_SIZE         256     /**< Bytes per sector */
#define D64_DIR_TRACK           18      /**< Directory track */
#define D64_DIR_SECTOR            1     /**< First directory sector */
#define D64_BAM_TRACK           18      /**< BAM track */
#define D64_BAM_SECTOR            0     /**< BAM sector */

/** Commodore DOS file type constants (lower 4 bits of file type byte) */
#define D64_FTYPE_DEL  0x00
#define D64_FTYPE_SEQ  0x01
#define D64_FTYPE_PRG  0x02
#define D64_FTYPE_USR  0x03
#define D64_FTYPE_REL  0x04

/** D64 file type byte flags */
#define D64_FTYPE_CLOSED    0x80    /**< File properly closed (bit 7) */
#define D64_FTYPE_LOCKED    0x40    /**< File locked (bit 6) */
#define D64_FTYPE_MASK      0x07    /**< File type mask (bits 0-2) */

// ============================================================================
// D64 Types
// ============================================================================

/** D64 directory entry */
typedef struct {
    char     filename[17];          /**< PETSCII filename, null-terminated */
    uint8_t  file_type;             /**< Raw file type byte (includes closed/locked flags) */
    uint8_t  start_track;           /**< First track of file data */
    uint8_t  start_sector;          /**< First sector of file data */
    uint16_t size_blocks;           /**< File size in 254-byte blocks */
} commodore_d64_entry_t;

/** D64 directory listing */
typedef struct {
    commodore_d64_entry_t entries[D64_MAX_DIR_ENTRIES];
    int count;                      /**< Number of entries found */
    char disk_name[17];             /**< Disk name from BAM, null-terminated */
    char disk_id[6];                /**< Disk ID + DOS type, null-terminated */
} commodore_d64_directory_t;

/**
 * D64 disk image handle.
 * Usable both for one-shot file extraction and as an ongoing volume
 * (e.g., attached to a 1541 drive emulation).
 */
typedef struct {
    uint8_t* data;                  /**< Raw disk image data */
    size_t   data_size;             /**< Size of disk image */
    int      num_tracks;            /**< 35 or 40 */
    bool     has_errors;            /**< Whether error bytes are present */
    bool     owns_data;             /**< Whether we allocated data (and should free it) */
} commodore_d64_t;

// ============================================================================
// D64 Format API
// ============================================================================

/**
 * Open a D64 disk image from file.
 * @param filepath  Path to the .d64 file
 * @param out_d64   Output disk handle
 * @return true on success.  Caller must call commodore_d64_close().
 */
bool commodore_d64_open(const char* filepath, commodore_d64_t* out_d64);

/**
 * Open a D64 disk image from an in-memory buffer.
 * The buffer is NOT copied; caller must keep it alive until close.
 */
bool commodore_d64_open_mem(const uint8_t* data, size_t size, commodore_d64_t* out_d64);

/** Read the directory of a D64 disk image. */
bool commodore_d64_read_directory(const commodore_d64_t* d64, commodore_d64_directory_t* out_dir);

/**
 * Extract a file from a D64 disk image by directory index.
 * Follows the track/sector chain and returns raw file data (with 2-byte
 * load-address header for PRG files).
 */
bool commodore_d64_extract_file(const commodore_d64_t* d64, int entry_idx,
                                uint8_t** out_data, size_t* out_size);

/**
 * Extract the first PRG file from a D64 disk image.
 * Convenience: finds the first closed PRG and extracts it.
 */
bool commodore_d64_extract_first_prg(const commodore_d64_t* d64, commodore_prg_t* out_prg);

/** Close a D64 disk image and free resources. */
void commodore_d64_close(commodore_d64_t* d64);

// ============================================================================
// Low-Level Sector Access (for future 1541 drive emulation)
// ============================================================================

/** Get byte offset in D64 image for a given track (1-based) and sector. */
int d64_sector_offset(int track, int sector);

/** Get the number of sectors on a given track. */
int d64_max_sector(int track);

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t D64_FORMAT_DESCRIPTOR;

#ifdef __cplusplus
}
#endif
