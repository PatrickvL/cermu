/**
 * LNX Format Handler — Implementation
 *
 * Lynx archive format (created by cbmconvert, Will Corley's original, etc.):
 *
 * 1. BASIC dissolve stub — a short C64/VIC-20 BASIC program that prints
 *    "USE LYNX TO DISSOLVE THIS FILE".  Ends with \0\0\0\r pattern.
 * 2. Directory header:
 *      " <block_count>  <signature containing 'LYNX'>\r <file_count> \r"
 * 3. Per-file directory entry (repeated file_count times):
 *      <filename up to 16 chars>\r <blocks>\r <type_char>\r <last_block_len>\r
 * 4. Data section starts at byte offset (block_count * 254).
 *
 * Reference: cbmconvert lynx.c by Marko Mäkelä.
 */

#include "lnx_format.h"
#include "format_registry.h"
#include "../encoding/petscii.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Open / Close
// ============================================================================

bool commodore_lynx_t::open_mem(const uint8_t* buf, size_t size) {
    if (!buf) return false;
    memset(this, 0, sizeof(*this));

    data = const_cast<uint8_t*>(buf);
    data_size = size;
    owns_data = false;

    if (data_size < 100) return false;
    return true;
}

void commodore_lynx_t::close() {
    if (owns_data && data) free(data);
    memset(this, 0, sizeof(*this));
}

// ============================================================================
// Directory
// ============================================================================

bool commodore_lynx_t::read_directory(commodore_lynx_directory_t* out_dir) const {
    if (!data || !out_dir) return false;
    memset(out_dir, 0, sizeof(*out_dir));

    const uint8_t* buf = data;
    size_t size = data_size;

    /* Step 1: Skip the BASIC dissolve header by finding \0\0\0\r pattern */
    size_t pos = 4;
    size_t max_header = (size < LNX_MAX_BASIC_LENGTH) ? size : LNX_MAX_BASIC_LENGTH;
    bool found_header_end = false;
    for (; pos < max_header; pos++) {
        if (buf[pos-4] == 0x00 && buf[pos-3] == 0x00 &&
            buf[pos-2] == 0x00 && buf[pos-1] == 0x0D) {
            found_header_end = true;
            break;
        }
    }
    if (!found_header_end) pos = 0;  /* Try without BASIC header */

    /* Step 2: Parse " <blkcount>  <signature containing 'LYNX'>\r <fcount> \r" */
    while (pos < size && buf[pos] == ' ') pos++;
    unsigned blkcount = 0;
    while (pos < size && buf[pos] >= '0' && buf[pos] <= '9') {
        blkcount = blkcount * 10 + (buf[pos] - '0');
        pos++;
    }
    if (blkcount == 0) {
        printf("LNXFormat: Invalid block count\n");
        return false;
    }

    /* Skip spaces and signature text until CR */
    while (pos < size && buf[pos] != 0x0D) pos++;
    if (pos >= size) return false;

    /* Verify "LYNX" appeared in the signature area */
    bool has_lynx_sig = false;
    {
        size_t scan_start = (pos > 30) ? pos - 30 : 0;
        for (size_t s = scan_start; s + 3 < pos; s++) {
            if (buf[s] == 'L' && buf[s+1] == 'Y' && buf[s+2] == 'N' && buf[s+3] == 'X') {
                has_lynx_sig = true;
                break;
            }
        }
    }
    if (!has_lynx_sig) {
        printf("LNXFormat: LYNX signature not found\n");
        return false;
    }
    pos++; /* skip CR */

    /* Read file count */
    while (pos < size && (buf[pos] == ' ' || buf[pos] == 0x0D)) pos++;
    unsigned fcount = 0;
    while (pos < size && buf[pos] >= '0' && buf[pos] <= '9') {
        fcount = fcount * 10 + (buf[pos] - '0');
        pos++;
    }
    while (pos < size && (buf[pos] == ' ' || buf[pos] == 0x0D)) pos++;

    if (fcount == 0 || fcount > LNX_MAX_FILES) {
        printf("LNXFormat: Invalid file count: %u\n", fcount);
        return false;
    }

    out_dir->header_blocks = blkcount;
    out_dir->file_count    = fcount;

    printf("LNXFormat: header_blocks=%u, files=%u, data_start=%u\n",
           blkcount, fcount, blkcount * 254);

    /* Step 3: Parse per-file entries */
    size_t archive_pos = (size_t)blkcount * 254;

    for (unsigned f = 0; f < fcount && pos < size; f++) {
        commodore_lynx_entry_t* entry = &out_dir->entries[f];

        /* Filename (up to 16 chars, CR-terminated) */
        int name_len = 0;
        while (pos < size && buf[pos] != 0x0D && name_len < 16) {
            entry->filename[name_len++] = (char)buf[pos++];
        }
        entry->filename[name_len] = '\0';
        for (int t = name_len - 1; t >= 0; t--) {
            if ((uint8_t)entry->filename[t] == 0xA0 || entry->filename[t] == ' ')
                entry->filename[t] = '\0';
            else break;
        }
        if (pos < size && buf[pos] == 0x0D) pos++;

        /* Block count */
        while (pos < size && buf[pos] == ' ') pos++;
        unsigned blocks = 0;
        while (pos < size && buf[pos] >= '0' && buf[pos] <= '9') {
            blocks = blocks * 10 + (buf[pos] - '0');
            pos++;
        }
        while (pos < size && (buf[pos] == ' ' || buf[pos] == 0x0D)) pos++;

        /* File type character */
        char ftype = 'P';
        if (pos < size && buf[pos] != 0x0D) ftype = (char)buf[pos++];
        if (pos < size && buf[pos] == 0x0D) pos++;

        /* Last block length */
        while (pos < size && buf[pos] == ' ') pos++;
        unsigned last_len = 0;
        while (pos < size && buf[pos] >= '0' && buf[pos] <= '9') {
            last_len = last_len * 10 + (buf[pos] - '0');
            pos++;
        }
        while (pos < size && (buf[pos] == ' ' || buf[pos] == 0x0D)) pos++;

        entry->file_type      = ftype;
        entry->blocks         = blocks;
        entry->last_block_len = last_len;
        entry->data_offset    = archive_pos;

        if (blocks > 0 && last_len > 0)
            entry->data_length = (size_t)last_len + (size_t)blocks * 254 - 255;
        else
            entry->data_length = 0;

        printf("LNXFormat: [%u] \"%s\" type=%c blocks=%u lastlen=%u len=%zu off=%zu\n",
               f, entry->filename, ftype, blocks, last_len,
               entry->data_length, entry->data_offset);

        archive_pos += (size_t)blocks * 254;
    }

    return true;
}

// ============================================================================
// File Extraction
// ============================================================================

bool commodore_lynx_t::extract_file(const commodore_lynx_directory_t* dir,
                                    int entry_idx, commodore_prg_t* out_prg) const {
    if (!data || !dir || !out_prg) return false;
    if (entry_idx < 0 || entry_idx >= (int)dir->file_count) return false;
    memset(out_prg, 0, sizeof(*out_prg));

    const commodore_lynx_entry_t* entry = &dir->entries[entry_idx];

    if (entry->data_length < 2) {
        printf("LNXFormat: File \"%s\" too small (%zu bytes)\n",
               entry->filename, entry->data_length);
        return false;
    }

    if (entry->data_offset + entry->data_length > data_size) {
        size_t avail = data_size - entry->data_offset;
        if (avail < 2) return false;
        printf("LNXFormat: File \"%s\" truncated (%zu of %zu bytes available)\n",
               entry->filename, avail, entry->data_length);
        return commodore_prg_parse(data + entry->data_offset, avail, out_prg);
    }

    return commodore_prg_parse(data + entry->data_offset,
                               entry->data_length, out_prg);
}

bool commodore_lynx_t::extract_all_prgs(commodore_prg_t* out_prgs, int max_prgs,
                                        int* out_count) const {
    if (!out_prgs || !out_count) return false;
    *out_count = 0;

    commodore_lynx_directory_t dir;
    if (!read_directory(&dir)) return false;

    for (unsigned i = 0; i < dir.file_count && *out_count < max_prgs; i++) {
        if (dir.entries[i].file_type != 'P') continue;
        if (dir.entries[i].data_length < 2) continue;

        commodore_prg_t prg = {};
        if (extract_file(&dir, (int)i, &prg)) {
            out_prgs[*out_count] = prg;
            (*out_count)++;
            printf("LNXFormat: Extracted PRG \"%s\": $%04X-$%04X (%zu bytes)\n",
                   dir.entries[i].filename, prg.load_addr, prg.end_addr, prg.data_size);
        }
    }

    printf("LNXFormat: Extracted %d PRG files\n", *out_count);
    return *out_count > 0;
}

// ============================================================================
// Format Identification
// ============================================================================

static float lnx_identify(const uint8_t* data, size_t file_size, const char* extension) {
    /* Scan for "LYNX" in the first kilobyte */
    if (data && file_size >= 100) {
        size_t scan = (file_size < 1024) ? file_size : 1024;
        for (size_t i = 0; i + 3 < scan; i++) {
            if (data[i] == 'L' && data[i+1] == 'Y' &&
                data[i+2] == 'N' && data[i+3] == 'X')
                return 0.9f;
        }
    }
    if (extension && format_ext_match(extension, ".lnx")) return 0.8f;
    return 0.0f;
}

// ============================================================================
// Load Callback
// ============================================================================

static bool lnx_load(const uint8_t* data, size_t size, format_load_result_t* out) {
    commodore_lynx_t lynx;
    if (lynx.open_mem(data, size)) {
        int count = 0;
        if (lynx.extract_all_prgs(out->files,
                                  FORMAT_LOAD_MAX_FILES, &count)) {
            out->type = FORMAT_LOAD_ARCHIVE;
            out->file_count = count;
            lynx.close();
            return true;
        }
        lynx.close();
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "LNX opened but no PRGs extracted");
    } else {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Failed to open LNX from memory");
    }
    out->type = FORMAT_LOAD_ERROR;
    return false;
}

// ============================================================================
// Format Descriptor
// ============================================================================

/// LNX file-type char → extension suffix for display.
static const char* lnx_type_extension(char file_type) {
    switch (file_type) {
        case 'P': case 'p': return ".prg";
        case 'S': case 's': return ".seq";
        case 'U': case 'u': return ".usr";
        case 'R': case 'r': return ".rel";
        case 'D': case 'd': return "";     // DEL — no extension suffix
        default:            return ".prg";
    }
}

/// List directory entries in a Lynx archive.  Returns entry count, or -1 on error.
static int lnx_list_entries(const uint8_t* data, size_t size,
                            format_container_entry_t* entries, int max_entries) {
    commodore_lynx_t lnx{};
    if (!lnx.open_mem(data, size)) return -1;

    commodore_lynx_directory_t dir{};
    if (!lnx.read_directory(&dir)) { lnx.close(); return -1; }

    int count = 0;
    for (unsigned i = 0; i < dir.file_count && count < max_entries; ++i) {
        const auto& le = dir.entries[i];

        // Lynx filenames are PETSCII — convert to ASCII
        char name_buf[17]{};
        memcpy(name_buf, le.filename, 16);
        for (int j = 0; j < 16 && name_buf[j]; ++j)
            name_buf[j] = petscii_to_ascii(static_cast<uint8_t>(name_buf[j]));

        auto& entry = entries[count];
        snprintf(entry.display_name, sizeof(entry.display_name),
                 "%s%s", name_buf, lnx_type_extension(le.file_type));
        entry.size  = le.data_length;
        entry.index = static_cast<int>(i);
        ++count;
    }

    lnx.close();
    return count;
}

/// Extract a Lynx entry by index.  Returns PRG data with 2-byte load address header.
static bool lnx_extract_entry(const uint8_t* data, size_t size,
                               int entry_index, uint8_t** out_data, size_t* out_size) {
    commodore_lynx_t lnx{};
    if (!lnx.open_mem(data, size)) return false;

    commodore_lynx_directory_t dir{};
    if (!lnx.read_directory(&dir)) { lnx.close(); return false; }

    commodore_prg_t prg{};
    bool ok = lnx.extract_file(&dir, entry_index, &prg);
    lnx.close();

    if (!ok) return false;

    // Build PRG-format buffer: 2-byte load address + data
    *out_size = 2 + prg.data_size;
    *out_data = static_cast<uint8_t*>(malloc(*out_size));
    if (!*out_data) { commodore_prg_free(&prg); return false; }

    (*out_data)[0] = static_cast<uint8_t>(prg.load_addr & 0xFF);
    (*out_data)[1] = static_cast<uint8_t>(prg.load_addr >> 8);
    memcpy(*out_data + 2, prg.data, prg.data_size);
    commodore_prg_free(&prg);
    return true;
}

static const char* lnx_extensions[] = { ".lnx", NULL };

const format_descriptor_t LNX_FORMAT_DESCRIPTOR = {
    "LNX",
    "Lynx Multi-File Archive",
    lnx_extensions,
    FORMAT_CAP_LOADABLE | FORMAT_CAP_CONTAINER,
    lnx_identify,
    lnx_load,
    lnx_list_entries,
    lnx_extract_entry
};

// ============================================================================
// Auto-Registration
// ============================================================================

REGISTER_FORMAT(LNX, &LNX_FORMAT_DESCRIPTOR)
