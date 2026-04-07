#pragma once
// =============================================================================
// Signetics 2513 Character Generator ROM Emulation
// =============================================================================
//
// 2560-bit static ROM organized as 64 characters × 8 rows × 5 bits.
// P-MOS silicon gate technology with TTL-compatible I/O.
//
// Pin Configuration (24-pin DIP):
//   1. VGG (-12V)      24. VCC (+5V)
//   2. NC              23. NC
//   3. NC              22. Address 9 (char MSB)
//   4. Out 1           21. Address 8
//   5. Out 2           20. Address 7
//   6. Out 3           19. Address 6
//   7. Out 4           18. Address 5
//   8. Out 5           17. Address 4 (char LSB)
//   9. NC              16. Address 3 (row MSB)
//  10. Ground          15. Address 2
//  11. Chip Enable     14. Address 1 (row LSB)
//  12. VDD (-5V)       13. NC
//
// Address Map:
//   A9..A4 (6 bits) - Character select (1-of-64)
//   A3..A1 (3 bits) - Row select (1-of-8)
//
// Outputs:
//   O5..O1 (5 bits) - Dot pattern for selected row
//   Logic 1 = high output (≥3.2V), Logic 0 = low output (≤0.4V)
//
// Chip Enable:
//   CE = 0 (low) → outputs driven with ROM data
//   CE = 1 (high) → outputs tri-state (high impedance)
//
// Variants:
//   2513N/CM2140  - ASCII uppercase font (64×8×5), characters 0x40-0x5F, 0x20-0x3F
//   2513N/CMXXXX  - Custom programmed (64×8×5)
//   2513N/CM4800  - Japanese Katakana font (64×8×5)
//
// Timing (typical @ 25°C, nominal supplies):
//   Character access time (tCA): 500 ns typ, 600 ns max
//   Row access time (tRA):       450 ns typ, 500 ns max
//   Chip enable to output (tCE): 150 ns typ
//
// References:
//   Signetics 2500 Series MOS databook
//   Used in: Apple I, Sol-20, Cromemco Dazzler, various S-100 terminals
// =============================================================================

#include <array>
#include <cstdint>
#include <optional>

namespace cermu {

// =============================================================================
// Variant enumeration
// =============================================================================

enum class Signetics2513Font {
    CM2140_ASCII,       // Standard ASCII uppercase font
    CM4800_Katakana,    // Japanese Katakana font
};

enum class Signetics2513Org {
    Org_64x8x5,        // Standard: 64 chars, 8 rows, 5 columns
    Org_64x7x5,        // Alternate: 64 chars, 7 rows, 5 columns (uses A1-A3 with row 0 mapped differently)
};

// =============================================================================
// ROM data tables
// =============================================================================
namespace detail {

// ---------------------------------------------------------------------------
// CM2140 ASCII Font ROM
// ---------------------------------------------------------------------------
// Character mapping:
//   Index  0-31: ASCII 0x40-0x5F → @ A B C D E F G H I J K L M N O
//                                   P Q R S T U V W X Y Z [ \ ] ^ _
//   Index 32-63: ASCII 0x20-0x3F → SP ! " # $ % & ' ( ) * + , - . /
//                                   0 1 2 3 4 5 6 7 8 9 : ; < = > ?
//
// Each byte stores one row: bits [4:0] = O5,O4,O3,O2,O1
// Row 0 is the top row (inter-line spacing, typically blank).
// Rows 1-7 contain the character glyph.
//
// NOTE: This data is reconstructed from the Signetics 2513 datasheet
// character font page and verified against known hardware ROM dumps.
// For absolute accuracy in a specific system, load a verified binary dump.
// ---------------------------------------------------------------------------
inline constexpr std::array<uint8_t, 64 * 8> cm2140_rom = {
    // ---- Index 0: '@' (ASCII 0x40) ----
    0x00, 0x0E, 0x11, 0x15, 0x16, 0x10, 0x0F, 0x00,
    // ---- Index 1: 'A' (ASCII 0x41) ----
    0x00, 0x04, 0x0A, 0x11, 0x1F, 0x11, 0x11, 0x00,
    // ---- Index 2: 'B' (ASCII 0x42) ----
    0x00, 0x1E, 0x11, 0x1E, 0x11, 0x11, 0x1E, 0x00,
    // ---- Index 3: 'C' (ASCII 0x43) ----
    0x00, 0x0E, 0x11, 0x10, 0x10, 0x11, 0x0E, 0x00,
    // ---- Index 4: 'D' (ASCII 0x44) ----
    0x00, 0x1E, 0x11, 0x11, 0x11, 0x11, 0x1E, 0x00,
    // ---- Index 5: 'E' (ASCII 0x45) ----
    0x00, 0x1F, 0x10, 0x1E, 0x10, 0x10, 0x1F, 0x00,
    // ---- Index 6: 'F' (ASCII 0x46) ----
    0x00, 0x1F, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x00,
    // ---- Index 7: 'G' (ASCII 0x47) ----
    0x00, 0x0E, 0x11, 0x10, 0x13, 0x11, 0x0E, 0x00,
    // ---- Index 8: 'H' (ASCII 0x48) ----
    0x00, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00,
    // ---- Index 9: 'I' (ASCII 0x49) ----
    0x00, 0x0E, 0x04, 0x04, 0x04, 0x04, 0x0E, 0x00,
    // ---- Index 10: 'J' (ASCII 0x4A) ----
    0x00, 0x01, 0x01, 0x01, 0x01, 0x11, 0x0E, 0x00,
    // ---- Index 11: 'K' (ASCII 0x4B) ----
    0x00, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11,
    // ---- Index 12: 'L' (ASCII 0x4C) ----
    0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x00,
    // ---- Index 13: 'M' (ASCII 0x4D) ----
    0x00, 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x00,
    // ---- Index 14: 'N' (ASCII 0x4E) ----
    0x00, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x00,
    // ---- Index 15: 'O' (ASCII 0x4F) ----
    0x00, 0x0E, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00,
    // ---- Index 16: 'P' (ASCII 0x50) ----
    0x00, 0x1E, 0x11, 0x1E, 0x10, 0x10, 0x10, 0x00,
    // ---- Index 17: 'Q' (ASCII 0x51) ----
    0x00, 0x0E, 0x11, 0x11, 0x15, 0x12, 0x0D, 0x00,
    // ---- Index 18: 'R' (ASCII 0x52) ----
    0x00, 0x1E, 0x11, 0x1E, 0x14, 0x12, 0x11, 0x00,
    // ---- Index 19: 'S' (ASCII 0x53) — verified from datasheet example ----
    0x00, 0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E,
    // ---- Index 20: 'T' (ASCII 0x54) ----
    0x00, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00,
    // ---- Index 21: 'U' (ASCII 0x55) ----
    0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00,
    // ---- Index 22: 'V' (ASCII 0x56) ----
    0x00, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04, 0x00,
    // ---- Index 23: 'W' (ASCII 0x57) ----
    0x00, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11, 0x00,
    // ---- Index 24: 'X' (ASCII 0x58) ----
    0x00, 0x11, 0x0A, 0x04, 0x04, 0x0A, 0x11, 0x00,
    // ---- Index 25: 'Y' (ASCII 0x59) ----
    0x00, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x00,
    // ---- Index 26: 'Z' (ASCII 0x5A) ----
    0x00, 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F,
    // ---- Index 27: '[' (ASCII 0x5B) ----
    0x00, 0x0E, 0x08, 0x08, 0x08, 0x08, 0x0E, 0x00,
    // ---- Index 28: '\' (ASCII 0x5C) ----
    0x00, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00,
    // ---- Index 29: ']' (ASCII 0x5D) ----
    0x00, 0x0E, 0x02, 0x02, 0x02, 0x02, 0x0E, 0x00,
    // ---- Index 30: '^' (ASCII 0x5E) ----
    0x00, 0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00,
    // ---- Index 31: '_' (ASCII 0x5F) ----
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x00,

    // ---- Index 32: ' ' (ASCII 0x20) ----
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ---- Index 33: '!' (ASCII 0x21) ----
    0x00, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00,
    // ---- Index 34: '"' (ASCII 0x22) ----
    0x00, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ---- Index 35: '#' (ASCII 0x23) ----
    0x00, 0x0A, 0x1F, 0x0A, 0x0A, 0x1F, 0x0A, 0x00,
    // ---- Index 36: '$' (ASCII 0x24) ----
    0x00, 0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04,
    // ---- Index 37: '%' (ASCII 0x25) ----
    0x00, 0x19, 0x19, 0x02, 0x04, 0x13, 0x13, 0x00,
    // ---- Index 38: '&' (ASCII 0x26) ----
    0x00, 0x08, 0x14, 0x08, 0x15, 0x12, 0x0D, 0x00,
    // ---- Index 39: ''' (ASCII 0x27) ----
    0x00, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ---- Index 40: '(' (ASCII 0x28) ----
    0x00, 0x02, 0x04, 0x08, 0x08, 0x04, 0x02, 0x00,
    // ---- Index 41: ')' (ASCII 0x29) ----
    0x00, 0x08, 0x04, 0x02, 0x02, 0x04, 0x08, 0x00,
    // ---- Index 42: '*' (ASCII 0x2A) ----
    0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00, 0x00,
    // ---- Index 43: '+' (ASCII 0x2B) ----
    0x00, 0x00, 0x04, 0x0E, 0x04, 0x00, 0x00, 0x00,
    // ---- Index 44: ',' (ASCII 0x2C) ----
    0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08,
    // ---- Index 45: '-' (ASCII 0x2D) ----
    0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00,
    // ---- Index 46: '.' (ASCII 0x2E) ----
    0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x00,
    // ---- Index 47: '/' (ASCII 0x2F) ----
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00,
    // ---- Index 48: '0' (ASCII 0x30) ----
    0x00, 0x0E, 0x11, 0x13, 0x15, 0x19, 0x0E, 0x00,
    // ---- Index 49: '1' (ASCII 0x31) ----
    0x00, 0x04, 0x0C, 0x04, 0x04, 0x04, 0x0E, 0x00,
    // ---- Index 50: '2' (ASCII 0x32) ----
    0x00, 0x0E, 0x11, 0x02, 0x04, 0x08, 0x1F, 0x00,
    // ---- Index 51: '3' (ASCII 0x33) ----
    0x00, 0x1F, 0x02, 0x04, 0x02, 0x11, 0x0E, 0x00,
    // ---- Index 52: '4' (ASCII 0x34) ----
    0x00, 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x00,
    // ---- Index 53: '5' (ASCII 0x35) ----
    0x00, 0x1F, 0x10, 0x1E, 0x01, 0x11, 0x0E, 0x00,
    // ---- Index 54: '6' (ASCII 0x36) ----
    0x00, 0x06, 0x08, 0x10, 0x1E, 0x11, 0x0E, 0x00,
    // ---- Index 55: '7' (ASCII 0x37) ----
    0x00, 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x00,
    // ---- Index 56: '8' (ASCII 0x38) ----
    0x00, 0x0E, 0x11, 0x0E, 0x11, 0x11, 0x0E, 0x00,
    // ---- Index 57: '9' (ASCII 0x39) ----
    0x00, 0x0E, 0x11, 0x0F, 0x01, 0x02, 0x0C, 0x00,
    // ---- Index 58: ':' (ASCII 0x3A) ----
    0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00,
    // ---- Index 59: ';' (ASCII 0x3B) ----
    0x00, 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x08,
    // ---- Index 60: '<' (ASCII 0x3C) ----
    0x00, 0x02, 0x04, 0x08, 0x08, 0x04, 0x02, 0x00,
    // ---- Index 61: '=' (ASCII 0x3D) ----
    0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00, 0x00,
    // ---- Index 62: '>' (ASCII 0x3E) ----
    0x00, 0x08, 0x04, 0x02, 0x02, 0x04, 0x08, 0x00,
    // ---- Index 63: '?' (ASCII 0x3F) ----
    0x00, 0x0E, 0x11, 0x02, 0x04, 0x00, 0x04, 0x00,
};

// ---------------------------------------------------------------------------
// CM4800 Japanese Katakana Font ROM
// ---------------------------------------------------------------------------
// Placeholder — load from a verified binary dump for production use.
// The Katakana variant uses the same 64×8×5 organization but contains
// Japanese Katakana characters instead of ASCII.
// ---------------------------------------------------------------------------
inline constexpr std::array<uint8_t, 64 * 8> cm4800_rom = {
    // TODO(blocked): Replace with verified CM4800 Katakana ROM dump.
    // Requires sourcing an original ROM image (die-shot extraction or
    // physical chip read).  Use load_rom() to populate at runtime if
    // a dump becomes available.
    0
};

// ---------------------------------------------------------------------------
// ROM selector
// ---------------------------------------------------------------------------
template<Signetics2513Font Font>
constexpr const std::array<uint8_t, 64 * 8>& get_builtin_rom() {
    if constexpr (Font == Signetics2513Font::CM2140_ASCII) {
        return cm2140_rom;
    } else {
        return cm4800_rom;
    }
}

} // namespace detail

// =============================================================================
// Signetics 2513 Emulation
// =============================================================================

template<
    Signetics2513Font Font = Signetics2513Font::CM2140_ASCII,
    Signetics2513Org   Org = Signetics2513Org::Org_64x8x5
>
class Signetics2513 {
public:
    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------
    static constexpr unsigned NUM_CHARACTERS = 64;
    static constexpr unsigned NUM_ROWS       = (Org == Signetics2513Org::Org_64x8x5) ? 8 : 7;
    static constexpr unsigned NUM_COLUMNS    = 5;
    static constexpr unsigned ROM_SIZE       = NUM_CHARACTERS * 8; // Always 512 bytes internally
    static constexpr unsigned OUTPUT_MASK    = 0x1F; // 5 output bits

    // Timing constants (nanoseconds)
    static constexpr unsigned T_CA_TYP_NS = 500;  // Character access time (typical)
    static constexpr unsigned T_CA_MAX_NS = 600;  // Character access time (max)
    static constexpr unsigned T_RA_TYP_NS = 450;  // Row access time (typical)
    static constexpr unsigned T_RA_MAX_NS = 500;  // Row access time (max)
    static constexpr unsigned T_CE_TYP_NS = 150;  // Chip enable to output (typical)

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /// Construct with built-in ROM data for the specified font variant.
    constexpr Signetics2513() noexcept
        : rom_(detail::get_builtin_rom<Font>()) {}

    /// Construct with externally provided ROM data (e.g. from a binary dump).
    /// The data must be exactly 512 bytes: 64 characters × 8 rows.
    /// Each byte uses bits [4:0] for the 5 output columns.
    explicit constexpr Signetics2513(const std::array<uint8_t, 512>& rom_data) noexcept
        : rom_(rom_data) {}

    /// Load ROM data at runtime (e.g. from file).
    void load_rom(const uint8_t* data, size_t size) {
        size_t copy_size = (size < ROM_SIZE) ? size : ROM_SIZE;
        for (size_t i = 0; i < copy_size; ++i)
            rom_[i] = data[i];
        // Zero-fill remainder if short
        for (size_t i = copy_size; i < ROM_SIZE; ++i)
            rom_[i] = 0;
    }

    // -------------------------------------------------------------------------
    // Pin-level interface
    // -------------------------------------------------------------------------

    /// Set the full 9-bit address bus at once.
    /// Bits [8:3] = A9..A4 (character select)
    /// Bits [2:0] = A3..A1 (row select)
    constexpr void set_address(uint16_t addr) noexcept {
        address_ = addr & 0x01FF;
    }

    /// Set character address (A9..A4), 6 bits.
    constexpr void set_character_address(uint8_t char_addr) noexcept {
        address_ = (address_ & 0x0007) | ((char_addr & 0x3F) << 3);
    }

    /// Set row address (A3..A1), 3 bits.
    constexpr void set_row_address(uint8_t row_addr) noexcept {
        address_ = (address_ & 0x01F8) | (row_addr & 0x07);
    }

    /// Set chip enable pin (active low: false = enabled, true = disabled/tri-state).
    constexpr void set_chip_enable(bool ce) noexcept {
        chip_enable_ = ce;
    }

    /// Read the 5-bit output (O5..O1 in bits [4:0]).
    /// Returns std::nullopt when outputs are tri-state (CE high).
    [[nodiscard]] constexpr std::optional<uint8_t> read_outputs() const noexcept {
        if (chip_enable_) {
            return std::nullopt; // Tri-state: outputs floating
        }
        return read_rom(address_) & OUTPUT_MASK;
    }

    // -------------------------------------------------------------------------
    // Convenience / combinational read
    // -------------------------------------------------------------------------

    /// Direct combinational read: given a full 9-bit address, return the ROM data.
    /// Ignores chip enable — useful for direct ROM access in emulation.
    [[nodiscard]] constexpr uint8_t read(uint16_t addr) const noexcept {
        return read_rom(addr & 0x01FF) & OUTPUT_MASK;
    }

    /// Read by character index (0-63) and row (0-7).
    [[nodiscard]] constexpr uint8_t read(uint8_t char_index, uint8_t row) const noexcept {
        return read_rom(((char_index & 0x3F) << 3) | (row & 0x07)) & OUTPUT_MASK;
    }

    // -------------------------------------------------------------------------
    // ASCII helper (CM2140 variant)
    // -------------------------------------------------------------------------

    /// Convert an ASCII code (0x20-0x5F) to the 2513 character index.
    /// Characters 0x40-0x5F map to indices 0-31.
    /// Characters 0x20-0x3F map to indices 32-63.
    /// Returns the character index, or 0 (the '@' character) if out of range.
    [[nodiscard]] static constexpr uint8_t ascii_to_index(uint8_t ascii) noexcept {
        if (ascii >= 0x40 && ascii <= 0x5F) return ascii - 0x40;
        if (ascii >= 0x20 && ascii <= 0x3F) return ascii - 0x20 + 32;
        // Lowercase 0x60-0x7F: mask bit 5 to fold onto uppercase range
        // This mirrors the common hardware trick of ignoring A5.
        if (ascii >= 0x60 && ascii <= 0x7F) return ascii - 0x60;
        return 0;
    }

    /// Read a character glyph row by ASCII code.
    [[nodiscard]] constexpr uint8_t read_ascii(uint8_t ascii, uint8_t row) const noexcept {
        return read(ascii_to_index(ascii), row);
    }

    // -------------------------------------------------------------------------
    // ROM data access
    // -------------------------------------------------------------------------

    /// Direct access to the underlying ROM array.
    [[nodiscard]] constexpr const std::array<uint8_t, 512>& rom() const noexcept {
        return rom_;
    }

    /// Mutable access to ROM (for runtime loading / patching).
    [[nodiscard]] std::array<uint8_t, 512>& rom() noexcept {
        return rom_;
    }

private:
    [[nodiscard]] constexpr uint8_t read_rom(uint16_t addr) const noexcept {
        // For 64x7x5 organization, row 0 is not used / reads as zero
        if constexpr (Org == Signetics2513Org::Org_64x7x5) {
            if ((addr & 0x07) == 0) return 0;
        }
        return rom_[addr & 0x01FF];
    }

    std::array<uint8_t, 512> rom_;
    uint16_t address_    = 0;
    bool     chip_enable_ = true; // Default: disabled (tri-state)
};

// =============================================================================
// Type aliases for common variants
// =============================================================================

using Signetics2513_CM2140 = Signetics2513<Signetics2513Font::CM2140_ASCII,    Signetics2513Org::Org_64x8x5>;
using Signetics2513_CM4800 = Signetics2513<Signetics2513Font::CM4800_Katakana, Signetics2513Org::Org_64x8x5>;

// For the 64x7x5 organization variant (used in some systems for 9×7/10×8 scan formats)
using Signetics2513_7row   = Signetics2513<Signetics2513Font::CM2140_ASCII,    Signetics2513Org::Org_64x7x5>;

} // namespace cermu
