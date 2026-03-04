/**
 * iNES Format — NES ROM image identification
 *
 * Content-based identification for iNES / NES 2.0 ROM images.
 * The load callback is intentionally omitted — the NES system
 * parses cartridge data directly through its Cartridge class.
 */

#include "ines_format.h"
#include "format_registry.h"
#include <cstring>

// ============================================================================
// iNES Identification
// ============================================================================

static float ines_identify(const uint8_t* data, size_t file_size, const char* extension) {
    // Content check: iNES header magic "NES\x1A"
    if (data && file_size >= 16 &&
        data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A) {
        return 1.0f;  // Unambiguous magic
    }

    // Extension-only fallback
    if (extension && format_ext_match(extension, ".nes")) {
        return 0.8f;
    }

    return 0.0f;
}

// ============================================================================
// FORMAT DESCRIPTOR
// ============================================================================

static const char* ines_extensions[] = { ".nes", nullptr };

const format_descriptor_t INES_FORMAT_DESCRIPTOR = {
    "iNES",                              // name
    "NES ROM Image (iNES/NES 2.0)",      // description
    ines_extensions,                     // extensions
    FORMAT_CAP_METADATA,                 // capabilities (identify only)
    ines_identify,                       // identify
    nullptr,                             // load — handled by NES Cartridge class
    nullptr,                             // list_entries
    nullptr,                             // extract_entry
};

REGISTER_FORMAT(INES, &INES_FORMAT_DESCRIPTOR)
