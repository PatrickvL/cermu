/**
 * BIN Format Handler — Implementation
 */

#include "core/formats/bin_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Format Identification
// ============================================================================

static float bin_identify(const uint8_t* data, size_t file_size, const char* extension) {
    if (extension && format_ext_match(extension, ".bin")) return 0.5f;
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool bin_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (!data || size == 0) {
        snprintf(out->error_msg, sizeof(out->error_msg), "Empty BIN data");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }
    out->type = FORMAT_LOAD_RAW;
    out->program.data = (uint8_t*)malloc(size);
    if (!out->program.data) {
        snprintf(out->error_msg, sizeof(out->error_msg), "Out of memory");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }
    memcpy(out->program.data, data, size);
    out->program.data_size = size;
    out->program.load_addr = 0;
    out->program.end_addr = (uint16_t)(size > 0xFFFF ? 0xFFFF : size);
    return true;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* bin_extensions[] = { ".bin", NULL };

const format_descriptor_t BIN_FORMAT_DESCRIPTOR = {
    "BIN",
    "Raw Binary File",
    bin_extensions,
    FORMAT_CAP_LOADABLE,
    bin_identify,
    bin_load,
    nullptr,  // list_entries
    nullptr   // extract_entry
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(BIN, &BIN_FORMAT_DESCRIPTOR)
