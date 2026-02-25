#pragma once
// =============================================================================
// C64 Screen Text Utilities — Write text to screen RAM as screen codes
// =============================================================================
//
// Provides low-level helpers for writing text, colour, and hex values directly
// to C64 screen RAM ($0400, 40×25) and colour RAM.  These operate on raw
// uint8_t* buffers and have no dependency on the C64 system struct, making
// them usable from any context (SID player info page, test harnesses, etc.).
//
// Three character conversion modes are provided:
//   - ASCII:   for string literals and host-generated text (c64_ascii_to_screencode)
//   - Latin-1: for raw metadata from file formats like SID (c64_latin1_to_screencode)
//   - PETSCII: for native C64 text data (c64_petscii_to_screencode)
// =============================================================================

#include <cstdint>

/**
 * Convert an ASCII character to a C64 screen code (uppercase/graphics mode).
 *
 *   A–Z → $01–$1A,  a–z → $01–$1A (same — uppercase mode),
 *   ' '–'?' → $20–$3F,  '@' → $00,  unmapped → $2E ('.')
 */
uint8_t c64_ascii_to_screencode(char c);

/**
 * Convert an ISO 8859-1 (Latin-1) byte to a C64 screen code.
 *
 * ASCII letters and punctuation are mapped normally.  Latin-1 extended
 * characters (0xC0–0xFF) are accent-stripped to their base letter so
 * e.g. ü → U, é → E.  This is the correct converter for SID file
 * metadata which the spec defines as ISO 8859-1.
 *
 * Special: £ (0xA3) maps to the C64's native £ sign (screen code $1C).
 */
uint8_t c64_latin1_to_screencode(uint8_t ch);

/**
 * Convert a PETSCII byte to a C64 screen code (uppercase/graphics mode).
 *
 * Handles the full 0x00–0xFF PETSCII range including graphic characters.
 * Use this for text that originates from the C64 itself, NOT for file
 * format metadata (which is typically Latin-1 or ASCII).
 */
uint8_t c64_petscii_to_screencode(uint8_t ch);

/**
 * Write a null-terminated ASCII string to screen RAM and colour RAM
 * at the given (row, col) position.  Clipped to column 39.
 */
void c64_write_screen_text(uint8_t* screen, uint8_t* color,
                           int row, int col,
                           const char* text, uint8_t color_val);

/**
 * Write a null-terminated raw byte string to screen RAM and colour RAM
 * using Latin-1-to-screencode conversion (accent stripping).
 * Use this for metadata strings from file formats (SID, etc.).
 * Clipped to column 39.
 */
void c64_write_screen_latin1(uint8_t* screen, uint8_t* color,
                              int row, int col,
                              const char* text, uint8_t color_val);

/**
 * Fill an entire 40-column screen row with a single screen code and colour.
 */
void c64_fill_screen_row(uint8_t* screen, uint8_t* color,
                         int row, uint8_t sc, uint8_t col_val);

/**
 * Write a 16-bit hex value as "$XXXX" to screen RAM and colour RAM.
 * Returns the number of characters written (always 5).
 */
int c64_write_hex16(uint8_t* screen, uint8_t* color,
                    int row, int col, uint16_t val, uint8_t col_val);
