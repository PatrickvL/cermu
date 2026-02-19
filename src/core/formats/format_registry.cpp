/**
 * Format Registry — Implementation
 *
 * The registry dispatches entirely through format_descriptor_t callbacks.
 * It contains ZERO format-specific logic.
 */

#include "format_registry.h"
#include <cstring>
#include <algorithm>
#include <stdio.h>

// ============================================================================
// FormatRegistry singleton
// ============================================================================

FormatRegistry& FormatRegistry::instance() {
    static FormatRegistry registry;
    return registry;
}

void FormatRegistry::register_format(const format_descriptor_t* descriptor) {
    if (!descriptor) return;
    printf("FormatRegistry: Registered format: %s (%s)\n",
           descriptor->name, descriptor->description);
    formats_.push_back(descriptor);
}

const std::vector<const format_descriptor_t*>& FormatRegistry::get_formats() const {
    return formats_;
}

const format_descriptor_t* FormatRegistry::find_by_extension(const char* extension) const {
    if (!extension) return nullptr;
    for (auto* fmt : formats_) {
        if (!fmt->extensions) continue;
        for (const char** ext = fmt->extensions; *ext; ext++) {
            if (format_ext_match(extension, *ext))
                return fmt;
        }
    }
    return nullptr;
}

const format_descriptor_t* FormatRegistry::identify(const uint8_t* data, size_t file_size,
                                                     const char* extension) const {
    float best = 0.0f;
    const format_descriptor_t* best_fmt = nullptr;

    for (auto* fmt : formats_) {
        if (!fmt->identify) continue;
        float conf = fmt->identify(data, file_size, extension);
        if (conf > best) {
            best = conf;
            best_fmt = fmt;
        }
    }
    return (best > 0.0f) ? best_fmt : nullptr;
}

std::vector<std::string> FormatRegistry::get_all_extensions() const {
    std::vector<std::string> result;
    for (auto* fmt : formats_) {
        if (!fmt->extensions) continue;
        for (const char** ext = fmt->extensions; *ext; ext++)
            result.push_back(*ext);
    }
    return result;
}

std::string FormatRegistry::get_file_dialog_filter() const {
    std::string filter;
    for (auto* fmt : formats_) {
        if (!fmt->extensions) continue;
        for (const char** ext = fmt->extensions; *ext; ext++) {
            if (!filter.empty()) filter += ',';
            filter += *ext;
        }
    }
    return filter;
}

// ============================================================================
// Unified Load — pure dispatch through descriptor->load()
// ============================================================================

bool FormatRegistry::load_file(const char* filepath, format_load_result_t* out) const {
    if (!filepath || !out) return false;
    memset(out, 0, sizeof(*out));
    out->type = FORMAT_LOAD_ERROR;

    /* Determine the file extension */
    const char* ext = strrchr(filepath, '.');

    /* Identify format — try content-based first if we can read the file */
    const format_descriptor_t* fmt = nullptr;
    {
        size_t peek_size = 0;
        uint8_t* peek = format_read_entire_file(filepath, &peek_size);
        if (peek) {
            fmt = identify(peek, peek_size, ext);
            free(peek);
        } else if (ext) {
            fmt = find_by_extension(ext);
        }
    }

    if (!fmt) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Unrecognised file format: %s", filepath);
        return false;
    }

    if (!fmt->load) {
        snprintf(out->error_msg, sizeof(out->error_msg),
                 "Format '%s' does not support direct loading", fmt->name);
        return false;
    }

    bool ok = fmt->load(filepath, out);
    out->format = fmt;
    return ok;
}

// ============================================================================
// C API wrapper
// ============================================================================

bool format_load_file(const char* filepath, format_load_result_t* out) {
    return FormatRegistry::instance().load_file(filepath, out);
}
