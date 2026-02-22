/**
 * Format Handler — Shared utility implementations
 */

#include "format_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// Program Data
// ============================================================================

void program_data_s::release() {
    if (data) {
        free(data);
        data = NULL;
        data_size = 0;
    }
}

// ============================================================================
// Load Result
// ============================================================================

const char* format_load_type_name(format_load_type_t type) {
    switch (type) {
        case FORMAT_LOAD_PROGRAM:  return "PROGRAM";
        case FORMAT_LOAD_ARCHIVE:  return "ARCHIVE";
        case FORMAT_LOAD_METADATA: return "METADATA";
        case FORMAT_LOAD_RAW:      return "RAW";
        case FORMAT_LOAD_ERROR:    return "ERROR";
        default:                   return "UNKNOWN";
    }
}

void format_load_result_s::release() {
    program.release();
    for (int i = 0; i < file_count; i++)
        files[i].release();
    memset(this, 0, sizeof(*this));
}

// ============================================================================
// File I/O
// ============================================================================

uint8_t* format_read_entire_file(const char* filepath, size_t* out_size) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) { fclose(f); return NULL; }

    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

bool format_ext_match(const char* ext, const char* target) {
    if (!ext || !target) return false;
    while (*ext && *target) {
        if (tolower((unsigned char)*ext) != tolower((unsigned char)*target)) return false;
        ext++; target++;
    }
    return *ext == *target;
}

bool format_in_list(const format_descriptor_t* fmt,
                    const format_descriptor_t* const* list) {
    if (!fmt || !list) return false;
    for (; *list; list++) {
        if (*list == fmt) return true;
    }
    return false;
}

// ============================================================================
// C++ Helpers — format descriptor list utilities
// ============================================================================

std::vector<std::string> format_list_extensions(
    const format_descriptor_t* const* formats) {
    std::vector<std::string> out;
    if (!formats) return out;
    for (; *formats; formats++) {
        const format_descriptor_t* f = *formats;
        if (!f->extensions) continue;
        for (const char** ext = f->extensions; *ext; ext++)
            out.push_back(*ext);
    }
    return out;
}

std::string format_list_dialog_filter(
    const format_descriptor_t* const* formats,
    const char* label) {
    if (!formats || !label) return ".*";
    // Build collection filter: "C64 Files{.prg,.d64,.t64,...}"
    std::string filter = std::string(label) + " Files{";
    bool first = true;
    for (const format_descriptor_t* const* p = formats; *p; p++) {
        const format_descriptor_t* f = *p;
        if (!f->extensions) continue;
        for (const char** ext = f->extensions; *ext; ext++) {
            if (!first) filter += ',';
            const char* e = *ext;
            if (e[0] == '.') e++;
            filter += '.';
            filter += e;
            first = false;
        }
    }
    filter += "},.*";
    return filter;
}
