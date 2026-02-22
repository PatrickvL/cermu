#pragma once

/**
 * LNX Format Handler — Lynx Multi-File Archive
 *
 * Parses Lynx archives (cbmconvert / Will Corley format) containing
 * multiple Commodore program files.  Provides directory listing and
 * extraction of individual or all PRG files.
 */

#include "format_handler.h"
#include "prg_format.h"   /* commodore_prg_t used as extraction target */

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LNX Constants
// ============================================================================

#define LNX_MAX_FILES           256     /**< Maximum files in a Lynx archive */
#define LNX_MAX_BASIC_LENGTH    1024    /**< Maximum length of BASIC dissolve header */

// ============================================================================
// LNX Types
// ============================================================================

/** Lynx archive file entry */
typedef struct {
    char     filename[17];          /**< PETSCII filename, null-terminated */
    char     file_type;             /**< 'P'=PRG, 'S'=SEQ, 'U'=USR, 'R'=REL, 'D'=DEL */
    unsigned blocks;                /**< Number of 254-byte data blocks */
    unsigned last_block_len;        /**< Bytes used in last block (or record len for REL) */
    size_t   data_offset;           /**< Byte offset of file data within the archive */
    size_t   data_length;           /**< Actual file data length in bytes */
} commodore_lynx_entry_t;

/** Lynx archive directory */
typedef struct {
    unsigned header_blocks;         /**< Blocks occupied by header+directory */
    unsigned file_count;            /**< Number of files in archive */
    commodore_lynx_entry_t entries[LNX_MAX_FILES];
} commodore_lynx_directory_t;

/** Lynx archive handle */
typedef struct commodore_lynx_s {
    uint8_t* data;                  /**< Raw archive data */
    size_t   data_size;             /**< Size of archive */
    bool     owns_data;             /**< Whether we allocated data */

    /** Open a Lynx archive from file. Caller must call close(). */
    bool open(const char* filepath);

    /** Read the directory of a Lynx archive. */
    bool read_directory(commodore_lynx_directory_t* out_dir) const;

    /** Extract a file from a Lynx archive by index as a PRG. */
    bool extract_file(const commodore_lynx_directory_t* dir,
                      int entry_idx, commodore_prg_t* out_prg) const;

    /** Extract ALL PRG files from a Lynx archive. */
    bool extract_all_prgs(commodore_prg_t* out_prgs, int max_prgs,
                          int* out_count) const;

    /** Close a Lynx archive and free resources. */
    void close();
} commodore_lynx_t;

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t LNX_FORMAT_DESCRIPTOR;

#ifdef __cplusplus
}
#endif
