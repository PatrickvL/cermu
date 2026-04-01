/**
 * SID Format Handler — Implementation
 *
 * Parses PSID/RSID file headers, extracts the C64 payload, and returns
 * a FORMAT_LOAD_PROGRAM result with SID metadata in the metadata blob.
 */

#include "core/cermu.hpp"
#include "core/formats/sid_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Header Parsing
// ============================================================================

/**
 * Minimum SID file header sizes by version:
 *   v1: $76 bytes (ends before flags)
 *   v2+: $7C bytes (includes flags, reloc, second SID)
 */
#define SID_V1_HEADER_SIZE  0x76
#define SID_V2_HEADER_SIZE  0x7C

bool sid_parse_header(const uint8_t* data, size_t size, sid_header_t* out) {
    if (!data || !out) return false;
    memset(out, 0, sizeof(*out));

    // Need at least 4 bytes for magic
    if (size < 4) return false;

    // Check magic: "PSID" or "RSID"
    if (memcmp(data, "PSID", 4) == 0) {
        out->type = SID_TYPE_PSID;
    } else if (memcmp(data, "RSID", 4) == 0) {
        out->type = SID_TYPE_RSID;
    } else {
        return false;
    }

    // Read version and data offset (both BE16)
    out->version     = format_read_be16(data + 0x04);
    out->data_offset = format_read_be16(data + 0x06);

    // Validate version
    if (out->version < 1 || out->version > 4) {
        log_info("SIDFormat: Unsupported version %u\n", out->version);
        return false;
    }

    // Validate minimum header size
    size_t min_header = (out->version == 1) ? SID_V1_HEADER_SIZE : SID_V2_HEADER_SIZE;
    if (size < min_header || size < out->data_offset) {
        log_info("SIDFormat: File too small (%zu bytes, need %zu)\n", size, min_header);
        return false;
    }

    // Core addresses
    out->load_addr  = format_read_be16(data + 0x08);
    out->init_addr  = format_read_be16(data + 0x0A);
    out->play_addr  = format_read_be16(data + 0x0C);

    // Subtune info
    out->num_songs   = format_read_be16(data + 0x0E);
    out->start_song  = format_read_be16(data + 0x10);
    out->speed_flags = format_read_be32(data + 0x12);

    // Clamp subtune counts
    if (out->num_songs == 0) out->num_songs = 1;
    if (out->start_song == 0) out->start_song = 1;
    if (out->start_song > out->num_songs) out->start_song = 1;

    // Metadata strings (32 bytes each, may not be null-terminated in file).
    // SID spec defines these as ISO 8859-1 (Latin-1).  We keep the raw bytes
    // for native C64 screen display (PETSCII/Latin-1 → screen codes) and also
    // produce UTF-8 versions for host APIs (SDL window title, ImGui).
    memcpy(out->name,     data + 0x16, 32); out->name[32] = '\0';
    memcpy(out->author,   data + 0x36, 32); out->author[32] = '\0';
    memcpy(out->released, data + 0x56, 32); out->released[32] = '\0';

    // Preserve raw bytes before UTF-8 expansion
    memcpy(out->name_raw,     out->name,     33);
    memcpy(out->author_raw,   out->author,   33);
    memcpy(out->released_raw, out->released, 33);

    // Convert to UTF-8 for host display (bytes 0x80-0xFF → 2-byte sequences)
    format_latin1_to_utf8_buf(out->name,     sizeof(out->name));
    format_latin1_to_utf8_buf(out->author,   sizeof(out->author));
    format_latin1_to_utf8_buf(out->released, sizeof(out->released));

    // Version 2+ flags
    if (out->version >= 2 && size >= SID_V2_HEADER_SIZE) {
        out->flags       = format_read_be16(data + 0x76);
        out->start_page  = data[0x78];
        out->page_length = data[0x79];
        out->second_sid_addr = data[0x7A];
        out->third_sid_addr  = data[0x7B];

        // Decode flags
        out->video     = (sid_video_t)((out->flags >> 4) & 0x03);
        out->sid_model = (sid_model_t)((out->flags >> 6) & 0x03);

        if (out->version >= 3) {
            out->sid2_model = (sid_model_t)((out->flags >> 8) & 0x03);
        }
        if (out->version >= 4) {
            out->sid3_model = (sid_model_t)((out->flags >> 10) & 0x03);
        }
    }

    // Resolve load address from payload if header says 0
    if (out->load_addr == 0) {
        // The first 2 bytes of the C64 payload are the little-endian load address
        size_t payload_offset = out->data_offset;
        if (payload_offset + 2 > size) {
            log_info("SIDFormat: No room for embedded load address\n");
            return false;
        }
        out->load_addr = format_read_le16(data + payload_offset);
    }

    // RSID constraints validation
    if (out->type == SID_TYPE_RSID) {
        // RSID play_addr must be 0 (tune provides its own IRQ handler).
        // RSID init_addr == 0 is valid — it indicates a BASIC program SID
        // that should be loaded at the BASIC start address and RUN.
        if (out->play_addr != 0) {
            log_info("SIDFormat: RSID with play_addr != 0 is invalid\n");
            return false;
        }
    }

    return true;
}

const sid_header_t* sid_get_metadata(const format_load_result_t* result) {
    if (!result) return NULL;
    if (result->metadata_size < sizeof(sid_metadata_blob_t)) return NULL;

    const sid_metadata_blob_t* blob = (const sid_metadata_blob_t*)result->metadata;
    if (blob->tag != SID_METADATA_TAG) return NULL;

    return &blob->header;
}

// ============================================================================
// Format Identification
// ============================================================================

static float sid_identify(const uint8_t* data, size_t file_size, const char* extension) {
    // Content-based: check magic bytes
    if (data && file_size >= 4) {
        if (memcmp(data, "PSID", 4) == 0 || memcmp(data, "RSID", 4) == 0) {
            return 1.0f;  // Perfect confidence — unambiguous magic
        }
    }

    // Extension-based fallback
    if (extension && format_ext_match(extension, ".sid")) {
        return 0.9f;
    }

    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool sid_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (!data || !size || !out) return false;

    const uint8_t* file_data = data;
    size_t file_size = size;

    // Parse header
    sid_header_t header;
    if (!sid_parse_header(file_data, file_size, &header)) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "SID: Invalid SID data");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Determine payload offset and size
    // If load_addr was 0 in header, the first 2 payload bytes are the address
    // and the actual data starts 2 bytes later
    size_t payload_start = header.data_offset;
    uint16_t load_addr_from_header = format_read_be16(file_data + 0x08);

    if (load_addr_from_header == 0) {
        // Skip the embedded 2-byte load address in the payload
        payload_start += 2;
    }

    if (payload_start >= file_size) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "SID: No payload data");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    size_t payload_size = file_size - payload_start;

    // Validate payload fits in C64 address space
    if ((uint32_t)header.load_addr + payload_size > 0x10000) {
        log_info("SIDFormat: Warning — payload exceeds 64KB (load=$%04X, size=%zu), truncating\n",
               header.load_addr, payload_size);
        payload_size = 0x10000 - header.load_addr;
    }

    // Build FORMAT_LOAD_PROGRAM result
    out->type = FORMAT_LOAD_PROGRAM;
    out->program.load_addr = header.load_addr;
    out->program.data_size = payload_size;
    out->program.end_addr  = (uint16_t)(header.load_addr + payload_size);
    out->program.data = (uint8_t*)malloc(payload_size);
    if (!out->program.data) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "SID: Out of memory for payload (%zu bytes)", payload_size);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }
    memcpy(out->program.data, file_data + payload_start, payload_size);

    // Store parsed header as metadata so C64System can detect SID files
    // and inject the player stub instead of BASIC auto-run
    if (sizeof(sid_metadata_blob_t) <= FORMAT_METADATA_MAX_SIZE) {
        sid_metadata_blob_t* blob = (sid_metadata_blob_t*)out->metadata;
        blob->tag = SID_METADATA_TAG;
        blob->header = header;
        out->metadata_size = sizeof(sid_metadata_blob_t);
    }

    log_info("SIDFormat: %s v%u — \"%s\" by %s\n",
           header.type == SID_TYPE_PSID ? "PSID" : "RSID",
           header.version, header.name, header.author);
    log_info("SIDFormat: load=$%04X init=$%04X play=$%04X songs=%u default=%u\n",
           header.load_addr, header.init_addr, header.play_addr,
           header.num_songs, header.start_song);
    if (header.version >= 2) {
        const char* vid_names[] = {"Unknown", "PAL", "NTSC", "PAL+NTSC"};
        const char* sid_names[] = {"Unknown", "6581", "8580", "6581+8580"};
        log_info("SIDFormat: video=%s sid=%s\n",
               vid_names[header.video & 3], sid_names[header.sid_model & 3]);
    }

    return true;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* sid_extensions[] = { ".sid", NULL };

const format_descriptor_t SID_FORMAT_DESCRIPTOR = {
    "SID",
    "C64 SID Music File (PSID/RSID)",
    sid_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_METADATA,
    sid_identify,
    sid_load,
    nullptr,  // list_entries
    nullptr   // extract_entry
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(SID, &SID_FORMAT_DESCRIPTOR)
