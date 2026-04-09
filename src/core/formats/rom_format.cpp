/**
 * ROM Format Handler — Generic ROM file for sideways ROM banks
 *
 * Handles raw .rom binary files used as sideways/paged ROM images
 * (BBC Micro, Acorn Electron, etc.).  No header or container structure —
 * just raw binary loaded as FORMAT_LOAD_RAW.
 *
 * The system layer handles placement into the correct ROM slot.
 */

#include "core/cermu.hpp"
#include "core/formats/format_handler.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdlib>
#include <cstring>

// ============================================================================
// Format Identification
// ============================================================================

static float rom_identify(const uint8_t* /*data*/, size_t file_size, const char* extension) {
    // .rom files are raw binary — no magic number to check.
    // Accept files ≤64KB (reasonable ROM size limit).
    if (extension && format_ext_match(extension, ".rom") && file_size <= 65536) {
        return 0.5f;  // Low confidence — extension-only, no magic
    }
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool rom_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    out->program.data = static_cast<uint8_t*>(malloc(size));
    if (!out->program.data) {
        out->type = FORMAT_LOAD_ERROR;
        snprintf(out->error_msg, sizeof(out->error_msg), "ROM: allocation failed");
        return false;
    }
    memcpy(out->program.data, data, size);
    out->program.data_size = size;
    out->program.load_addr = 0;
    out->program.end_addr = 0;
    out->type = FORMAT_LOAD_RAW;
    return true;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* rom_extensions[] = { ".rom", NULL };

static const format_descriptor_t ROM_FORMAT_DESCRIPTOR = {
    "ROM",
    "Raw ROM Image",
    rom_extensions,
    FORMAT_CAP_LOADABLE,
    rom_identify,
    rom_load,
    nullptr,
    nullptr
};

REGISTER_FORMAT(ROM, &ROM_FORMAT_DESCRIPTOR)
