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
typedef struct commodore_d64_s {
    uint8_t* data;                  /**< Raw disk image data */
    size_t   data_size;             /**< Size of disk image */
    int      num_tracks;            /**< 35 or 40 */
    bool     has_errors;            /**< Whether error bytes are present */
    bool     owns_data;             /**< Whether we allocated data (and should free it) */

    /** Open a D64 disk image from file. Caller must call close(). */
    bool open(const char* filepath);

    /** Open a D64 disk image from an in-memory buffer. Buffer is NOT copied. */
    bool open_mem(const uint8_t* buf, size_t size);

    /** Read the directory of a D64 disk image. */
    bool read_directory(commodore_d64_directory_t* out_dir) const;

    /** Extract a file by directory index. Returns raw file data. */
    bool extract_file(int entry_idx, uint8_t** out_data, size_t* out_size) const;

    /** Extract the first PRG file from a D64 disk image. */
    bool extract_first_prg(commodore_prg_t* out_prg) const;

    /** Close a D64 disk image and free resources. */
    void close();
} commodore_d64_t;

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
