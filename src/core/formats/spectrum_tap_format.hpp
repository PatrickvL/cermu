#pragma once

/**
 * Spectrum TAP Format Handler — ZX Spectrum Tape Files
 *
 * The .tap format stores blocks of tape-saved data.  Each block starts
 * with a 2-byte LE length field, followed by the raw tape data including
 * the flag byte and checksum.
 *
 * Note: This is the ZX Spectrum TAP format, distinct from the Commodore
 * TAP format (raw tape pulse data) in tap_format.hpp.
 *
 * Block structure:
 *   2 bytes: block length N (LE, not counting these 2 bytes)
 *   1 byte:  flag ($00 = header, $FF = data)
 *   N-2 bytes: payload
 *   1 byte:  checksum (XOR of flag + payload bytes)
 *
 * Header block (flag=$00, payload=17 bytes):
 *   1 byte:  type (0=Program, 1=Number array, 2=Char array, 3=Code)
 *   10 bytes: filename (space-padded)
 *   2 bytes: data length (LE)
 *   2 bytes: param1 (autostart line for Program, start address for Code)
 *   2 bytes: param2 (var offset for Program, unused for Code = 32768)
 *
 * A typical BASIC program on tape:
 *   Block 0: header (type=0, Program)
 *   Block 1: data (the BASIC program bytes)
 *   Block 2: header (type=3, Code) — optional screen$ loader
 *   Block 3: data (the code/screen bytes)
 */

#include "core/formats/format_handler.hpp"

// ============================================================================
// Spectrum TAP Block Types
// ============================================================================

enum spectrum_tap_type_t : uint8_t {
    SPECTRUM_TAP_PROGRAM      = 0,
    SPECTRUM_TAP_NUMBER_ARRAY = 1,
    SPECTRUM_TAP_CHAR_ARRAY   = 2,
    SPECTRUM_TAP_CODE         = 3,
};

// ============================================================================
// Spectrum TAP Header (17-byte payload in a header block)
// ============================================================================

struct spectrum_tap_header_t {
    uint8_t  type;               // 0=Program, 1=NumArr, 2=CharArr, 3=Code
    char     filename[11];       // 10 chars + null terminator
    uint16_t data_length;        // Length of the associated data block payload
    uint16_t param1;             // Autostart line (Program) or start address (Code)
    uint16_t param2;             // Variable offset (Program) or 32768 (Code)
};

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t SPECTRUM_TAP_FORMAT_DESCRIPTOR;
