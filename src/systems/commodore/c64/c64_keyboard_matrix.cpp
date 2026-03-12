#include "systems/commodore/c64/c64_keyboard_matrix.h"

// ============================================================================
// C64 Keyboard Matrix — 8×8 (EmuKey-based)
// ============================================================================
// CIA Port A ($DC00) = column select (output)
// CIA Port B ($DC01) = row read (input)
// Array convention: array[7-PB_bit][7-PA_bit]
//
// The keys[] table stores EmuKey values identifying each physical key.
// The decode tables store the ASCII character produced when the key is
// pressed with a specific modifier (SHIFT, C=, CTRL), or 0 if no
// distinct character is produced (the KERNAL ROM handles it at runtime).
//
// Unshifted characters are derived from emu_key_to_char(key).
// NOTE on cursor keys: CRSR→ and CRSR↓ are the only physical cursor keys.
//   Host LEFT/UP are handled via auto-shift in key_down/key_up.

static const emu_key_t c64_keys[C64_KEYBOARD_ROWS * C64_KEYBOARD_COLS] = {
    // row 7: RUN/STOP, /, ,, N, V, X, LSHIFT, CRSR↓
    EMUKEY_TAB,  EMUKEY_SLASH,  EMUKEY_COMMA,  EMUKEY_N,  EMUKEY_V,  EMUKEY_X,  EMUKEY_LSHIFT,  EMUKEY_DOWN,
    // row 6: Q, ↑(char), @, O, U, T, E, F5
    EMUKEY_Q,  EMUKEY_CBM_ARROW_UP,  EMUKEY_LEFTBRACKET,  EMUKEY_O,  EMUKEY_U,  EMUKEY_T,  EMUKEY_E,  EMUKEY_F5,
    // row 5: C=, =, :, K, H, F, S, F3
    EMUKEY_LGUI,  EMUKEY_EQUALS,  EMUKEY_SEMICOLON,  EMUKEY_K,  EMUKEY_H,  EMUKEY_F,  EMUKEY_S,  EMUKEY_F3,
    // row 4: SPACE, RSHIFT, ., M, B, C, Z, F1
    EMUKEY_SPACE,  EMUKEY_RSHIFT,  EMUKEY_PERIOD,  EMUKEY_M,  EMUKEY_B,  EMUKEY_C,  EMUKEY_Z,  EMUKEY_F1,
    // row 3: 2, HOME, -, 0, 8, 6, 4, F7
    EMUKEY_2,  EMUKEY_HOME,  EMUKEY_MINUS,  EMUKEY_0,  EMUKEY_8,  EMUKEY_6,  EMUKEY_4,  EMUKEY_F7,
    // row 2: CTRL, ;, L, J, G, D, A, CRSR→
    EMUKEY_LCTRL,  EMUKEY_APOSTROPHE,  EMUKEY_L,  EMUKEY_J,  EMUKEY_G,  EMUKEY_D,  EMUKEY_A,  EMUKEY_RIGHT,
    // row 1: ←(char), *, P, I, Y, R, W, RETURN
    EMUKEY_CBM_ARROW_LEFT,  EMUKEY_RIGHTBRACKET,  EMUKEY_P,  EMUKEY_I,  EMUKEY_Y,  EMUKEY_R,  EMUKEY_W,  EMUKEY_RETURN,
    // row 0: 1, £, +, 9, 7, 5, 3, DEL
    EMUKEY_1,  EMUKEY_CBM_POUND,  EMUKEY_BACKSLASH,  EMUKEY_9,  EMUKEY_7,  EMUKEY_5,  EMUKEY_3,  EMUKEY_BACKSPACE,
};

// Unshifted character decode table — PETSCII codes per matrix position.
// Mirrors the C64 KERNAL's normal (unshifted) decode table at $EB81.
// Values match the PETSCII code the KERNAL puts in the keyboard buffer
// when a key is pressed with no modifiers.
// 0 = non-character key (modifier, function key, cursor key, RETURN, DEL).
//
// Commodore-specific characters use their PETSCII codes:
//   ← (left arrow) = $5F,  ↑ (up arrow) = $5E,  £ (pound) = $5C
// These diverge from ASCII ($5F=_, $5E=^, $5C=\) — petscii_to_host_char()
// bridges the mapping.
static const petscii_t c64_unshifted_chars[C64_KEYBOARD_ROWS * C64_KEYBOARD_COLS] = {
    // row 7: (RUN/STOP), /, ,, N, V, X, (LSHIFT), (CRSR↓)
    0, '/', ',', 'N', 'V', 'X', 0, 0,
    // row 6: Q, ↑($5E), @, O, U, T, E, (F5)
    'Q', 0x5E, '@', 'O', 'U', 'T', 'E', 0,
    // row 5: (C=), =, :, K, H, F, S, (F3)
    0, '=', ':', 'K', 'H', 'F', 'S', 0,
    // row 4: SPACE, (RSHIFT), ., M, B, C, Z, (F1)
    ' ', 0, '.', 'M', 'B', 'C', 'Z', 0,
    // row 3: 2, (HOME), -, 0, 8, 6, 4, (F7)
    '2', 0, '-', '0', '8', '6', '4', 0,
    // row 2: (CTRL), ;, L, J, G, D, A, (CRSR→)
    0, ';', 'L', 'J', 'G', 'D', 'A', 0,
    // row 1: ←($5F), *, P, I, Y, R, W, (RETURN)
    0x5F, '*', 'P', 'I', 'Y', 'R', 'W', 0,
    // row 0: 1, £($5C), +, 9, 7, 5, 3, (DEL)
    '1', 0x5C, '+', '9', '7', '5', '3', 0,
};

// Shifted character decode table — PETSCII codes per matrix position.
// Mirrors the C64 KERNAL's shifted decode table at $EBC2.
// Letters use PETSCII lowercase ($C1–$DA), enabling character-accurate
// mapping: host lowercase 'a' → guest Shift+A → C64 lowercase 'a'.
// 0 = no distinct character (modifier key, function key, cursor key,
//     or same character as unshifted — handled by KERNAL ROM at runtime).
static const petscii_t c64_shifted_chars[C64_KEYBOARD_ROWS * C64_KEYBOARD_COLS] = {
    // row 7: (RUN/STOP), ?, <, n($CE), v($D6), x($D8), (LSHIFT), (CRSR↑ via auto-shift)
    0, '?', '<', 0xCE, 0xD6, 0xD8, 0, 0,
    // row 6: q($D1), π($DE), (@), o($CF), u($D5), t($D4), e($C5), (F6)
    0xD1, 0xDE, 0, 0xCF, 0xD5, 0xD4, 0xC5, 0,
    // row 5: (C=), (=), [, k($CB), h($C8), f($C6), s($D3), (F4)
    0, 0, '[', 0xCB, 0xC8, 0xC6, 0xD3, 0,
    // row 4: (SPACE), (RSHIFT), >, m($CD), b($C2), c($C3), z($DA), (F2)
    0, 0, '>', 0xCD, 0xC2, 0xC3, 0xDA, 0,
    // row 3: ", (CLR), (-), (0), (, &, $, (F8)
    '"', 0, 0, 0, '(', '&', '$', 0,
    // row 2: (CTRL), ], l($CC), j($CA), g($C7), d($C4), a($C1), (CRSR← via auto-shift)
    0, ']', 0xCC, 0xCA, 0xC7, 0xC4, 0xC1, 0,
    // row 1: (←), (*), p($D0), i($C9), y($D9), r($D2), w($D7), (RETURN)
    0, 0, 0xD0, 0xC9, 0xD9, 0xD2, 0xD7, 0,
    // row 0: !, (£), (+), ), ', %, #, (INST)
    '!', 0, 0, ')', '\'', '%', '#', 0,
};

static const keyboard_decode_table_t c64_decode_tables[] = {
    { KEYMOD_NONE,  c64_unshifted_chars },
    { KEYMOD_SHIFT, c64_shifted_chars },
    // Future: { KEYMOD_CBM,  c64_cbm_chars },   — C= key graphics characters
    // Future: { KEYMOD_CTRL, c64_ctrl_chars },   — CTRL key colour codes
};

const keyboard_matrix_config_t c64_keyboard_config = {
    .model = KEYBOARD_MODEL_C64,
    .scan_chip = KEYBOARD_SCAN_CIA,
    .rows = C64_KEYBOARD_ROWS,
    .cols = C64_KEYBOARD_COLS,
    .description = "C64 8x8 keyboard matrix",
    .keys = c64_keys,
    .num_decode_tables = sizeof(c64_decode_tables) / sizeof(c64_decode_tables[0]),
    .decode_tables = c64_decode_tables,
};
