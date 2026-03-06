/**
 * A26 Format Handler — Atari 2600 ROM image identification
 *
 * Content-based identification for Atari 2600 cartridge ROM dumps.
 * The load callback is intentionally omitted — the A2600 system
 * loads cartridge data directly through its own file loading path
 * and mapper factory.
 */

#include "a26_format.h"
#include "format_registry.h"
#include <cstring>

// ============================================================================
// A26 Identification
// ============================================================================

// Valid Atari 2600 cartridge sizes (bytes)
static bool is_valid_a2600_size(size_t size) {
    return size == 2048   ||   // 2K (no banking)
           size == 4096   ||   // 4K (no banking)
           size == 8192   ||   // 8K (F8, E0, FE)
           size == 12288  ||   // 12K (FA / CBS RAM Plus)
           size == 16384  ||   // 16K (F6)
           size == 32768  ||   // 32K (F4 or 3F)
           size == 65536  ||   // 64K (3F Tigervision)
           size == 131072 ||   // 128K (3F Tigervision)
           size == 262144 ||   // 256K (3F Tigervision)
           size == 524288;     // 512K (3F Tigervision)
}

static float a26_identify(const uint8_t* data, size_t file_size, const char* extension) {
    // .a26 extension is Atari 2600 specific
    if (extension && format_ext_match(extension, ".a26")) {
        return 0.95f;  // Very high confidence — .a26 is unambiguous
    }

    // .bin with valid A2600 cart size — possible but ambiguous
    if (extension && format_ext_match(extension, ".bin")) {
        if (is_valid_a2600_size(file_size)) {
            return 0.3f;  // Low confidence — .bin is shared with many systems
        }
    }

    // No content-based magic bytes — A2600 ROMs are headerless
    (void)data;
    return 0.0f;
}

// ============================================================================
// FORMAT DESCRIPTOR
// ============================================================================

static const char* a26_extensions[] = { ".a26", nullptr };

const format_descriptor_t A26_FORMAT_DESCRIPTOR = {
    "A26",                                       // name
    "Atari 2600 Cartridge ROM Image",            // description
    a26_extensions,                              // extensions
    FORMAT_CAP_METADATA,                         // capabilities (identify only)
    a26_identify,                                // identify
    nullptr,                                     // load — handled by A2600 system
    nullptr,                                     // list_entries
    nullptr,                                     // extract_entry
};

REGISTER_FORMAT(A26, &A26_FORMAT_DESCRIPTOR)
