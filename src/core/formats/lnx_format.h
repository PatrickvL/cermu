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
typedef struct {
    uint8_t* data;                  /**< Raw archive data */
    size_t   data_size;             /**< Size of archive */
    bool     owns_data;             /**< Whether we allocated data */
} commodore_lynx_t;

// ============================================================================
// LNX Format API
// ============================================================================

/**
 * Open a Lynx archive from file.
 * @return true on success.  Caller must call commodore_lynx_close().
 */
bool commodore_lynx_open(const char* filepath, commodore_lynx_t* out_lynx);

/** Read the directory of a Lynx archive. */
bool commodore_lynx_read_directory(const commodore_lynx_t* lynx, commodore_lynx_directory_t* out_dir);

/**
 * Extract a file from a Lynx archive by index as a PRG.
 * @param dir       Previously-read directory
 * @param entry_idx Directory entry index (0-based)
 */
bool commodore_lynx_extract_file(const commodore_lynx_t* lynx,
                                 const commodore_lynx_directory_t* dir,
                                 int entry_idx, commodore_prg_t* out_prg);

/**
 * Extract ALL PRG files from a Lynx archive.
 * @param out_prgs   Caller-allocated array (max LNX_MAX_FILES)
 * @param max_prgs   Size of out_prgs array
 * @param out_count  Number of PRGs actually extracted
 * @return true if at least one PRG was extracted
 */
bool commodore_lynx_extract_all_prgs(const commodore_lynx_t* lynx,
                                     commodore_prg_t* out_prgs, int max_prgs,
                                     int* out_count);

/** Close a Lynx archive and free resources. */
void commodore_lynx_close(commodore_lynx_t* lynx);

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t LNX_FORMAT_DESCRIPTOR;

#ifdef __cplusplus
}
#endif
