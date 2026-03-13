/**
 * TAP Format Handler — Implementation
 */

#include "core/formats/tap_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdio>
#include <cstring>

// ============================================================================
// TAP Header Reading
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
    if (extension && format_ext_match(extension, ".tap")) {
        // Spectrum TAP files share the .tap extension but have no magic.
        // Detect the Spectrum block structure (first block: length=19, flag=$00)
        // and yield so the Spectrum TAP handler can claim it instead.
        if (data && file_size >= 21) {
            uint16_t first_len = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
            if (first_len == 19 && data[2] == 0x00)
                return 0.1f;
        }
        return 0.8f;
    }
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool tap_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    commodore_tap_header_t hdr;
    if (commodore_tap_read_header_mem(data, size, &hdr)) {
        out->type = FORMAT_LOAD_METADATA;
        memcpy(out->metadata, &hdr, sizeof(hdr));
        out->metadata_size = sizeof(hdr);
        return true;
    }
    snprintf(out->error_msg, sizeof(out->error_msg),
             "Failed to read TAP header from memory");
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
    tap_load,
    nullptr,  // list_entries
    nullptr   // extract_entry
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(TAP, &TAP_FORMAT_DESCRIPTOR)
