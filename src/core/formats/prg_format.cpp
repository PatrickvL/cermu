/**
 * PRG Format Handler — Implementation
 */

#include "prg_format.h"
#include "format_registry.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// PRG Format Implementation
// ============================================================================

bool commodore_prg_load(const char* filepath, commodore_prg_t* out_prg) {
    if (!filepath || !out_prg) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    size_t file_size = 0;
    uint8_t* file_data = format_read_entire_file(filepath, &file_size);
    if (!file_data) {
        printf("PRGFormat: Cannot open PRG file: %s\n", filepath);
        return false;
    }

    bool ok = commodore_prg_parse(file_data, file_size, out_prg);
    free(file_data);
    return ok;
}

bool commodore_prg_parse(const uint8_t* buffer, size_t size, commodore_prg_t* out_prg) {
    if (!buffer || !out_prg || size < 2) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    uint16_t load_addr = format_read_le16(buffer);
    size_t data_size = size - 2;

    if ((uint32_t)load_addr + data_size > 0x10000) {
        printf("PRGFormat: PRG data exceeds 64KB (load=$%04X, size=%zu)\n",
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
// BIN Format Implementation
// ============================================================================

bool commodore_bin_load(const char* filepath, uint8_t** out_data, size_t* out_size) {
    if (!filepath || !out_data || !out_size) return false;
    *out_data = format_read_entire_file(filepath, out_size);
    return (*out_data != NULL);
}

// ============================================================================
// Format Identification
// ============================================================================

static float prg_identify(const uint8_t* data, size_t file_size, const char* extension) {
    if (extension && format_ext_match(extension, ".prg")) return 0.9f;
    /* PRG is too generic to identify by content alone */
    return 0.0f;
}

static float bin_identify(const uint8_t* data, size_t file_size, const char* extension) {
    if (extension && format_ext_match(extension, ".bin")) return 0.5f;
    return 0.0f;
}

// ============================================================================
// Load Callbacks
// ============================================================================

static bool prg_load(const char* filepath, format_load_result_t* out) {
    if (commodore_prg_load(filepath, &out->program)) {
        out->type = FORMAT_LOAD_PROGRAM;
        return true;
    }
    snprintf(out->error_msg, sizeof(out->error_msg), "Failed to load PRG: %s", filepath);
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

static bool bin_load(const char* filepath, format_load_result_t* out) {
    uint8_t* data = NULL;
    size_t size = 0;
    if (commodore_bin_load(filepath, &data, &size)) {
        out->type = FORMAT_LOAD_RAW;
        out->program.data = data;
        out->program.data_size = size;
        out->program.load_addr = 0;
        out->program.end_addr = (uint16_t)(size > 0xFFFF ? 0xFFFF : size);
        return true;
    }
    snprintf(out->error_msg, sizeof(out->error_msg), "Failed to load BIN: %s", filepath);
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

// ============================================================================
// Format Descriptors
// ============================================================================

static const char* prg_extensions[] = { ".prg", NULL };
static const char* bin_extensions[] = { ".bin", NULL };

const format_descriptor_t PRG_FORMAT_DESCRIPTOR = {
    "PRG",
    "Commodore Program File",
    prg_extensions,
    FORMAT_CAP_LOADABLE,
    prg_identify,
    prg_load
};

const format_descriptor_t BIN_FORMAT_DESCRIPTOR = {
    "BIN",
    "Raw Binary File",
    bin_extensions,
    FORMAT_CAP_LOADABLE,
    bin_identify,
    bin_load
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(PRG, &PRG_FORMAT_DESCRIPTOR)
REGISTER_FORMAT(BIN, &BIN_FORMAT_DESCRIPTOR)
