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
// Two character conversion modes are provided:
//   - ASCII:   for string literals and host-generated text (c64_ascii_to_screencode)
//   - PETSCII: for raw metadata bytes from file formats   (c64_petscii_to_screencode)
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
 * Convert a PETSCII byte to a C64 screen code (uppercase/graphics mode).
 *
 * Handles the full 0x00–0xFF range including PETSCII graphic characters
 * in the 0xA0–0xFF range.  Suitable for raw metadata bytes from SID files
 * which may be encoded as Latin-1 or PETSCII.
 *
 *   $00–$1F → dot (control codes)
 *   $20–$3F → direct
 *   $40–$5F → $00–$1F  (@, A–Z, [, £, ], ↑, ←)
 *   $60–$7F → $40–$5F  (graphic characters)
 *   $80–$9F → dot (control codes)
 *   $A0–$BF → $60–$7F  (reversed/shifted graphics)
 *   $C0–$DF → $00–$1F  (duplicate of $40–$5F)
 *   $E0–$FE → $60–$7E  (duplicate of $A0–$BE)
 *   $FF     → $5E  (π)
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
 * using PETSCII-to-screencode conversion.  Use this for metadata strings
 * that come directly from file formats (SID, etc.) rather than ASCII literals.
 * Clipped to column 39.
 */
void c64_write_screen_petscii(uint8_t* screen, uint8_t* color,
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
