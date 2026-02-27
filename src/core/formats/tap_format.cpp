/**
 * TAP Format Handler — Implementation
 */

#include "tap_format.h"
#include "format_registry.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// TAP Header Reading
// ============================================================================

bool commodore_tap_read_header(const char* filepath, commodore_tap_header_t* out_header) {
    if (!filepath || !out_header) return false;
    memset(out_header, 0, sizeof(*out_header));

    // Use format_read_entire_file (VFS-aware) instead of direct fopen
    size_t file_size = 0;
    uint8_t* file_data = format_read_entire_file(filepath, &file_size);
    if (!file_data || file_size < 20) {
        free(file_data);
        return false;
    }

    const uint8_t* raw = file_data;

    memcpy(out_header->signature, raw, 12);
    out_header->version        = raw[12];
    out_header->platform       = raw[13];
    out_header->video_standard = raw[14];
    out_header->reserved       = raw[15];
    out_header->data_size      = format_read_le32(raw + 16);

    /* Validate signature */
    if (memcmp(raw, "C64-TAPE-RAW", 12) != 0 &&
        memcmp(raw, "C16-TAPE-RAW", 12) != 0) {
        printf("TAPFormat: Invalid TAP signature: %.12s\n", raw);
        free(file_data);
        return false;
    }

    printf("TAPFormat: sig=%.12s ver=%d platform=%d std=%d size=%u\n",
           out_header->signature, out_header->version, out_header->platform,
           out_header->video_standard, out_header->data_size);
    free(file_data);
    return true;
}

int commodore_tap_identify_platform(const char* filepath) {
    commodore_tap_header_t hdr;
    if (!commodore_tap_read_header(filepath, &hdr)) return -1;

    if (memcmp(hdr.signature, "C64-TAPE-RAW", 12) == 0)
        return hdr.platform;       /* 0=C64, 1=VIC-20 */
    if (memcmp(hdr.signature, "C16-TAPE-RAW", 12) == 0)
        return 2;                  /* C16/Plus4 */
    return -1;
}

// ============================================================================
// Buffer-based variants (avoid re-reading files already in memory)
// ============================================================================

bool commodore_tap_read_header_mem(const uint8_t* data, size_t size,
                                   commodore_tap_header_t* out_header) {
    if (!data || !out_header || size < 20) return false;
    memset(out_header, 0, sizeof(*out_header));

    memcpy(out_header->signature, data, 12);
    out_header->version        = data[12];
    out_header->platform       = data[13];
    out_header->video_standard = data[14];
    out_header->reserved       = data[15];
    out_header->data_size      = format_read_le32(data + 16);

    if (memcmp(data, "C64-TAPE-RAW", 12) != 0 &&
        memcmp(data, "C16-TAPE-RAW", 12) != 0) {
        return false;
    }
    return true;
}

int commodore_tap_identify_platform_mem(const uint8_t* data, size_t size) {
    commodore_tap_header_t hdr;
    if (!commodore_tap_read_header_mem(data, size, &hdr)) return -1;

    if (memcmp(hdr.signature, "C64-TAPE-RAW", 12) == 0)
        return hdr.platform;       /* 0=C64, 1=VIC-20 */
    if (memcmp(hdr.signature, "C16-TAPE-RAW", 12) == 0)
        return 2;                  /* C16/Plus4 */
    return -1;
}

// ============================================================================
// Format Identification
// ============================================================================

static float tap_identify(const uint8_t* data, size_t file_size, const char* extension) {
    if (data && file_size >= 20) {
        if (memcmp(data, "C64-TAPE-RAW", 12) == 0 ||
            memcmp(data, "C16-TAPE-RAW", 12) == 0)
            return 0.95f;
    }
    if (extension && format_ext_match(extension, ".tap")) return 0.8f;
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool tap_load(const char* filepath, format_load_result_t* out) {
    commodore_tap_header_t hdr;
    if (commodore_tap_read_header(filepath, &hdr)) {
        out->type = FORMAT_LOAD_METADATA;
        memcpy(out->metadata, &hdr, sizeof(hdr));
        out->metadata_size = sizeof(hdr);
        return true;
    }
    snprintf(out->error_msg, sizeof(out->error_msg),
             "Failed to read TAP header: %s", filepath);
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* tap_extensions[] = { ".tap", NULL };

const format_descriptor_t TAP_FORMAT_DESCRIPTOR = {
    "TAP",
    "Raw Tape Pulse Data",
    tap_extensions,
    FORMAT_CAP_STREAMABLE | FORMAT_CAP_METADATA,
    tap_identify,
    tap_load
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(TAP, &TAP_FORMAT_DESCRIPTOR)
