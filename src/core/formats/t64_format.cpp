/**
 * T64 Format Handler — Implementation
 */

#include "t64_format.h"
#include "format_registry.h"
#include "../encoding/petscii.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Open / Close
// ============================================================================

bool commodore_t64_t::open(const char* filepath) {
    if (!filepath) return false;
    memset(this, 0, sizeof(*this));

    data = format_read_entire_file(filepath, &data_size);
    if (!data) {
        printf("T64Format: Cannot open T64 file: %s\n", filepath);
        return false;
    }
    owns_data = true;

    /* Verify T64 signature: "C64 tape image file" or "C64S tape image file" */
    if (data_size < T64_HEADER_SIZE ||
        memcmp(data, "C64", 3) != 0) {
        printf("T64Format: Invalid T64 signature\n");
        free(data);
        data = NULL;
        return false;
    }

    printf("T64Format: Opened T64: %zu bytes\n", data_size);
    return true;
}

void commodore_t64_t::close() {
    if (owns_data && data) free(data);
    memset(this, 0, sizeof(*this));
}

// ============================================================================
// Directory
// ============================================================================

bool commodore_t64_t::read_directory(commodore_t64_directory_t* out_dir) const {
    if (!data || !out_dir) return false;
    memset(out_dir, 0, sizeof(*out_dir));

    const uint8_t* hdr = data;

    out_dir->version     = format_read_le16(hdr + 0x20);
    out_dir->max_entries = format_read_le16(hdr + 0x22);
    out_dir->used_entries = format_read_le16(hdr + 0x24);

    memcpy(out_dir->tape_name, hdr + 0x28, 24);
    out_dir->tape_name[24] = '\0';
    petscii_trim_padding(out_dir->tape_name, 24);

    int count = 0;
    int max = out_dir->max_entries;
    if (max > T64_MAX_ENTRIES) max = T64_MAX_ENTRIES;

    for (int i = 0; i < max && count < T64_MAX_ENTRIES; i++) {
        size_t entry_offset = T64_HEADER_SIZE + (size_t)i * T64_ENTRY_SIZE;
        if (entry_offset + T64_ENTRY_SIZE > data_size) break;

        const uint8_t* e = data + entry_offset;

        uint8_t  c64s_type = e[0];
        uint8_t  file_type = e[1];
        uint16_t start     = format_read_le16(e + 2);
        uint16_t end       = format_read_le16(e + 4);
        uint32_t data_off  = format_read_le32(e + 8);

        if (c64s_type == 0) continue;

        commodore_t64_entry_t* te = &out_dir->entries[count];
        te->c64s_file_type = c64s_type;
        te->file_type      = file_type;
        te->start_addr     = start;
        te->end_addr       = end;
        te->data_offset    = data_off;
        te->data_size      = (end > start) ? (end - start) : 0;

        memcpy(te->filename, e + 16, 16);
        te->filename[16] = '\0';
        petscii_trim_padding(te->filename, 16);

        count++;
    }

    out_dir->count = count;
    printf("T64Format: Directory: \"%s\", %d entries\n", out_dir->tape_name, count);
    return true;
}

// ============================================================================
// File Extraction
// ============================================================================

bool commodore_t64_t::extract_file(int entry_idx, commodore_prg_t* out_prg) const {
    if (!out_prg) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    commodore_t64_directory_t dir;
    if (!read_directory(&dir)) return false;
    if (entry_idx < 0 || entry_idx >= dir.count) return false;

    const commodore_t64_entry_t* entry = &dir.entries[entry_idx];

    if (entry->data_size == 0 || entry->data_offset == 0) {
        printf("T64Format: Entry %d has no data\n", entry_idx);
        return false;
    }

    if (entry->data_offset + entry->data_size > data_size) {
        printf("T64Format: Entry %d data extends beyond file\n", entry_idx);
        return false;
    }

    out_prg->load_addr = entry->start_addr;
    out_prg->end_addr  = entry->end_addr;
    out_prg->data_size = entry->data_size;
    out_prg->data = (uint8_t*)malloc(entry->data_size);
    if (!out_prg->data) return false;

    memcpy(out_prg->data, data + entry->data_offset, entry->data_size);

    printf("T64Format: Extracted \"%s\": load=$%04X size=%u\n",
           entry->filename, entry->start_addr, entry->data_size);
    return true;
}

bool commodore_t64_t::extract_first_prg(commodore_prg_t* out_prg) const {
    if (!out_prg) return false;

    commodore_t64_directory_t dir;
    if (!read_directory(&dir)) return false;

    for (int i = 0; i < dir.count; i++) {
        if (dir.entries[i].c64s_file_type == 1 && dir.entries[i].data_size > 0)
            return extract_file(i, out_prg);
    }

    printf("T64Format: No valid entries found\n");
    return false;
}

// ============================================================================
// Format Identification
// ============================================================================

static float t64_identify(const uint8_t* data, size_t file_size, const char* extension) {
    /* Check for "C64" signature at start */
    if (file_size >= T64_HEADER_SIZE && data && memcmp(data, "C64", 3) == 0)
        return 0.95f;
    if (extension && format_ext_match(extension, ".t64")) return 0.8f;
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool t64_load(const char* filepath, format_load_result_t* out) {
    commodore_t64_t t64;
    if (t64.open(filepath)) {
        if (t64.extract_first_prg(&out->program)) {
            out->type = FORMAT_LOAD_PROGRAM;
            t64.close();
            return true;
        }
        t64.close();
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "T64 opened but no PRG found: %s", filepath);
    } else {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Failed to open T64: %s", filepath);
    }
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

// ============================================================================
// Format Descriptor
// ============================================================================

static const char* t64_extensions[] = { ".t64", NULL };

const format_descriptor_t T64_FORMAT_DESCRIPTOR = {
    "T64",
    "Tape Archive Image",
    t64_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER,
    t64_identify,
    t64_load
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(T64, &T64_FORMAT_DESCRIPTOR)
