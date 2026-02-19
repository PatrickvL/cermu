#include "c16_keyboard_matrix.h"

// ============================================================================
// C16 / Plus/4 Keyboard Matrix — 8×8 (EmuKey-based)
// ============================================================================
// TED PIO2 ($FD30) selects which row(s) to scan (active-low output).
// TED register $FF08 reads the column result (active-low input).
//
// Matrix layout verified against VICE emulator (data/PLUS4/gtk3_pos.vkm)
// and Commodore 264 Hardware Specification.
//
// Key differences from C64:
//   - Dedicated cursor keys: UP (5,3), DOWN (5,0), LEFT (6,0), RIGHT (6,3)
//   - ESC key at (6,4)
//   - Both SHIFT keys wired to same position (1,7)
//   - Function keys: F1/F4 (0,4), F2/F5 (0,5), F3/F6 (0,6), HELP/F7 (0,3)
//   - No separate RESTORE key in matrix (wired directly to NMI)

// Key positions note (same Commodore physical layout conventions):
//   '@' key → EMUKEY_LEFTBRACKET
//   '*' key → EMUKEY_RIGHTBRACKET
//   ':' key → EMUKEY_SEMICOLON
//   ';' key → EMUKEY_APOSTROPHE
//   '+' key → EMUKEY_BACKSLASH

static const emu_key_t c16_keys[C16_KEYBOARD_ROWS * C16_KEYBOARD_COLS] = {
    // Row 0: DEL, RETURN, POUND, HELP(F7), F1, F2, F3, @
    EMUKEY_BACKSPACE,  EMUKEY_RETURN,  EMUKEY_CBM_POUND,  EMUKEY_F7,  EMUKEY_F1,  EMUKEY_F2,  EMUKEY_F3,  EMUKEY_LEFTBRACKET,
    // Row 1: 3, W, A, 4, Z, S, E, SHIFT (both L/R share this position)
    EMUKEY_3,  EMUKEY_W,  EMUKEY_A,  EMUKEY_4,  EMUKEY_Z,  EMUKEY_S,  EMUKEY_E,  EMUKEY_LSHIFT,
    // Row 2: 5, R, D, 6, C, F, T, X
    EMUKEY_5,  EMUKEY_R,  EMUKEY_D,  EMUKEY_6,  EMUKEY_C,  EMUKEY_F,  EMUKEY_T,  EMUKEY_X,
    // Row 3: 7, Y, G, 8, B, H, U, V
    EMUKEY_7,  EMUKEY_Y,  EMUKEY_G,  EMUKEY_8,  EMUKEY_B,  EMUKEY_H,  EMUKEY_U,  EMUKEY_V,
    // Row 4: 9, I, J, 0, M, K, O, N
    EMUKEY_9,  EMUKEY_I,  EMUKEY_J,  EMUKEY_0,  EMUKEY_M,  EMUKEY_K,  EMUKEY_O,  EMUKEY_N,
    // Row 5: CRSR↓, P, L, CRSR↑, ., :, -, ,
    EMUKEY_DOWN,  EMUKEY_P,  EMUKEY_L,  EMUKEY_UP,  EMUKEY_PERIOD,  EMUKEY_SEMICOLON,  EMUKEY_MINUS,  EMUKEY_COMMA,
    // Row 6: CRSR←, *, ;, CRSR→, ESC, =, +, /
    EMUKEY_LEFT,  EMUKEY_RIGHTBRACKET,  EMUKEY_APOSTROPHE,  EMUKEY_RIGHT,  EMUKEY_ESCAPE,  EMUKEY_EQUALS,  EMUKEY_BACKSLASH,  EMUKEY_SLASH,
    // Row 7: 1, HOME, CTRL, 2, SPACE, COMMODORE, Q, RUN/STOP
    EMUKEY_1,  EMUKEY_HOME,  EMUKEY_LCTRL,  EMUKEY_2,  EMUKEY_SPACE,  EMUKEY_LGUI,  EMUKEY_Q,  EMUKEY_TAB,
};

// Unshifted character decode table — PETSCII codes per matrix position.
// 0 = non-character key (modifier, function key, cursor key, RETURN, DEL).
// C16/Plus4: £ = $5C.  No ← or ↑ dedicated keys in the matrix.
static const petscii_t c16_unshifted_chars[C16_KEYBOARD_ROWS * C16_KEYBOARD_COLS] = {
    // Row 0: (DEL), (RETURN), £($5C), (HELP/F7), (F1), (F2), (F3), @
    0, 0, 0x5C, 0, 0, 0, 0, '@',
    // Row 1: 3, W, A, 4, Z, S, E, (SHIFT)
    '3', 'W', 'A', '4', 'Z', 'S', 'E', 0,
    // Row 2: 5, R, D, 6, C, F, T, X
    '5', 'R', 'D', '6', 'C', 'F', 'T', 'X',
    // Row 3: 7, Y, G, 8, B, H, U, V
    '7', 'Y', 'G', '8', 'B', 'H', 'U', 'V',
    // Row 4: 9, I, J, 0, M, K, O, N
    '9', 'I', 'J', '0', 'M', 'K', 'O', 'N',
    // Row 5: (CRSR↓), P, L, (CRSR↑), ., :, -, ,
    0, 'P', 'L', 0, '.', ':', '-', ',',
    // Row 6: (CRSR←), *, ;, (CRSR→), (ESC), =, +, /
    0, '*', ';', 0, 0, '=', '+', '/',
    // Row 7: 1, (HOME), (CTRL), 2, SPACE, (C=), Q, (RUN/STOP)
    '1', 0, 0, '2', ' ', 0, 'Q', 0,
};

// Shifted character decode table — PETSCII codes per matrix position.
// Letters use PETSCII lowercase ($C1–$DA) for character-accurate mapping.
// 0 = no distinct character (modifier key, function key, cursor key,
//     or same character as unshifted — handled by KERNAL/TED at runtime).
static const petscii_t c16_shifted_chars[C16_KEYBOARD_ROWS * C16_KEYBOARD_COLS] = {
    // Row 0: (INST), (RETURN), (£), (HELP), (F4), (F5), (F6), (@)
    0, 0, 0, 0, 0, 0, 0, 0,
    // Row 1: #, w($D7), a($C1), $, z($DA), s($D3), e($C5), (SHIFT)
    '#', 0xD7, 0xC1, '$', 0xDA, 0xD3, 0xC5, 0,
    // Row 2: %, r($D2), d($C4), &, c($C3), f($C6), t($D4), x($D8)
    '%', 0xD2, 0xC4, '&', 0xC3, 0xC6, 0xD4, 0xD8,
    // Row 3: ', y($D9), g($C7), (, b($C2), h($C8), u($D5), v($D6)
    '\'', 0xD9, 0xC7, '(', 0xC2, 0xC8, 0xD5, 0xD6,
    // Row 4: ), i($C9), j($CA), (0), m($CD), k($CB), o($CF), n($CE)
    ')', 0xC9, 0xCA, 0, 0xCD, 0xCB, 0xCF, 0xCE,
    // Row 5: (CRSR↓), p($D0), l($CC), (CRSR↑), >, [, (-), <
    0, 0xD0, 0xCC, 0, '>', '[', 0, '<',
    // Row 6: (CRSR←), (*), ], (CRSR→), (ESC), (=), (+), ?
    0, 0, ']', 0, 0, 0, 0, '?',
    // Row 7: !, (CLR), (CTRL), ", (SPACE), (C=), q($D1), (RUN/STOP)
    '!', 0, 0, '"', 0, 0, 0xD1, 0,
};

static const keyboard_decode_table_t c16_decode_tables[] = {
    { KEYMOD_NONE,  c16_unshifted_chars },
    { KEYMOD_SHIFT, c16_shifted_chars },
    // Future: { KEYMOD_CBM,  c16_cbm_chars },
};

const keyboard_matrix_config_t c16_keyboard_config = {
    .model = KEYBOARD_MODEL_PLUS4_C16,
    .scan_chip = KEYBOARD_SCAN_TED,
    .rows = C16_KEYBOARD_ROWS,
    .cols = C16_KEYBOARD_COLS,
    .description = "C16/Plus4 8x8 keyboard matrix (TED 7360)",
    .keys = c16_keys,
    .num_decode_tables = sizeof(c16_decode_tables) / sizeof(c16_decode_tables[0]),
    .decode_tables = c16_decode_tables,
};
