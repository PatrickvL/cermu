#pragma once
// keyboard_matrix.hpp — Shared keyboard matrix utilities
//
// Most home computers use an N×M keyboard matrix scanned by the CPU.
// The host-side pattern is always the same:
//   - An array of uint8_t (one per row), active-low (0 = pressed).
//   - A static table mapping SDL keycodes to (row, col) positions.
//   - On key-down: clear the bit.  On key-up: set the bit.
//
// This header eliminates the per-system copy-paste of that logic.

#include <SDL_keycode.h>
#include <cstdint>
#include <cstring>

// ── Types ────────────────────────────────────────────────────────────

/// A single entry in a keyboard mapping table.
/// One SDL keycode maps to one matrix position (row, col).
/// A key that affects multiple positions (e.g. Backspace → SHIFT + 0)
/// appears as multiple entries with the same sdl_key.
struct KeyMatrixMapping {
    SDL_Keycode sdl_key;
    uint8_t     row;
    uint8_t     col;        // bit index within the row byte
};

// ── Functions ────────────────────────────────────────────────────────

/// Reset all rows to 0xFF (all keys released, active-low convention).
inline void keyboard_matrix_reset(uint8_t* matrix, int num_rows) {
    std::memset(matrix, 0xFF, static_cast<size_t>(num_rows));
}

/// Apply a key press/release event to a keyboard matrix.
///
/// Iterates the mapping table and for every entry matching `key`:
///   - pressed  → clears bit `col` in matrix[row]  (contact closed)
///   - released → sets   bit `col` in matrix[row]  (contact open)
///
/// Handles multi-position keys naturally: if the same SDL_Keycode
/// appears in multiple table entries, all positions are updated.
///
/// @param mappings     Pointer to the mapping table (static constexpr).
/// @param num_mappings Number of entries in the table.
/// @param matrix       Pointer to the row array (uint8_t[num_rows]).
/// @param key          The SDL keycode from the event.
/// @param pressed      true = key down, false = key up.
/// @return true if at least one mapping matched (key was consumed).
inline bool keyboard_matrix_apply(const KeyMatrixMapping* mappings,
                                  int num_mappings,
                                  uint8_t* matrix,
                                  SDL_Keycode key,
                                  bool pressed) {
    bool matched = false;
    for (int i = 0; i < num_mappings; ++i) {
        if (mappings[i].sdl_key == key) {
            uint8_t row = mappings[i].row;
            uint8_t bit = static_cast<uint8_t>(1u << mappings[i].col);
            if (pressed)
                matrix[row] &= ~bit;   // active-low: clear = pressed
            else
                matrix[row] |= bit;    // active-low: set   = released
            matched = true;
        }
    }
    return matched;
}

/// Convenience overload for statically-sized mapping arrays.
/// Usage:
///   static constexpr KeyMatrixMapping mappings[] = { ... };
///   keyboard_matrix_apply(mappings, matrix, key, pressed);
template<int N>
inline bool keyboard_matrix_apply(const KeyMatrixMapping (&mappings)[N],
                                  uint8_t* matrix,
                                  SDL_Keycode key,
                                  bool pressed) {
    return keyboard_matrix_apply(mappings, N, matrix, key, pressed);
}
