/**
 * FDS Format Handler — Implementation
 *
 * Parses Famicom Disk System disk images (.fds) and produces a
 * FORMAT_LOAD_RAW result with the raw disk side data.  The NES system
 * layer uses the metadata to configure the FDS mapper.
 */

#include "core/cermu.hpp"
#include "core/formats/fds_format.hpp"
#include "core/formats/format_registry.hpp"
#include <cstring>

// ============================================================================
// Format Identification
// ============================================================================

static float fds_identify(const uint8_t* data, size_t file_size, const char* extension) {
    // Headered FDS: starts with "FDS\x1a"
    if (file_size >= FDS_HEADER_SIZE + FDS_SIDE_SIZE &&
        data[0] == 'F' && data[1] == 'D' && data[2] == 'S' && data[3] == 0x1A) {
        return 0.95f;
    }

    // Headerless FDS: raw disk data, first byte should be $01 (disk info block)
    if (file_size >= FDS_SIDE_SIZE && (file_size % FDS_SIDE_SIZE) == 0 &&
        data[0] == 0x01) {
        // Additional sanity: bytes 1-14 should contain "*NINTENDO-HVC*"
        if (file_size >= 15 && memcmp(data + 1, "*NINTENDO-HVC*", 14) == 0) {
            return 0.95f;
        }
        return 0.7f;
    }

    // Extension-only fallback
    if (extension && format_ext_match(extension, ".fds")) return 0.6f;
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool fds_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    const uint8_t* disk_data = data;
    size_t disk_size = size;
    bool has_header = false;

    // Check for fwNES header
    if (size >= FDS_HEADER_SIZE &&
        data[0] == 'F' && data[1] == 'D' && data[2] == 'S' && data[3] == 0x1A) {
        has_header = true;
        disk_data = data + FDS_HEADER_SIZE;
        disk_size = size - FDS_HEADER_SIZE;
    }

    // Validate disk side count
    uint8_t num_sides = static_cast<uint8_t>(disk_size / FDS_SIDE_SIZE);
    if (num_sides == 0 || num_sides > FDS_MAX_SIDES) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "FDS: Invalid disk size %zu (expected %d per side)",
                 disk_size, FDS_SIDE_SIZE);
        out->type = FORMAT_LOAD_ERROR;
        return false;
    }

    size_t total = static_cast<size_t>(num_sides) * FDS_SIDE_SIZE;

    // Allocate and copy raw disk data
    out->program.data = static_cast<uint8_t*>(malloc(total));
    if (!out->program.data) {
        out->type = FORMAT_LOAD_ERROR;
        snprintf(out->error_msg, sizeof(out->error_msg), "FDS: allocation failed");
        return false;
    }
    memcpy(out->program.data, disk_data, total);
    out->program.data_size = total;
    out->program.load_addr = 0;
    out->program.end_addr = 0;
    out->type = FORMAT_LOAD_RAW;

    // Fill metadata for the system layer
    fds_metadata_t meta{};
    meta.num_sides = num_sides;

    // Extract game name from disk info block (block 1, offset 16, 3 bytes manufacturer + game name)
    if (total >= 56) {
        // Disk info block: byte 0 = $01, bytes 16-19 = game name (not great, but standard)
        // The real game name is at offset 16 (4 bytes), but most tools use the full
        // block for identification.  We'll grab what's available.
        memset(meta.game_name, 0, sizeof(meta.game_name));
    }

    static_assert(sizeof(fds_metadata_t) <= FORMAT_METADATA_MAX_SIZE,
                  "FDS metadata exceeds metadata buffer");
    memcpy(out->metadata, &meta, sizeof(meta));
    out->metadata_size = sizeof(meta);

    log_info("FDS: Loaded %d disk side%s (%zu bytes%s)\n",
             num_sides, num_sides > 1 ? "s" : "", total,
             has_header ? ", fwNES header stripped" : "");

    return true;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* fds_extensions[] = { ".fds", NULL };

const format_descriptor_t FDS_FORMAT_DESCRIPTOR = {
    "FDS",
    "Famicom Disk System Image",
    fds_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_METADATA,
    fds_identify,
    fds_load,
    nullptr,    // no list_entries (single disk image)
    nullptr     // no extract_entry
};

REGISTER_FORMAT(FDS, &FDS_FORMAT_DESCRIPTOR)
