#pragma once

/**
 * CRT Format Handler — Cartridge Image
 *
 * Parses CRT cartridge image headers and CHIP packet metadata.
 * The CRT format itself is C64-specific, but the parsing code is generic.
 * Full chip-bank loading is system-specific and done by the system layer.
 */

#include "format_handler.h"
// ============================================================================
// CRT Types
// ============================================================================

/** CRT header (first 64 bytes) */
struct commodore_crt_header_t {
    char     signature[16];         /**< "C64 CARTRIDGE   " */
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
// CRT Format API
// ============================================================================

/**
 * Read CRT file header for identification and metadata.
 */
/**
 * Read CRT header from an in-memory buffer.
 */
bool commodore_crt_read_header_mem(const uint8_t* data, size_t data_size, commodore_crt_header_t* out_header);

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t CRT_FORMAT_DESCRIPTOR;

