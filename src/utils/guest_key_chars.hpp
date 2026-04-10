#pragma once
// guest_key_chars.hpp — Unicode codepoints for guest keyboard characters
//
// All guest keyboard keys are identified by char32_t values:
//   - Standard characters use real Unicode codepoints (e.g., U+00A3 for £)
//   - Non-character keys (modifiers, function keys, actions) use the
//     Unicode Private Use Area (U+E000–U+F8FF)
//
// Matrix tables use these values exclusively — no SDL dependency.
// The host-to-guest key mapping layer translates SDL events to these
// codepoints; the matrix tables define guest-side truth only.
//
// Named constants are provided for characters that would otherwise
// require hex notation.  Standard ASCII characters ('A', '@', '+', etc.)
// can be used as char32_t literals directly.

#include <cstdint>

// ============================================================================
// Unicode characters appearing on guest keyboards
// ============================================================================
// Named constants for non-ASCII characters.  Standard ASCII characters
// don't need constants — use char32_t literals ('A', ':', '+', etc.).

inline constexpr char32_t UKEY_LEFT_ARROW     = U'\u2190';  // ← (Commodore ← key)
inline constexpr char32_t UKEY_UP_ARROW       = U'\u2191';  // ↑ (Commodore ↑ key)
inline constexpr char32_t UKEY_POUND_SIGN     = U'\u00A3';  // £ (Commodore £ key)
inline constexpr char32_t UKEY_PI             = U'\u03C0';  // π (Shifted ↑ on C64/VIC-20)
inline constexpr char32_t UKEY_ESCAPE         = U'\u001B';  // ESC (C16/Plus4)

// ============================================================================
// Private Use Area: non-character guest keys  (U+E000–U+F8FF)
// ============================================================================
// Keys that do not produce printable output.  Grouped by system family.
// Values are arbitrary within U+E000–U+F8FF.

// Marker
inline constexpr char32_t UKEY_NONE           = 0;          // No key / empty position

// --- Commodore family (U+E000–U+E03F) ---

// Modifiers
inline constexpr char32_t UKEY_CBM_SHIFT_L    = U'\uE000';  // Left Shift
inline constexpr char32_t UKEY_CBM_SHIFT_R    = U'\uE001';  // Right Shift
inline constexpr char32_t UKEY_CBM_CTRL       = U'\uE002';  // Control
inline constexpr char32_t UKEY_CBM_COMMODORE  = U'\uE003';  // C= (Commodore key)

// Action keys
inline constexpr char32_t UKEY_CBM_RUN_STOP   = U'\uE004';  // RUN/STOP
inline constexpr char32_t UKEY_CBM_RESTORE    = U'\uE005';  // RESTORE (NMI, not in matrix)
inline constexpr char32_t UKEY_CBM_DEL        = U'\uE006';  // INST/DEL
inline constexpr char32_t UKEY_CBM_HOME       = U'\uE007';  // HOME/CLR

// Navigation
inline constexpr char32_t UKEY_CBM_CURSOR_DN  = U'\uE008';  // Cursor Down (↓)
inline constexpr char32_t UKEY_CBM_CURSOR_RT  = U'\uE009';  // Cursor Right (→)
inline constexpr char32_t UKEY_CBM_CURSOR_UP  = U'\uE00A';  // Cursor Up (C16/Plus4, C128)
inline constexpr char32_t UKEY_CBM_CURSOR_LT  = U'\uE00B';  // Cursor Left (C16/Plus4, C128)

// Function keys
inline constexpr char32_t UKEY_CBM_F1         = U'\uE00C';  // F1
inline constexpr char32_t UKEY_CBM_F2         = U'\uE00D';  // F2 (C16/Plus4 physical key)
inline constexpr char32_t UKEY_CBM_F3         = U'\uE00E';  // F3
inline constexpr char32_t UKEY_CBM_F5         = U'\uE00F';  // F5
inline constexpr char32_t UKEY_CBM_F7         = U'\uE010';  // F7 / HELP

// PET-specific
inline constexpr char32_t UKEY_CBM_REVERSE    = U'\uE018';  // REVERSE (RVS ON/OFF)

// C128-specific
inline constexpr char32_t UKEY_CBM_ALT        = U'\uE020';  // ALT key
inline constexpr char32_t UKEY_CBM_HELP       = U'\uE021';  // HELP key
inline constexpr char32_t UKEY_CBM_LINE_FEED  = U'\uE022';  // LINE FEED
inline constexpr char32_t UKEY_CBM_NO_SCROLL  = U'\uE023';  // NO SCROLL
inline constexpr char32_t UKEY_CBM_TAB        = U'\uE024';  // TAB (C128 extended matrix)

// C128 numeric keypad (mostly printable but required to be distinct keys from their ASCII counterparts)
inline constexpr char32_t UKEY_CBM_KP_0       = U'\uE030';  // Numpad 0
inline constexpr char32_t UKEY_CBM_KP_1       = U'\uE031';  // Numpad 1
inline constexpr char32_t UKEY_CBM_KP_2       = U'\uE032';  // Numpad 2
inline constexpr char32_t UKEY_CBM_KP_3       = U'\uE033';  // Numpad 3
inline constexpr char32_t UKEY_CBM_KP_4       = U'\uE034';  // Numpad 4
inline constexpr char32_t UKEY_CBM_KP_5       = U'\uE035';  // Numpad 5
inline constexpr char32_t UKEY_CBM_KP_6       = U'\uE036';  // Numpad 6
inline constexpr char32_t UKEY_CBM_KP_7       = U'\uE037';  // Numpad 7
inline constexpr char32_t UKEY_CBM_KP_8       = U'\uE038';  // Numpad 8
inline constexpr char32_t UKEY_CBM_KP_9       = U'\uE039';  // Numpad 9
inline constexpr char32_t UKEY_CBM_KP_PLUS    = U'\uE03A';  // Numpad +
inline constexpr char32_t UKEY_CBM_KP_MINUS   = U'\uE03B';  // Numpad −
inline constexpr char32_t UKEY_CBM_KP_PERIOD  = U'\uE03C';  // Numpad .
inline constexpr char32_t UKEY_CBM_KP_ENTER   = U'\uE03D';  // Numpad ENTER

// ============================================================================
// Convenience tests
// ============================================================================

/// Is this a Private Use Area key (non-character)?
inline constexpr bool ukey_is_pua(char32_t c) {
    return c >= U'\uE000' && c <= U'\uF8FF';
}

/// Is this a printable character (not PUA, not UKEY_NONE)?
inline constexpr bool ukey_is_printable(char32_t c) {
    return c != UKEY_NONE && !ukey_is_pua(c);
}
