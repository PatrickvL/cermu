/**
 * SCL Format Handler — Implementation
 *
 * Parses ZX Spectrum .scl (TR-DOS container) files.
 * Identifies by the "SINCLAIR" magic at offset 0.
 *
 * Uses the shared trdos_common.hpp types for parsing, loading,
 * and container browsing.
 */

#include "core/formats/scl_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t SCL_MAGIC_SIZE   = 8;
static constexpr size_t SCL_HEADER_SIZE  = 9;   // magic(8) + file_count(1)

static const uint8_t SCL_MAGIC[SCL_MAGIC_SIZE] = {
    'S','I','N','C','L','A','I','R'
};

// ============================================================================
// Internal: Parse SCL file entries
// ============================================================================

struct scl_parsed_t {
    trdos::file_entry_t entries[trdos::MAX_FILES];
    size_t              data_offsets[trdos::MAX_FILES];
    const uint8_t*      data_ptrs[trdos::MAX_FILES];
    size_t              data_sizes[trdos::MAX_FILES];
    int                 count;
};

/// Parse all file headers and compute data offsets.
static bool scl_parse(const uint8_t* data, size_t size, scl_parsed_t& out) {
    out.count = 0;
    if (!data || size < SCL_HEADER_SIZE) return false;
    if (std::memcmp(data, SCL_MAGIC, SCL_MAGIC_SIZE) != 0) return false;

    int file_count = data[8];
    if (file_count == 0 || file_count > trdos::MAX_FILES) return false;

    size_t headers_end = SCL_HEADER_SIZE + file_count * trdos::SCL_ENTRY_SIZE;
    if (headers_end > size) return false;

    int count = 0;
    size_t data_offset = headers_end;

    for (int i = 0; i < file_count; ++i) {
        const uint8_t* p = data + SCL_HEADER_SIZE + i * trdos::SCL_ENTRY_SIZE;

        trdos::parse_entry(p, trdos::SCL_ENTRY_SIZE, out.entries[count]);

        size_t sector_data = static_cast<size_t>(out.entries[count].sectors) * trdos::SECTOR_SIZE;

        out.data_offsets[count] = data_offset;
        out.data_ptrs[count]    = data + data_offset;
        out.data_sizes[count]   = (data_offset + sector_data <= size) ? sector_data : (size - data_offset);

        data_offset += sector_data;
        count++;
    }

    out.count = count;
    return count > 0;
}

// ============================================================================
// Format Identification
// ============================================================================

static float scl_identify(const uint8_t* data, size_t file_size,
                           const char* extension) {
    bool ext_match = extension && format_ext_match(extension, ".scl");

    // Check magic signature
    bool has_magic = (file_size >= SCL_HEADER_SIZE &&
                      std::memcmp(data, SCL_MAGIC, SCL_MAGIC_SIZE) == 0);

    if (has_magic) {
        int n = data[8];
        if (n > 0 && n <= trdos::MAX_FILES) {
            size_t expected_min = SCL_HEADER_SIZE + n * trdos::SCL_ENTRY_SIZE;
            if (file_size >= expected_min) {
                return ext_match ? 0.95f : 0.85f;
            }
        }
    }

    if (ext_match) return 0.3f;  // Extension matches but no valid magic
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool scl_load(const uint8_t* data, size_t size,
                     format_load_result_t* out) {
    scl_parsed_t parsed;
    if (!scl_parse(data, size, parsed)) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "SCL: Invalid or empty container");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    return trdos::load_best_entry(parsed.entries, parsed.count,
                                  parsed.data_ptrs, parsed.data_sizes,
                                  "SCL", out);
}

// ============================================================================
// Container: List Entries
// ============================================================================

static int scl_list_entries(const uint8_t* data, size_t size,
                            format_container_entry_t* out_entries,
                            int max_entries) {
    scl_parsed_t parsed;
    if (!scl_parse(data, size, parsed)) return -1;
    return trdos::list_entries_common(parsed.entries, parsed.count,
                                      out_entries, max_entries);
}

// ============================================================================
// Container: Extract Entry
// ============================================================================

static bool scl_extract_entry(const uint8_t* data, size_t size,
                              int entry_index, uint8_t** out_data,
                              size_t* out_size) {
    scl_parsed_t parsed;
    if (!scl_parse(data, size, parsed)) return false;
    if (entry_index < 0 || entry_index >= parsed.count) return false;

    const trdos::file_entry_t& e = parsed.entries[entry_index];
    size_t payload_size = trdos::file_data_length(e);
    if (payload_size > parsed.data_sizes[entry_index])
        payload_size = parsed.data_sizes[entry_index];

    *out_data = static_cast<uint8_t*>(std::malloc(payload_size));
    if (!*out_data) return false;

    std::memcpy(*out_data, parsed.data_ptrs[entry_index], payload_size);
    *out_size = payload_size;
    return true;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* scl_extensions[] = { ".scl", nullptr };

const format_descriptor_t SCL_FORMAT_DESCRIPTOR = {
    "SCL",
    "ZX Spectrum TR-DOS Container",
    scl_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER,
    scl_identify,
    scl_load,
    scl_list_entries,
    scl_extract_entry
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(SCL, &SCL_FORMAT_DESCRIPTOR)
