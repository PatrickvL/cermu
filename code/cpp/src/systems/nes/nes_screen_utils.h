#pragma once
/**
 * NES Screen Text Utilities
 *
 * Provides text rendering to the NES PPU nametable, analogous to
 * c64_screen_utils for the Commodore 64.  Uses an embedded 8×8 ASCII
 * font that is loaded into CHR-RAM pattern table memory.
 *
 * The NES PPU renders 32 columns × 30 rows of 8×8 tiles.  Each tile
 * in the nametable is an index into the pattern table where the font
 * glyph bitmaps reside.
 *
 * Typical usage for NSF player info pages:
 *   1. Call nes_screen_init_font()  — uploads font tiles to CHR-RAM
 *   2. Call nes_screen_set_palette() — sets text/background colours
 *   3. Use nes_screen_write_text(), nes_screen_fill_row(), etc.
 */

#include <cstdint>
#include <cstddef>

namespace nes_system {

class PPU;        // Forward declaration
class MemoryBus;  // Forward declaration

// NES nametable dimensions (in tiles)
static constexpr int NES_COLS = 32;
static constexpr int NES_ROWS = 30;

/**
 * Upload the built-in 8×8 ASCII font into CHR-RAM pattern table 0.
 *
 * Tiles are stored at pattern table addresses matching their ASCII code,
 * so tile 0x41 = 'A', tile 0x20 = space, etc.  This allows direct use
 * of ASCII values as nametable tile indices.
 *
 * @param ppu  PPU instance (font goes into pattern table 0 via CHR-RAM)
 */
void nes_screen_init_font(PPU* ppu);

/**
 * Set up a text-mode palette in PPU palette RAM.
 *
 * Sets background colour and foreground colours for palette 0
 * (used by the nametable).  Defaults give a classic NES look.
 *
 * @param ppu  PPU instance
 * @param bg_color     Background NES palette index (default: 0x0F = black)
 * @param fg_color     Text NES palette index (default: 0x30 = white)
 * @param accent_color Accent NES palette index (default: 0x12 = dark blue header)
 * @param dim_color    Dimmed text NES palette index (default: 0x10 = light grey)
 */
void nes_screen_set_palette(PPU* ppu,
                            uint8_t bg_color    = 0x0F,
                            uint8_t fg_color    = 0x30,
                            uint8_t accent_color = 0x12,
                            uint8_t dim_color   = 0x10);

/**
 * Clear the entire nametable (fill with space tiles).
 * Also zeroes attribute table to use palette 0 everywhere.
 */
void nes_screen_clear(PPU* ppu);

/**
 * Write a null-terminated string to the nametable at (row, col).
 * Characters are mapped 1:1 as ASCII tile indices.
 * Out-of-bounds characters are silently clipped.
 */
void nes_screen_write_text(PPU* ppu, int row, int col, const char* text);

/**
 * Write a limited-length string to the nametable at (row, col).
 * Writes exactly min(max_len, strlen(text)) characters.
 */
void nes_screen_write_text_n(PPU* ppu, int row, int col,
                              const char* text, int max_len);

/**
 * Fill an entire nametable row with a single tile.
 */
void nes_screen_fill_row(PPU* ppu, int row, uint8_t tile);

/**
 * Write a 16-bit value as a 4-digit hex string: "$XXXX"
 */
void nes_screen_write_hex16(PPU* ppu, int row, int col, uint16_t value);

/**
 * Write a decimal number (0-255) at (row, col).
 * Writes up to 3 digits with no leading zeros.
 */
void nes_screen_write_dec(PPU* ppu, int row, int col, int value);

/**
 * Set the attribute table entry for a specific nametable region.
 *
 * The NES attribute table divides the screen into 16×15 groups of
 * 2×2 tiles, each selecting one of 4 palettes.  This function sets
 * the palette for the 2×2-tile block containing (row, col).
 *
 * @param ppu      PPU instance
 * @param row      Tile row (0-29)
 * @param col      Tile column (0-31)
 * @param palette  Palette index (0-3)
 */
void nes_screen_set_attribute(PPU* ppu, int row, int col, uint8_t palette);

/**
 * Set attributes for an entire row of tiles to use a given palette.
 */
void nes_screen_set_row_attribute(PPU* ppu, int row, uint8_t palette);

/**
 * Configure PPU registers for static text display (no scrolling,
 * background rendering enabled, pattern table 0 for background).
 */
void nes_screen_enable_display(PPU* ppu);

} // namespace nes_system
