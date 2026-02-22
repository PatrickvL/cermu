#pragma once

/**
 * Format Handler — Base types and interface for file format handlers
 *
 * Defines the format descriptor interface, generic program data type,
 * unified load result, and shared utility functions.
 *
 * Design principles:
 *   - Format handlers are system-agnostic (know about data, not systems)
 *   - Each format self-registers via FormatRegistry
 *   - Each format provides an identify() + load() callback pair
 *   - The registry dispatches through these callbacks—no format-specific code
 *   - Metadata is stored as an opaque blob; consumers cast based on format
 *   - Prepared for usage as storage volume backends (FORMAT_CAP_VOLUME)
 *   - All allocations are documented; caller frees returned buffers
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Format Capability Flags
// ============================================================================

typedef enum {
    FORMAT_CAP_NONE         = 0,
    FORMAT_CAP_LOADABLE     = (1 << 0),  /**< Produces directly loadable program data */
    FORMAT_CAP_CONTAINER    = (1 << 1),  /**< Contains multiple files (D64, T64, LNX) */
    FORMAT_CAP_STREAMABLE   = (1 << 2),  /**< Sequential/streaming data (TAP) */
    FORMAT_CAP_METADATA     = (1 << 3),  /**< Provides format-specific metadata (CRT, TAP) */
    FORMAT_CAP_VOLUME       = (1 << 4),  /**< Usable as storage volume (D64 for 1541, etc.) */
} format_capability_t;

// ============================================================================
// Program Data — Generic loadable program (address + bytes)
// ============================================================================

/**
 * A block of loadable program data: load address and raw bytes.
 * This is the universal exchange type between format handlers and the
 * system layer.  Caller is responsible for freeing `data` via
 * release().
 */
typedef struct program_data_s {
    uint8_t* data;          /**< Program data bytes.  Caller frees. */
    size_t   data_size;     /**< Size of data in bytes */
    uint16_t load_addr;     /**< Load address (destination in system memory) */
    uint16_t end_addr;      /**< End address (load_addr + data_size) */

    /** Free the heap-allocated data buffer. */
    void release();
} program_data_t;

// ============================================================================
// Load Result — Unified result from any format handler
// ============================================================================

/**
 * Categories of load results (format-agnostic).
 * Consumers switch on these; for METADATA results, check result.format
 * to determine the metadata layout.
 */
typedef enum {
    FORMAT_LOAD_NONE = 0,
    FORMAT_LOAD_PROGRAM,    /**< Single loadable program (check program) */
    FORMAT_LOAD_ARCHIVE,    /**< Multiple programs extracted (check files[]) */
    FORMAT_LOAD_METADATA,   /**< Format-specific metadata only (check metadata) */
    FORMAT_LOAD_RAW,        /**< Raw binary, no header (check program, load_addr=0) */
    FORMAT_LOAD_ERROR       /**< Load failed (check error_msg) */
} format_load_type_t;

#define FORMAT_LOAD_MAX_FILES       64
#define FORMAT_METADATA_MAX_SIZE    256

/* Forward-declare so load result can hold a back-pointer */
typedef struct format_descriptor_s format_descriptor_t;

/**
 * Unified load result.  Filled by a format handler's load() callback.
 */
typedef struct format_load_result_s {
    format_load_type_t           type;
    const format_descriptor_t*   format;      /**< Which format handler produced this */
    program_data_t               program;     /**< Valid for PROGRAM / RAW */
    program_data_t               files[FORMAT_LOAD_MAX_FILES]; /**< Valid for ARCHIVE */
    int                          file_count;  /**< Number of entries in files[] */
    uint8_t                      metadata[FORMAT_METADATA_MAX_SIZE]; /**< Opaque metadata blob */
    size_t                       metadata_size; /**< Bytes used in metadata[] */
    char                         error_msg[256];

    /** Free all heap-allocated program data buffers. */
    void release();
} format_load_result_t;

const char* format_load_type_name(format_load_type_t type);

// ============================================================================
// Format Descriptor — each format handler provides one of these
// ============================================================================

/**
 * Describes a file format handler.
 * Format implementations provide a static const instance and register it
 * with the FormatRegistry at static-init time via REGISTER_FORMAT.
 *
 * identify()  — content/extension-based confidence score (0.0–1.0)
 * load()      — parse the file and fill a format_load_result_t
 *
 * Either callback may be NULL (e.g., a volume-only format might omit load).
 */
struct format_descriptor_s {
    const char*  name;           /**< Short name: "PRG", "D64", etc. */
    const char*  description;    /**< Human-readable: "Commodore Program File" */
    const char** extensions;     /**< NULL-terminated: {".prg", NULL} */
    uint32_t     capabilities;   /**< Bitfield of format_capability_t */

    /** Content-based format identification (0.0–1.0). */
    float (*identify)(const uint8_t* data, size_t file_size, const char* extension);

    /** Load a file and fill the result.  NULL if format doesn't support direct loading. */
    bool  (*load)(const char* filepath, format_load_result_t* out);
};

// ============================================================================
// Shared Utility Functions
// ============================================================================

/** Read entire file into heap-allocated buffer.  Caller frees.  Returns NULL on failure. */
uint8_t* format_read_entire_file(const char* filepath, size_t* out_size);

/** Case-insensitive file extension comparison (includes the dot). */
bool format_ext_match(const char* ext, const char* target);

/**
 * Check whether a format descriptor appears in a NULL-terminated array.
 * Useful for checking if a system supports a specific format.
 */
bool format_in_list(const format_descriptor_t* fmt,
                    const format_descriptor_t* const* list);

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ Helpers — work on NULL-terminated format descriptor lists
// ============================================================================

#ifdef __cplusplus
#include <string>
#include <vector>

/**
 * Collect all extensions from a NULL-terminated format descriptor array.
 * Returns e.g. { ".prg", ".d64", ".t64" }.
 */
std::vector<std::string> format_list_extensions(
    const format_descriptor_t* const* formats);

/**
 * Build an ImGuiFileDialog-compatible filter from a format descriptor array.
 * @param formats  NULL-terminated array of format descriptor pointers
 * @param label    Short label, e.g. "C64" → produces "C64 Files{.prg,.d64,...}"
 * @return Filter string including an ",.*" All Files fallback.
 */
std::string format_list_dialog_filter(
    const format_descriptor_t* const* formats,
    const char* label);

#endif /* __cplusplus */

static inline uint16_t format_read_le16(const uint8_t* data) {
    return (uint16_t)(data[0] | (data[1] << 8));
}

static inline uint32_t format_read_le32(const uint8_t* data) {
    return (uint32_t)(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}

static inline uint16_t format_read_be16(const uint8_t* data) {
    return (uint16_t)((data[0] << 8) | data[1]);
}

static inline uint32_t format_read_be32(const uint8_t* data) {
    return (uint32_t)((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
}
