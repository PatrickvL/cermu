/**
 * CRT Format Handler — Implementation
 */

#include "crt_format.h"
#include "format_registry.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// CRT Header Reading
// ============================================================================

bool commodore_crt_read_header_mem(const uint8_t* data, size_t data_size, commodore_crt_header_t* out_header) {
    if (!data || data_size < 64 || !out_header) return false;
    memset(out_header, 0, sizeof(*out_header));

    const uint8_t* raw = data;

    /* Validate signature */
    if (memcmp(raw, "C64 CARTRIDGE   ", 16) != 0) {
        printf("CRTFormat: Invalid CRT signature\n");
        return false;
    }

    memcpy(out_header->signature, raw, 16);
    out_header->header_length = format_read_be32(raw + 16);
    out_header->version       = format_read_be16(raw + 20);
    out_header->hardware_type = format_read_be16(raw + 22);
    out_header->exrom         = raw[24];
    out_header->game          = raw[25];
    memcpy(out_header->reserved, raw + 26, 6);
    memcpy(out_header->name, raw + 32, 32);
    out_header->name[31] = '\0';

    printf("CRTFormat: \"%s\" hw_type=%d EXROM=%d GAME=%d\n",
           out_header->name, out_header->hardware_type,
           out_header->exrom, out_header->game);
    return true;
}

bool commodore_crt_read_header(const char* filepath, commodore_crt_header_t* out_header) {
    if (!filepath || !out_header) return false;

    size_t file_size = 0;
    uint8_t* file_data = format_read_entire_file(filepath, &file_size);
    if (!file_data) return false;

    bool ok = commodore_crt_read_header_mem(file_data, file_size, out_header);
    free(file_data);
    return ok;
}

// ============================================================================
// Format Identification
// ============================================================================

static float crt_identify(const uint8_t* data, size_t file_size, const char* extension) {
    if (data && file_size >= 64 && memcmp(data, "C64 CARTRIDGE   ", 16) == 0)
        return 0.95f;
    if (extension && format_ext_match(extension, ".crt")) return 0.8f;
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool crt_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    commodore_crt_header_t hdr;
    if (commodore_crt_read_header_mem(data, size, &hdr)) {
        out->type = FORMAT_LOAD_METADATA;
        memcpy(out->metadata, &hdr, sizeof(hdr));
        out->metadata_size = sizeof(hdr);
        return true;
    }
    snprintf(out->error_msg, sizeof(out->error_msg),
             "Failed to read CRT header from memory");
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* crt_extensions[] = { ".crt", NULL };

const format_descriptor_t CRT_FORMAT_DESCRIPTOR = {
    "CRT",
    "Cartridge Image",
    crt_extensions,
    FORMAT_CAP_METADATA,
    crt_identify,
    crt_load
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(CRT, &CRT_FORMAT_DESCRIPTOR)
