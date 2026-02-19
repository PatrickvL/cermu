/**
 * TAP Format Handler — Implementation
 */

#include "tap_format.h"
#include "format_registry.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// TAP Header Reading
// ============================================================================

bool commodore_tap_read_header(const char* filepath, commodore_tap_header_t* out_header) {
    if (!filepath || !out_header) return false;
    memset(out_header, 0, sizeof(*out_header));

    FILE* f = fopen(filepath, "rb");
    if (!f) return false;

    uint8_t raw[20];
    if (fread(raw, 1, 20, f) != 20) {
        fclose(f);
        return false;
    }
    fclose(f);

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
        return false;
    }

    printf("TAPFormat: sig=%.12s ver=%d platform=%d std=%d size=%u\n",
           out_header->signature, out_header->version, out_header->platform,
           out_header->video_standard, out_header->data_size);
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
