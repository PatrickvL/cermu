/**
 * SNA Format Handler — Implementation
 *
 * Parses ZX Spectrum .sna snapshot files.
 * Format loads as FORMAT_LOAD_RAW with metadata containing the full
 * sna_header_t so the system can restore Z80 registers and ULA state.
 */

#include "core/cermu.hpp"
#include "core/formats/sna_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Constants
// ============================================================================

static constexpr size_t SNA_HEADER_SIZE  = 27;
static constexpr size_t SNA_48K_SIZE     = 49179;   // 27 + 48K
static constexpr size_t SNA_128K_SIZE    = 131103;   // 49179 + 2(PC) + 1(7FFD) + 1(TRDOS) + 5*16384
static constexpr size_t SNA_128K_EXT_SIZE = 147487;  // With all 8 banks

// ============================================================================
// Header Parsing
// ============================================================================

bool sna_parse_header(const uint8_t* data, size_t size, sna_header_t* out) {
    if (!data || !out || size < SNA_48K_SIZE) return false;
    std::memset(out, 0, sizeof(*out));

    out->i_reg    = data[0];
    out->hl_prime = format_read_le16(data + 1);
    out->de_prime = format_read_le16(data + 3);
    out->bc_prime = format_read_le16(data + 5);
    out->af_prime = format_read_le16(data + 7);
    out->hl       = format_read_le16(data + 9);
    out->de       = format_read_le16(data + 11);
    out->bc       = format_read_le16(data + 13);
    out->iy       = format_read_le16(data + 15);
    out->ix       = format_read_le16(data + 17);
    out->iff2     = data[19];
    out->r_reg    = data[20];
    out->af       = format_read_le16(data + 21);
    out->sp       = format_read_le16(data + 23);
    out->int_mode = data[25];
    out->border   = data[26] & 0x07;

    // 128K extension
    if (size > SNA_48K_SIZE) {
        out->pc        = format_read_le16(data + SNA_48K_SIZE);
        out->port_7ffd = data[SNA_48K_SIZE + 2];
        out->trdos_rom = data[SNA_48K_SIZE + 3];
    }

    return true;
}

// ============================================================================
// Format Identification
// ============================================================================

static float sna_identify(const uint8_t* data, size_t file_size, const char* extension) {
    // Extension match
    bool ext_match = extension && format_ext_match(extension, ".sna");

    // Size-based validation: must be exact 48K or 128K snapshot size
    bool valid_48k  = (file_size == SNA_48K_SIZE);
    bool valid_128k = (file_size == SNA_128K_SIZE || file_size == SNA_128K_EXT_SIZE);

    if (ext_match && (valid_48k || valid_128k)) return 0.95f;
    if (ext_match) return 0.6f;  // Right extension but unusual size
    if (valid_48k || valid_128k) return 0.3f;  // Right size but no extension
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool sna_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    sna_header_t header;
    if (!sna_parse_header(data, size, &header)) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Invalid SNA file (expected %zu bytes, got %zu)", SNA_48K_SIZE, size);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Validate header fields
    if (header.int_mode > 2) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Invalid SNA interrupt mode: %d", header.int_mode);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    // Copy the RAM dump (starts at offset 27, 48KB for standard snapshots)
    size_t ram_size = size - SNA_HEADER_SIZE;
    if (size > SNA_48K_SIZE) {
        // 128K: include extension header + extra banks
        ram_size = size - SNA_HEADER_SIZE;
    }

    out->program.data = static_cast<uint8_t*>(std::malloc(ram_size));
    if (!out->program.data) {
        snprintf(out->error_msg, sizeof(out->error_msg), "SNA: out of memory");
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }
    std::memcpy(out->program.data, data + SNA_HEADER_SIZE, ram_size);
    out->program.data_size = ram_size;
    out->program.load_addr = 0x4000;  // RAM starts at $4000
    out->program.end_addr  = 0xFFFF;

    // Store the full header in metadata so the system can restore registers
    static_assert(sizeof(sna_header_t) <= FORMAT_METADATA_MAX_SIZE,
                  "sna_header_t must fit in metadata blob");
    std::memcpy(out->metadata, &header, sizeof(header));
    out->metadata_size = sizeof(header);

    out->type = FORMAT_LOAD_RAW;
    log_info("SNA: Parsed %s snapshot (SP=$%04X, IM=%d, border=%d)\n",
           size == SNA_48K_SIZE ? "48K" : "128K",
           header.sp, header.int_mode, header.border);
    return true;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* sna_extensions[] = { ".sna", nullptr };

const format_descriptor_t SNA_FORMAT_DESCRIPTOR = {
    "SNA",
    "ZX Spectrum Snapshot",
    sna_extensions,
    FORMAT_CAP_LOADABLE,
    sna_identify,
    sna_load,
    nullptr,  // list_entries
    nullptr   // extract_entry
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(SNA, &SNA_FORMAT_DESCRIPTOR)
