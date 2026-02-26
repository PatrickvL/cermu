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

#include <cstdint>

#include <cstddef>
// ============================================================================
// Format Capability Flags
// ============================================================================

enum format_capability_t {
    FORMAT_CAP_NONE         = 0,
    FORMAT_CAP_LOADABLE     = (1 << 0),  /**< Produces directly loadable program data */
    FORMAT_CAP_CONTAINER    = (1 << 1),  /**< Contains multiple files (D64, T64, LNX) */
    FORMAT_CAP_STREAMABLE   = (1 << 2),  /**< Sequential/streaming data (TAP) */
    FORMAT_CAP_METADATA     = (1 << 3),  /**< Provides format-specific metadata (CRT, TAP) */
    FORMAT_CAP_VOLUME       = (1 << 4),  /**< Usable as storage volume (D64 for 1541, etc.) */
};

// ============================================================================
// Program Data — Generic loadable program (address + bytes)
// ============================================================================

/**
 * A block of loadable program data: load address and raw bytes.
 * This is the universal exchange type between format handlers and the
 * system layer.  Caller is responsible for freeing `data` via
 * release().
 */
struct program_data_t {
    uint8_t* data;          /**< Program data bytes.  Caller frees. */
    size_t   data_size;     /**< Size of data in bytes */
    uint16_t load_addr;     /**< Load address (destination in system memory) */
    uint16_t end_addr;      /**< End address (load_addr + data_size) */

    /** Free the heap-allocated data buffer. */
    void release();
};

// ============================================================================
// Load Result — Unified result from any format handler
// ============================================================================

/**
 * Categories of load results (format-agnostic).
 * Consumers switch on these; for METADATA results, check result.format
 * to determine the metadata layout.
 */
enum format_load_type_t {
    FORMAT_LOAD_NONE = 0,
    FORMAT_LOAD_PROGRAM,    /**< Single loadable program (check program) */
    FORMAT_LOAD_ARCHIVE,    /**< Multiple programs extracted (check files[]) */
    FORMAT_LOAD_METADATA,   /**< Format-specific metadata only (check metadata) */
    FORMAT_LOAD_RAW,        /**< Raw binary, no header (check program, load_addr=0) */
    FORMAT_LOAD_ERROR       /**< Load failed (check error_msg) */
};

#define FORMAT_LOAD_MAX_FILES       64
#define FORMAT_METADATA_MAX_SIZE    512

/* Forward-declare so load result can hold a back-pointer */
struct format_descriptor_t;

/**
 * Unified load result.  Filled by a format handler's load() callback.
 */
struct format_load_result_t {
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
};

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
struct format_descriptor_t {
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
 * Convert a Latin-1 (ISO 8859-1) string to UTF-8 in-place within a buffer.
 * SID metadata uses ISO 8859-1; NSF metadata is nominally ASCII but files in
 * the wild sometimes contain Latin-1 characters.  Window managers and ImGui
 * require valid UTF-8.  Bytes 0x00–0x7F pass through unchanged; bytes
 * 0x80–0xFF are expanded to the correct 2-byte UTF-8 sequence.
 *
 * @param buf      Buffer containing the Latin-1 string (null-terminated).
 *                 Must be large enough to hold the expanded result
 *                 (worst case: 2× input length + 1).
 * @param buf_size Total size of the buffer in bytes.
 */
static inline void format_latin1_to_utf8_buf(char* buf, size_t buf_size) {
    if (!buf || buf_size < 2) return;
    /* First pass: measure the UTF-8 length */
    size_t src_len = 0;
    size_t utf8_len = 0;
    for (size_t i = 0; buf[i] && i < buf_size - 1; i++) {
        unsigned char ch = (unsigned char)buf[i];
        utf8_len += (ch >= 0x80) ? 2 : 1;
        src_len = i + 1;
    }
    if (utf8_len == src_len) return;  /* Pure ASCII — nothing to do */
    if (utf8_len >= buf_size) {
        /* Truncate: find the longest prefix that fits */
        utf8_len = 0;
        src_len = 0;
        for (size_t i = 0; buf[i] && i < buf_size - 1; i++) {
            unsigned char ch = (unsigned char)buf[i];
            size_t needed = (ch >= 0x80) ? 2 : 1;
            if (utf8_len + needed >= buf_size) break;
            utf8_len += needed;
            src_len = i + 1;
        }
    }
    /* Second pass: expand from the end to avoid overwriting unread bytes */
    buf[utf8_len] = '\0';
    size_t dst = utf8_len;
    for (size_t i = src_len; i > 0; i--) {
        unsigned char ch = (unsigned char)buf[i - 1];
        if (ch >= 0x80) {
            buf[--dst] = (char)(0x80 | (ch & 0x3F));
            buf[--dst] = (char)(0xC0 | (ch >> 6));
        } else {
            buf[--dst] = (char)ch;
        }
    }
}

/**
 * Check whether a format descriptor appears in a NULL-terminated array.
 * Useful for checking if a system supports a specific format.
 */
bool format_in_list(const format_descriptor_t* fmt,
                    const format_descriptor_t* const* list);
// ============================================================================
// C++ Helpers — work on NULL-terminated format descriptor lists
// ============================================================================

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
 * @param label    Short label, e.g. "C64" produces "C64 Files{.prg,.d64,...}"
 * @return Filter string including an ",.*" All Files fallback.
 */
std::string format_list_dialog_filter(
    const format_descriptor_t* const* formats,
    const char* label);

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
