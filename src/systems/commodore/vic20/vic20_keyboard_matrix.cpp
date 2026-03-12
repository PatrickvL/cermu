#include "systems/commodore/vic20/vic20_keyboard_matrix.h"

// ============================================================================
// VIC-20 Keyboard Matrix — 8×8 (EmuKey-based)
// ============================================================================
// VIA Port B ($9120) = column select (output)
// VIA Port A ($9121) = row read (input)
// Array convention: array[7-PB_col][7-PA_row]
//
// keys[] stores EmuKey values.  Shifted characters stored in decode tables.
// Unshifted characters derived from emu_key_to_char(key).

// Key positions note:
//   C64 '@' key → EMUKEY_LEFTBRACKET  (physical position of @ on Commodore)
//   C64 '*' key → EMUKEY_RIGHTBRACKET (physical position of * on Commodore)
//   C64 ':' key → EMUKEY_SEMICOLON    (Commodore : is on the ; key position)
//   C64 ';' key → EMUKEY_APOSTROPHE   (Commodore ; is on the ' key position)
//   C64 '+' key → EMUKEY_BACKSLASH    (Commodore + key, no direct ASCII match)

static const emu_key_t vic20_keys[VIC20_KEYBOARD_ROWS * VIC20_KEYBOARD_COLS] = {
    // PB7: F7, HOME, -, 0, 8, 6, 4, 2
    EMUKEY_F7,  EMUKEY_HOME,  EMUKEY_MINUS,  EMUKEY_0,  EMUKEY_8,  EMUKEY_6,  EMUKEY_4,  EMUKEY_2,
    // PB6: F5, ↑(char), @, O, U, T, E, Q
    EMUKEY_F5,  EMUKEY_CBM_ARROW_UP,  EMUKEY_LEFTBRACKET,  EMUKEY_O,  EMUKEY_U,  EMUKEY_T,  EMUKEY_E,  EMUKEY_Q,
    // PB5: F3, =, :, K, H, F, S, C=
    EMUKEY_F3,  EMUKEY_EQUALS,  EMUKEY_SEMICOLON,  EMUKEY_K,  EMUKEY_H,  EMUKEY_F,  EMUKEY_S,  EMUKEY_LGUI,
    // PB4: F1, RSHIFT, ., M, B, C, Z, SPACE
    EMUKEY_F1,  EMUKEY_RSHIFT,  EMUKEY_PERIOD,  EMUKEY_M,  EMUKEY_B,  EMUKEY_C,  EMUKEY_Z,  EMUKEY_SPACE,
    // PB3: CRSR↓, /, ,, N, V, X, LSHIFT, RUN/STOP
    EMUKEY_DOWN,  EMUKEY_SLASH,  EMUKEY_COMMA,  EMUKEY_N,  EMUKEY_V,  EMUKEY_X,  EMUKEY_LSHIFT,  EMUKEY_TAB,
    // PB2: CRSR→, ;, L, J, G, D, A, CTRL
    EMUKEY_RIGHT,  EMUKEY_APOSTROPHE,  EMUKEY_L,  EMUKEY_J,  EMUKEY_G,  EMUKEY_D,  EMUKEY_A,  EMUKEY_LCTRL,
    // PB1: RETURN, *, P, I, Y, R, W, ←(char)
    EMUKEY_RETURN,  EMUKEY_RIGHTBRACKET,  EMUKEY_P,  EMUKEY_I,  EMUKEY_Y,  EMUKEY_R,  EMUKEY_W,  EMUKEY_CBM_ARROW_LEFT,
    // PB0: DEL, £, +, 9, 7, 5, 3, 1
    EMUKEY_BACKSPACE,  EMUKEY_CBM_POUND,  EMUKEY_BACKSLASH,  EMUKEY_9,  EMUKEY_7,  EMUKEY_5,  EMUKEY_3,  EMUKEY_1,
};

// Unshifted character decode table — PETSCII codes per matrix position.
// Mirrors the VIC-20 KERNAL's normal (unshifted) decode table at $EC5E.
// 0 = non-character key (modifier, function key, cursor key, RETURN, DEL).
// Commodore-specific: ← = $5F, ↑ = $5E, £ = $5C.
static const petscii_t vic20_unshifted_chars[VIC20_KEYBOARD_ROWS * VIC20_KEYBOARD_COLS] = {
    // PB7: (F7), (HOME), -, 0, 8, 6, 4, 2
    0, 0, '-', '0', '8', '6', '4', '2',
    // PB6: (F5), ↑($5E), @, O, U, T, E, Q
    0, 0x5E, '@', 'O', 'U', 'T', 'E', 'Q',
    // PB5: (F3), =, :, K, H, F, S, (C=)
    0, '=', ':', 'K', 'H', 'F', 'S', 0,
    // PB4: (F1), (RSHIFT), ., M, B, C, Z, SPACE
    0, 0, '.', 'M', 'B', 'C', 'Z', ' ',
    // PB3: (CRSR↓), /, ,, N, V, X, (LSHIFT), (RUN/STOP)
    0, '/', ',', 'N', 'V', 'X', 0, 0,
    // PB2: (CRSR→), ;, L, J, G, D, A, (CTRL)
    0, ';', 'L', 'J', 'G', 'D', 'A', 0,
    // PB1: (RETURN), *, P, I, Y, R, W, ←($5F)
    0, '*', 'P', 'I', 'Y', 'R', 'W', 0x5F,
    // PB0: (DEL), £($5C), +, 9, 7, 5, 3, 1
    0, 0x5C, '+', '9', '7', '5', '3', '1',
};

// Shifted character decode table — PETSCII codes per matrix position.
// Mirrors the VIC-20 KERNAL's shifted decode table at $EC9F.
// Letters use PETSCII lowercase ($C1–$DA) for character-accurate mapping.
// 0 = no distinct character (modifier key, function key, cursor key,
//     or same character as unshifted — handled by KERNAL ROM at runtime).
static const petscii_t vic20_shifted_chars[VIC20_KEYBOARD_ROWS * VIC20_KEYBOARD_COLS] = {
    // PB7: (F8), (CLR), (-), (0), (, &, $, "
    0, 0, 0, 0, '(', '&', '$', '"',
    // PB6: (F6), π($DE), (@), o($CF), u($D5), t($D4), e($C5), q($D1)
    0, 0xDE, 0, 0xCF, 0xD5, 0xD4, 0xC5, 0xD1,
    // PB5: (F4), (=), [, k($CB), h($C8), f($C6), s($D3), (C=)
    0, 0, '[', 0xCB, 0xC8, 0xC6, 0xD3, 0,
    // PB4: (F2), (RSHIFT), >, m($CD), b($C2), c($C3), z($DA), (SPACE)
    0, 0, '>', 0xCD, 0xC2, 0xC3, 0xDA, 0,
    // PB3: (CRSR↑), ?, <, n($CE), v($D6), x($D8), (LSHIFT), (RUN/STOP)
    0, '?', '<', 0xCE, 0xD6, 0xD8, 0, 0,
    // PB2: (CRSR←), ], l($CC), j($CA), g($C7), d($C4), a($C1), (CTRL)
    0, ']', 0xCC, 0xCA, 0xC7, 0xC4, 0xC1, 0,
    // PB1: (RETURN), (*), p($D0), i($C9), y($D9), r($D2), w($D7), (←)
    0, 0, 0xD0, 0xC9, 0xD9, 0xD2, 0xD7, 0,
    // PB0: (INST), (£), (+), ), ', %, #, !
    0, 0, 0, ')', '\'', '%', '#', '!',
};

static const keyboard_decode_table_t vic20_decode_tables[] = {
    { KEYMOD_NONE,  vic20_unshifted_chars },
    { KEYMOD_SHIFT, vic20_shifted_chars },
    // Future: { KEYMOD_CBM,  vic20_cbm_chars },
};

const keyboard_matrix_config_t vic20_keyboard_config = {
    .model = KEYBOARD_MODEL_VIC20,
    .scan_chip = KEYBOARD_SCAN_VIA,
    .rows = VIC20_KEYBOARD_ROWS,
    .cols = VIC20_KEYBOARD_COLS,
    .description = "VIC-20 8x8 keyboard matrix",
    .keys = vic20_keys,
    .num_decode_tables = sizeof(vic20_decode_tables) / sizeof(vic20_decode_tables[0]),
    .decode_tables = vic20_decode_tables,
};
