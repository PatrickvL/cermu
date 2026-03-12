#pragma once

/**
 * T64 Format Handler — Tape Archive Image
 *
 * Parses T64 tape archive files containing one or more Commodore program
 * entries.  Provides directory listing and file extraction.
 */

#include "core/formats/format_handler.hpp"
#include "core/formats/prg_format.hpp"   /* commodore_prg_t used as extraction target */
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
struct commodore_t64_entry_t {
    char     filename[17];          /**< PETSCII filename, null-terminated */
    uint8_t  c64s_file_type;       /**< C64s file type (1=normal, 3=frozen memory image) */
    uint8_t  file_type;            /**< 1541 file type (0x82=PRG etc.) */
    uint16_t start_addr;           /**< Start address (load address) */
    uint16_t end_addr;             /**< End address */
    uint32_t data_offset;          /**< Offset in T64 file to actual data */
    uint32_t data_size;            /**< Computed size (end_addr - start_addr) */
};

/** T64 archive listing */
struct commodore_t64_directory_t {
    char tape_name[25];             /**< Tape name from header, null-terminated */
    uint16_t version;               /**< T64 version */
    uint16_t max_entries;           /**< Max directory entries */
    uint16_t used_entries;          /**< Used directory entries */
    commodore_t64_entry_t entries[T64_MAX_ENTRIES];
    int count;                      /**< Actual valid entry count */
};

/** T64 archive handle */
struct commodore_t64_t {
    uint8_t* data;                  /**< Raw T64 data */
    size_t   data_size;             /**< Size of T64 data */
    bool     owns_data;             /**< Whether we allocated data */

    /** Open a T64 tape archive from an in-memory buffer. */
    bool open_mem(const uint8_t* buf, size_t buf_size);

    /** Read the directory of a T64 tape archive. */
    bool read_directory(commodore_t64_directory_t* out_dir) const;

    /** Extract a file from a T64 archive as a PRG. */
    bool extract_file(int entry_idx, commodore_prg_t* out_prg) const;

    /** Extract the first PRG from a T64 archive. */
    bool extract_first_prg(commodore_prg_t* out_prg) const;

    /** Close a T64 archive and free resources. */
    void close();
};

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t T64_FORMAT_DESCRIPTOR;

