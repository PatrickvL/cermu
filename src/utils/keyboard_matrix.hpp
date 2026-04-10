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

// ── Guest Matrix Entry ───────────────────────────────────────────────

/// A single key position in a guest keyboard matrix, identified by
/// Unicode codepoint rather than SDL keycode.
///
/// Row and col are hardware bit positions that map directly to the
/// scan callback's array indices — no conversion or bit-reversal needed:
///   key_down:  row_open_contacts[row] &= ~(1 << col)
///   key_up:    row_open_contacts[row] |=  (1 << col)
///
/// `normal` is the character the key produces unshifted (or a PUA value
/// from guest_key_chars.hpp for non-character keys).
/// `shifted` is the character produced with Shift held, or 0 if the
/// shifted output is not a distinct printable character.
struct KeyMatrixEntry {
    uint8_t  row;       // hardware scan line (row_open_contacts index)
    uint8_t  col;       // hardware data line (bit position within row)
    char32_t normal;    // unshifted character (Unicode or PUA)
    char32_t shifted;   // shifted character (0 = no shifted output)
};

// ── Character Override ───────────────────────────────────────────────

/// A character override entry for keyboard matrices where the guest
/// character differs from the host SDL_Keycode identity.
///
/// Most keys on most systems produce the character implied by their
/// SDL_Keycode (SDLK_a → 'A'/'a', SDLK_2 → '2'/'"', etc.).  Only
/// the ~10-15 keys per system where guest and host disagree need an
/// explicit override.
///
/// The `modifier` field tells the mapper which guest modifier state
/// is required to produce this character (KEYMOD_NONE, KEYMOD_SHIFT).
struct KeyCharOverride {
    char32_t    character;   // Unicode codepoint the host user types
    uint8_t     row;         // Guest matrix row
    uint8_t     col;         // Guest matrix column
    uint8_t     modifier;    // Required guest modifier (KEYMOD_NONE, KEYMOD_SHIFT)
};

/// Maps a host SDL_Keycode to a guest char32_t key identity.
///
/// For most keys (letters, digits, RETURN, SPACE) the SDL_Keycode value
/// equals the char32_t value and no explicit binding is needed.  This
/// table only covers keys where the mapping is non-trivial:
///   - PUA keys:  SDLK_TAB → UKEY_CBM_RUN_STOP, SDLK_LGUI → UKEY_CBM_COMMODORE
///   - Non-ASCII: SDLK_BACKSLASH → UKEY_LEFT_ARROW (guest ← key)
struct HostKeyBinding {
    SDL_Keycode sdl_key;    // host key that fires from SDL_KEYDOWN
    char32_t    guest_key;  // guest char32_t identity from the matrix
};

// ── KeyMatrixEntry apply ─────────────────────────────────────────────

/// Apply a key press/release event to a keyboard matrix using
/// KeyMatrixEntry tables and HostKeyBinding lookup.
///
/// For most keys (letters, digits, punctuation) the SDL_Keycode value
/// maps directly to the char32_t identity used in KeyMatrixEntry::normal
/// (e.g. SDLK_a == 'a').  Non-ASCII keys (modifiers, arrows, F-keys)
/// require explicit HostKeyBinding entries to translate SDL_Keycode
/// values (which carry a scancode mask ≥ 0x40000000) to PUA constants.
///
/// Handles multi-position keys naturally: if the same char32_t identity
/// appears in multiple entries, all positions are updated (e.g. Spectrum
/// BACKSPACE → CAPS_SHIFT + 0).
inline bool keyboard_matrix_apply(const KeyMatrixEntry* entries,
                                  int num_entries,
                                  const HostKeyBinding* bindings,
                                  int num_bindings,
                                  uint8_t* matrix,
                                  SDL_Keycode key,
                                  bool pressed) {
    // Resolve SDL keycode to guest key identity
    char32_t guest = static_cast<char32_t>(key);
    for (int i = 0; i < num_bindings; ++i) {
        if (bindings[i].sdl_key == key) {
            guest = bindings[i].guest_key;
            break;
        }
    }

    // Search entries for matching key identity
    bool matched = false;
    for (int i = 0; i < num_entries; ++i) {
        if (entries[i].normal == guest) {
            uint8_t row = entries[i].row;
            uint8_t bit = static_cast<uint8_t>(1u << entries[i].col);
            if (pressed)
                matrix[row] &= ~bit;   // active-low: clear = pressed
            else
                matrix[row] |= bit;    // active-low: set   = released
            matched = true;
        }
    }
    return matched;
}

/// Convenience overload for statically-sized KeyMatrixEntry + HostKeyBinding arrays.
template<int NE, int NB>
inline bool keyboard_matrix_apply(const KeyMatrixEntry (&entries)[NE],
                                  const HostKeyBinding (&bindings)[NB],
                                  uint8_t* matrix,
                                  SDL_Keycode key,
                                  bool pressed) {
    return keyboard_matrix_apply(entries, NE, bindings, NB, matrix, key, pressed);
}
