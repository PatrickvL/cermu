/**
 * D71 Format Handler — 1571 Disk Image
 *
 * The 1571 is a double-sided 1541: 70 tracks (35 per side), same sector
 * geometry.  The directory and BAM structures are identical to D64,
 * with an additional BAM sector at track 53, sector 0 for the second side.
 *
 * This handler delegates to the existing commodore_d64_t infrastructure,
 * which has been extended to support 70-track images.
 */

#include "core/cermu.hpp"
#include "core/formats/d64_format.hpp"
#include "core/formats/format_registry.hpp"
#include "systems/commodore/petscii.hpp"
#include <cstring>

// ============================================================================
// Format Identification
// ============================================================================

static float d71_identify(const uint8_t* /*data*/, size_t file_size, const char* extension) {
    if (file_size == D71_STANDARD_SIZE || file_size == D71_STANDARD_SIZE_ERR) {
        return 0.95f;
    }
    if (extension && format_ext_match(extension, ".d71")) return 0.8f;
    return 0.0f;
}

// ============================================================================
// Load Callback — delegates to commodore_d64_t (same directory/file structure)
// ============================================================================

static bool d71_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    commodore_d64_t d71;
    if (d71.open_mem(data, size)) {
        if (d71.extract_first_prg(&out->program)) {
            out->type = FORMAT_LOAD_PROGRAM;
            d71.close();
            return true;
        }
        d71.close();
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "D71 opened but no PRG found");
    } else {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Failed to open D71 from memory");
    }
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

// ============================================================================
// Container Interface — directory listing and file extraction
// ============================================================================

static int d71_list_entries(const uint8_t* data, size_t size,
                            format_container_entry_t* entries, int max_entries) {
    commodore_d64_t d71{};
    if (!d71.open_mem(data, size)) return -1;

    commodore_d64_directory_t dir{};
    if (!d71.read_directory(&dir)) { d71.close(); return -1; }

    int count = 0;
    for (int i = 0; i < dir.count && count < max_entries; ++i) {
        const auto& de = dir.entries[i];
        if ((de.file_type & D64_FTYPE_MASK) == D64_FTYPE_DEL) continue;

        char name_buf[17]{};
        memcpy(name_buf, de.filename, 16);
        for (int j = 0; j < 16 && name_buf[j]; ++j)
            name_buf[j] = petscii_to_ascii(static_cast<uint8_t>(name_buf[j]));

        auto& entry = entries[count];
        const char* type_ext = ".prg";
        switch (de.file_type & D64_FTYPE_MASK) {
            case D64_FTYPE_SEQ: type_ext = ".seq"; break;
            case D64_FTYPE_USR: type_ext = ".usr"; break;
            case D64_FTYPE_REL: type_ext = ".rel"; break;
            default: break;
        }
        snprintf(entry.display_name, sizeof(entry.display_name),
                 "%s%s", name_buf, type_ext);
        entry.size  = static_cast<size_t>(de.size_blocks) * 254;
        entry.index = i;
        ++count;
    }

    d71.close();
    return count;
}

static bool d71_extract_entry(const uint8_t* data, size_t size,
                              int entry_index, uint8_t** out_data, size_t* out_size) {
    commodore_d64_t d71{};
    if (!d71.open_mem(data, size)) return false;
    bool ok = d71.extract_file(entry_index, out_data, out_size);
    d71.close();
    return ok;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* d71_extensions[] = { ".d71", NULL };

static const format_descriptor_t D71_FORMAT_DESCRIPTOR = {
    "D71",
    "1571 Disk Image",
    d71_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER | FORMAT_CAP_VOLUME,
    d71_identify,
    d71_load,
    d71_list_entries,
    d71_extract_entry
};

REGISTER_FORMAT(D71, &D71_FORMAT_DESCRIPTOR)
