/**
 * PRG Format Handler — Implementation
 */

#include "core/cermu.hpp"
#include "core/formats/prg_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// PRG Format Implementation
// ============================================================================

bool commodore_prg_parse(const uint8_t* buffer, size_t size, commodore_prg_t* out_prg) {
    if (!buffer || !out_prg || size < 2) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    uint16_t load_addr = format_read_le16(buffer);
    size_t data_size = size - 2;

    if ((uint32_t)load_addr + data_size > 0x10000) {
        log_info("PRGFormat: PRG data exceeds 64KB (load=$%04X, size=%zu)\n",
               load_addr, data_size);
        return false;
    }

    out_prg->data = (uint8_t*)malloc(data_size);
    if (!out_prg->data) return false;

    memcpy(out_prg->data, buffer + 2, data_size);
    out_prg->data_size = data_size;
    out_prg->load_addr = load_addr;
    out_prg->end_addr  = (uint16_t)(load_addr + data_size);

    return true;
}

void commodore_prg_free(commodore_prg_t* prg) {
    if (prg && prg->data) {
        free(prg->data);
        prg->data = NULL;
        prg->data_size = 0;
    }
}

// ============================================================================
// Format Identification
// ============================================================================

static float prg_identify(const uint8_t* data, size_t file_size, const char* extension) {
    if (extension && format_ext_match(extension, ".prg")) return 0.9f;
    /* PRG is too generic to identify by content alone */
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool prg_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (commodore_prg_parse(data, size, &out->program)) {
        out->type = FORMAT_LOAD_PROGRAM;
        return true;
    }
    snprintf(out->error_msg, sizeof(out->error_msg), "Failed to parse PRG data");
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* prg_extensions[] = { ".prg", NULL };

const format_descriptor_t PRG_FORMAT_DESCRIPTOR = {
    "PRG",
    "Commodore Program File",
    prg_extensions,
    FORMAT_CAP_LOADABLE,
    prg_identify,
    prg_load,
    nullptr,  // list_entries
    nullptr   // extract_entry
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(PRG, &PRG_FORMAT_DESCRIPTOR)
