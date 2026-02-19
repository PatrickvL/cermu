#pragma once

/**
 * T64 Format Handler — Tape Archive Image
 *
 * Parses T64 tape archive files containing one or more Commodore program
 * entries.  Provides directory listing and file extraction.
 */

#include "format_handler.h"
#include "prg_format.h"   /* commodore_prg_t used as extraction target */

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// T64 Constants
// ============================================================================

#define T64_HEADER_SIZE     64
#define T64_ENTRY_SIZE      32
#define T64_MAX_ENTRIES     256

// ============================================================================
// T64 Types
// ============================================================================

/** T64 directory entry */
typedef struct {
    char     filename[17];          /**< PETSCII filename, null-terminated */
    uint8_t  c64s_file_type;       /**< C64s file type (1=normal, 3=frozen memory image) */
    uint8_t  file_type;            /**< 1541 file type (0x82=PRG etc.) */
    uint16_t start_addr;           /**< Start address (load address) */
    uint16_t end_addr;             /**< End address */
    uint32_t data_offset;          /**< Offset in T64 file to actual data */
    uint32_t data_size;            /**< Computed size (end_addr - start_addr) */
} commodore_t64_entry_t;

/** T64 archive listing */
typedef struct {
    char tape_name[25];             /**< Tape name from header, null-terminated */
    uint16_t version;               /**< T64 version */
    uint16_t max_entries;           /**< Max directory entries */
    uint16_t used_entries;          /**< Used directory entries */
    commodore_t64_entry_t entries[T64_MAX_ENTRIES];
    int count;                      /**< Actual valid entry count */
} commodore_t64_directory_t;

/** T64 archive handle */
typedef struct {
    uint8_t* data;                  /**< Raw T64 data */
    size_t   data_size;             /**< Size of T64 data */
    bool     owns_data;             /**< Whether we allocated data */
} commodore_t64_t;

// ============================================================================
// T64 Format API
// ============================================================================

/** Open a T64 tape archive from file. */
bool commodore_t64_open(const char* filepath, commodore_t64_t* out_t64);

/** Read the directory of a T64 tape archive. */
bool commodore_t64_read_directory(const commodore_t64_t* t64, commodore_t64_directory_t* out_dir);

/** Extract a file from a T64 archive as a PRG. */
bool commodore_t64_extract_file(const commodore_t64_t* t64, int entry_idx, commodore_prg_t* out_prg);

/** Extract the first PRG from a T64 archive. */
bool commodore_t64_extract_first_prg(const commodore_t64_t* t64, commodore_prg_t* out_prg);

/** Close a T64 archive and free resources. */
void commodore_t64_close(commodore_t64_t* t64);

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t T64_FORMAT_DESCRIPTOR;

#ifdef __cplusplus
}
#endif
