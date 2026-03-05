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
 *   1. Call NesScreenUtils::init_font()  — uploads font tiles to CHR-RAM
 *   2. Call NesScreenUtils::set_palette() — sets text/background colours
 *   3. Use NesScreenUtils::write_text(), NesScreenUtils::fill_row(), etc.
 */

#include <cstdint>
#include <cstddef>

namespace nes_system {

class PPU;   // Forward declaration

/// All NES text-screen operations, collected as static methods.
/// No state — each method takes a PPU* as its first argument.
struct NesScreenUtils {
    // NES nametable dimensions (in tiles)
    static constexpr int COLS = 32;
    static constexpr int ROWS = 30;

    /// Upload the built-in 8×8 ASCII font into CHR-RAM pattern table 0.
    static void init_font(PPU* ppu);

    /// Set up a text-mode palette in PPU palette RAM.
    static void set_palette(PPU* ppu,
                            uint8_t bg_color     = 0x0F,
                            uint8_t fg_color     = 0x30,
                            uint8_t accent_color = 0x12,
                            uint8_t dim_color    = 0x10);

    /// Clear the entire nametable (fill with space tiles).
    static void clear(PPU* ppu);

    /// Write a null-terminated string to the nametable at (row, col).
    static void write_text(PPU* ppu, int row, int col, const char* text);

    /// Write a limited-length string to the nametable at (row, col).
    static void write_text_n(PPU* ppu, int row, int col,
                             const char* text, int max_len);

    /// Fill an entire nametable row with a single tile.
    static void fill_row(PPU* ppu, int row, uint8_t tile);

    /// Write a 16-bit value as a 4-digit hex string: "$XXXX"
    static void write_hex16(PPU* ppu, int row, int col, uint16_t value);

    /// Write a decimal number (0-255) at (row, col).
    static void write_dec(PPU* ppu, int row, int col, int value);

    /// Set the attribute table entry for a specific nametable region.
    static void set_attribute(PPU* ppu, int row, int col, uint8_t palette);

    /// Set attributes for an entire row of tiles to use a given palette.
    static void set_row_attribute(PPU* ppu, int row, uint8_t palette);

    /// Configure PPU registers for static text display.
    static void enable_display(PPU* ppu);
};

} // namespace nes_system
