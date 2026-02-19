#pragma once

/**
 * PETSCII Character Encoding Utilities
 *
 * PETSCII is the character encoding used by Commodore 8-bit computers.
 * This module provides encoding/decoding utilities that sit between raw
 * format data and display/processing code.
 *
 * Format handlers store raw byte data; this module interprets it.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Trim trailing PETSCII padding ($A0 shifted-space and regular spaces)
 * from a fixed-width string buffer.  Writes '\0' over padding bytes.
 *
 * @param str     Mutable string buffer
 * @param maxlen  Maximum length of the buffer (excluding any terminator the caller added)
 */
void petscii_trim_padding(char* str, int maxlen);

/**
 * Convert a single PETSCII byte to printable ASCII.
 * Non-printable codes become '.'.
 */
char petscii_to_ascii(uint8_t petscii);

#ifdef __cplusplus
}
#endif
