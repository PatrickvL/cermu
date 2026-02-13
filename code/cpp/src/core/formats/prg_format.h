#pragma once

/**
 * PRG Format Handler — Program Files and Raw Binaries
 *
 * The PRG format is the universal program file format for Commodore 8-bit
 * systems: 2-byte little-endian load address followed by raw program data.
 * BIN is a headerless variant (load address = 0).
 *
 * program_data_t (defined in format_handler.h) is the generic exchange type.
 * commodore_prg_t is a backward-compatible alias used by container format
 * extractors (D64, T64, LNX) and legacy consumer code.
 */

#include "format_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Backward-Compatible Alias
// ============================================================================

/** Alias so container formats and consumers can keep using the old name. */
typedef program_data_t commodore_prg_t;

/** Free wrapper (delegates to program_data_free). */
void commodore_prg_free(commodore_prg_t* prg);

// ============================================================================
// PRG Format API
// ============================================================================

/**
 * Load a PRG file from disk.
 * Reads the 2-byte LE load address and the remaining data.
 * @return true on success.  Caller must free out_prg->data.
 */
bool commodore_prg_load(const char* filepath, commodore_prg_t* out_prg);

/**
 * Parse a PRG from an in-memory buffer (e.g. extracted from D64/T64).
 * @param buffer  Raw PRG data including 2-byte header
 * @return true on success.  Caller must free out_prg->data.
 */
bool commodore_prg_parse(const uint8_t* buffer, size_t size, commodore_prg_t* out_prg);

// ============================================================================
// BIN Format API — Raw Binary (No Header)
// ============================================================================

/**
 * Load a raw binary file.  Returns heap-allocated data.  Caller frees.
 */
bool commodore_bin_load(const char* filepath, uint8_t** out_data, size_t* out_size);

// ============================================================================
// Format Descriptors
// ============================================================================

extern const format_descriptor_t PRG_FORMAT_DESCRIPTOR;
extern const format_descriptor_t BIN_FORMAT_DESCRIPTOR;

#ifdef __cplusplus
}
#endif
