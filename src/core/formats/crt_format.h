#pragma once

/**
 * CRT Format Handler — Cartridge Image
 *
 * Parses CRT cartridge image headers and CHIP packet metadata.
 * Supports both C64 ("C64 CARTRIDGE   ") and VIC-20 ("VIC20 CARTRIDGE ")
 * signatures.  Full chip-bank loading is system-specific and done by the
 * system layer; this module provides the shared header/CHIP parsing.
 */

#include "core/formats/format_handler.h"
#include <cstring>  // memcmp for commodore_crt_machine()
// ============================================================================
// CRT Types
// ============================================================================

/** CRT header (first 64 bytes) */
struct commodore_crt_header_t {
    char     signature[16];         /**< "C64 CARTRIDGE   " or "VIC20 CARTRIDGE " */
    uint32_t header_length;         /**< Header length (usually 0x40) */
    uint16_t version;               /**< CRT version (big-endian) */
    uint16_t hardware_type;         /**< Cartridge hardware type (big-endian) */
    uint8_t  exrom;                 /**< EXROM line state */
    uint8_t  game;                  /**< GAME line state */
    uint8_t  reserved[6];
    char     name[32];              /**< Cartridge name, null-terminated */
};

/** CRT CHIP packet header */
struct commodore_crt_chip_t {
    char     signature[4];          /**< "CHIP" */
    uint32_t packet_length;         /**< Total packet length (big-endian) */
    uint16_t chip_type;             /**< 0=ROM, 1=RAM, 2=Flash (big-endian) */
    uint16_t bank_number;           /**< Bank number (big-endian) */
    uint16_t load_address;          /**< Load address (big-endian) */
    uint16_t rom_size;              /**< ROM image size (big-endian) */
};

// ============================================================================
// CRT Machine Identification
// ============================================================================

/** Machine type inferred from the CRT signature. */
enum commodore_crt_machine_t {
    CRT_MACHINE_UNKNOWN = 0,
    CRT_MACHINE_C64     = 1,   /**< "C64 CARTRIDGE   " */
    CRT_MACHINE_VIC20   = 2,   /**< "VIC20 CARTRIDGE " */
};

/** Return the machine type for a CRT signature string (16 bytes). */
static inline commodore_crt_machine_t commodore_crt_machine(const char sig[16]) {
    if (memcmp(sig, "C64 CARTRIDGE   ", 16) == 0) return CRT_MACHINE_C64;
    if (memcmp(sig, "VIC20 CARTRIDGE ", 16) == 0) return CRT_MACHINE_VIC20;
    return CRT_MACHINE_UNKNOWN;
}

// ============================================================================
// CRT Format API
// ============================================================================

/**
 * Read CRT header from an in-memory buffer.
 * Accepts both C64 and VIC-20 signatures.
 */
bool commodore_crt_read_header_mem(const uint8_t* data, size_t data_size, commodore_crt_header_t* out_header);

// ============================================================================
// CHIP Packet Iterator
// ============================================================================

/**
 * Callback invoked for each CHIP packet found in the CRT image.
 *
 * @param chip       Parsed CHIP packet header
 * @param rom_data   Pointer to the ROM payload bytes (chip.rom_size bytes)
 * @param user_data  Opaque context passed through from the iterator
 * @return true to continue iterating, false to stop early
 */
typedef bool (*commodore_crt_chip_callback_t)(
    const commodore_crt_chip_t* chip,
    const uint8_t* rom_data,
    void* user_data);

/**
 * Iterate all CHIP packets in a CRT image buffer.
 *
 * Skips the CRT header (using header_length from the parsed header) and
 * calls @p callback for each valid CHIP packet found.
 *
 * @param data       Complete CRT file in memory
 * @param data_size  Size of the buffer
 * @param header     Previously parsed CRT header (for header_length)
 * @param callback   Function called for each CHIP packet
 * @param user_data  Opaque context passed to callback
 * @return Number of CHIP packets successfully processed, or -1 on error
 */
int commodore_crt_iterate_chips(
    const uint8_t* data, size_t data_size,
    const commodore_crt_header_t* header,
    commodore_crt_chip_callback_t callback,
    void* user_data);

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t CRT_FORMAT_DESCRIPTOR;

