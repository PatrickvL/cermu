/**
 * NSF (NES Sound Format) File Parser — Implementation
 *
 * Handles identification, parsing, and loading of .nsf files.
 * NSF is the standard music format for the NES/Famicom, analogous
 * to SID files for the Commodore 64.
 */

#include "nsf_format.h"
#include "format_registry.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// NSF HEADER PARSING
// ============================================================================

bool nsf_parse_header(const uint8_t* data, size_t size, nsf_header_t* out) {
    if (!data || !out || size < 128) return false;

    // Verify magic: "NESM\x1A"
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' ||
        data[3] != 'M' || data[4] != 0x1A) {
        return false;
    }

    memset(out, 0, sizeof(nsf_header_t));

    // Identity
    memcpy(out->magic, data, 5);
    out->version    = data[0x05];

    // Song info
    out->num_songs  = data[0x06];
    out->start_song = data[0x07];

    // Addresses (little-endian)
    out->load_addr  = format_read_le16(data + 0x08);
    out->init_addr  = format_read_le16(data + 0x0A);
    out->play_addr  = format_read_le16(data + 0x0C);

    // Metadata strings (32 bytes each, null-terminated).
    // NSF is nominally ASCII but files in the wild may contain Latin-1
    // characters.  Keep raw bytes for NES display, UTF-8 for host APIs.
    memcpy(out->name,      data + 0x0E, 32);  out->name[31] = '\0';
    memcpy(out->artist,    data + 0x2E, 32);  out->artist[31] = '\0';
    memcpy(out->copyright, data + 0x4E, 32);  out->copyright[31] = '\0';

    // Preserve raw bytes before UTF-8 expansion
    memcpy(out->name_raw,      out->name,      32);  out->name_raw[31] = '\0';
    memcpy(out->artist_raw,    out->artist,    32);  out->artist_raw[31] = '\0';
    memcpy(out->copyright_raw, out->copyright, 32);  out->copyright_raw[31] = '\0';

    // Convert to UTF-8 for host display
    format_latin1_to_utf8_buf(out->name,      sizeof(out->name));
    format_latin1_to_utf8_buf(out->artist,    sizeof(out->artist));
    format_latin1_to_utf8_buf(out->copyright, sizeof(out->copyright));

    // Timing
    out->ntsc_speed = format_read_le16(data + 0x6E);
    out->pal_speed  = format_read_le16(data + 0x78);

    // Bankswitch init values
    memcpy(out->bankswitch, data + 0x70, 8);

    // Flags
    out->region_flags = data[0x7A];
    out->chip_flags   = data[0x7B];

    // Reserved
    memcpy(out->expansion, data + 0x7C, 4);

    // Computed: does this NSF use bankswitching?
    out->uses_bankswitching = false;
    for (int i = 0; i < 8; i++) {
        if (out->bankswitch[i] != 0) {
            out->uses_bankswitching = true;
            break;
        }
    }

    return true;
}

// ============================================================================
// NSF METADATA EXTRACTION
// ============================================================================

const nsf_header_t* nsf_get_metadata(const format_load_result_t* result) {
    if (!result) return nullptr;

    const auto* blob = reinterpret_cast<const nsf_metadata_blob_t*>(result->metadata);
    if (blob->tag != NSF_METADATA_TAG) return nullptr;

    return &blob->header;
}

// ============================================================================
// FORMAT IDENTIFICATION
// ============================================================================

static float nsf_identify(const uint8_t* data, size_t file_size, const char* extension) {
    // Content-based: check NSF magic "NESM\x1A"
    if (data && file_size >= 128) {
        if (data[0] == 'N' && data[1] == 'E' && data[2] == 'S' &&
            data[3] == 'M' && data[4] == 0x1A) {
            return 1.0f;  // Perfect match
        }
    }

    // Extension-based fallback
    if (extension && format_ext_match(extension, ".nsf")) {
        return 0.9f;
    }

    return 0.0f;
}

// ============================================================================
// FORMAT LOADING
// ============================================================================

static bool nsf_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    if (!data || !size || !out) return false;

    const uint8_t* file_data = data;
    size_t file_size = size;

    // Parse header
    nsf_header_t header;
    if (!nsf_parse_header(file_data, file_size, &header)) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "NSF: Invalid NSF data");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Payload starts at offset 128
    if (file_size <= 128) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "NSF: No payload data");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    size_t payload_size = file_size - 128;

    // Build FORMAT_LOAD_PROGRAM result
    out->type = FORMAT_LOAD_PROGRAM;
    out->program.load_addr = header.load_addr;
    out->program.data_size = payload_size;
    out->program.end_addr  = (uint16_t)(header.load_addr + payload_size - 1);
    out->program.data = (uint8_t*)malloc(payload_size);
    if (!out->program.data) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "NSF: Out of memory for payload (%zu bytes)", payload_size);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }
    memcpy(out->program.data, file_data + 128, payload_size);

    // Store parsed header as metadata so the NES system can detect NSF files
    if (sizeof(nsf_metadata_blob_t) <= FORMAT_METADATA_MAX_SIZE) {
        nsf_metadata_blob_t* blob = (nsf_metadata_blob_t*)out->metadata;
        blob->tag = NSF_METADATA_TAG;
        blob->header = header;
        out->metadata_size = sizeof(nsf_metadata_blob_t);
    }

    printf("NSF: \"%s\" by %s\n", header.name, header.artist);
    printf("NSF: %d songs, start=%d, load=$%04X init=$%04X play=$%04X\n",
           header.num_songs, header.start_song,
           header.load_addr, header.init_addr, header.play_addr);
    printf("NSF: Payload: $%04X-$%04X (%zu bytes)\n",
           header.load_addr,
           (unsigned)(header.load_addr + payload_size - 1),
           payload_size);

    if (header.uses_bankswitching) {
        printf("NSF: Uses bankswitching — banks:");
        for (int i = 0; i < 8; i++) printf(" %02X", header.bankswitch[i]);
        printf("\n");
    }

    if (header.chip_flags) {
        printf("NSF: Extra chips: 0x%02X", header.chip_flags);
        if (header.chip_flags & NSF_CHIP_VRC6)      printf(" VRC6");
        if (header.chip_flags & NSF_CHIP_VRC7)       printf(" VRC7");
        if (header.chip_flags & NSF_CHIP_FDS)        printf(" FDS");
        if (header.chip_flags & NSF_CHIP_MMC5)       printf(" MMC5");
        if (header.chip_flags & NSF_CHIP_NAMCO163)   printf(" N163");
        if (header.chip_flags & NSF_CHIP_SUNSOFT5B)  printf(" 5B");
        printf("\n");
    }

    return true;
}

// ============================================================================
// FORMAT DESCRIPTOR
// ============================================================================

static const char* nsf_extensions[] = { ".nsf", nullptr };

const format_descriptor_t NSF_FORMAT_DESCRIPTOR = {
    "NSF",                                  // name
    "NES Sound Format music file",          // description
    nsf_extensions,                         // extensions
    FORMAT_CAP_LOADABLE | FORMAT_CAP_METADATA,  // capabilities
    nsf_identify,                           // identify
    nsf_load,                               // load
    nullptr,                                // list_entries
    nullptr,                                // extract_entry
};

REGISTER_FORMAT(NSF, &NSF_FORMAT_DESCRIPTOR)
