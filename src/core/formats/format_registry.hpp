#pragma once

/**
 * Format Registry — Central format-agnostic dispatch layer
 *
 * Maintains a list of all registered file format handlers and provides:
 *   - Format identification (by extension and/or content)
 *   - Unified file loading (dispatches through descriptor->load())
 *   - Extension queries for file dialogs
 *
 * The registry itself contains ZERO format-specific code.  All format
 * knowledge lives in the individual format handler implementations,
 * which self-register during static initialisation via REGISTER_FORMAT.
 *
 * Code that needs to load files should go through FormatRegistry rather
 * than calling format-specific APIs directly.
 */

#include "core/formats/format_handler.hpp"
/**
 * Load any supported file format (C API).
 * Identifies the format and dispatches to its load() callback.
 * @return true on success (type != FORMAT_LOAD_ERROR)
 */
bool format_load_file(const char* filepath, format_load_result_t* out);
// ============================================================================
// C++ Registry API
// ============================================================================

#include <vector>
#include <string>

/**
 * FormatRegistry — singleton that collects all file format descriptors.
 * Format handlers self-register during static initialisation.
 */
class FormatRegistry {
public:
    static FormatRegistry& instance();

    /** Register a format descriptor (called during static init). */
    void register_format(const format_descriptor_t* descriptor);

    /** Get all registered format descriptors. */
    const std::vector<const format_descriptor_t*>& get_formats() const;

    /** Find a format descriptor by file extension (e.g. ".d64"). */
    const format_descriptor_t* find_by_extension(const char* extension) const;

    /**
     * Identify format from content + extension.
     * Returns the descriptor with the highest identify() confidence, or nullptr.
     */
    const format_descriptor_t* identify(const uint8_t* data, size_t file_size,
                                        const char* extension) const;

    /** Get all supported file extensions (for file dialogs). */
    std::vector<std::string> get_all_extensions() const;

    /** Build a file-dialog filter string (e.g. ".prg,.d64,.t64,..."). */
    std::string get_file_dialog_filter() const;

    /**
     * Unified load — identifies format, dispatches to descriptor->load().
     */
    bool load_file(const char* filepath, format_load_result_t* out) const;

private:
    FormatRegistry() = default;
    std::vector<const format_descriptor_t*> formats_;
};

/**
 * Auto-registration macro.
 * Place at the end of each format's .cpp file.
 */
#define REGISTER_FORMAT(tag, descriptor_ptr) \
    namespace { \
        struct FormatRegistrar_##tag { \
            FormatRegistrar_##tag() { \
                FormatRegistry::instance().register_format(descriptor_ptr); \
            } \
        }; \
        static FormatRegistrar_##tag s_format_registrar_##tag; \
    }

