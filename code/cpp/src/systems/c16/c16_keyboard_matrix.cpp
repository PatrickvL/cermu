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

// Shifted character decode table — ASCII characters per matrix position.
// 0 = no distinct character (modifier key, function key, cursor key,
//     or same character as unshifted — handled by KERNAL/TED at runtime).
static const uint8_t c16_shifted_chars[C16_KEYBOARD_ROWS * C16_KEYBOARD_COLS] = {
    // Row 0: (INST), (RETURN), (£), (HELP), (F4), (F5), (F6), (@)
    0, 0, 0, 0, 0, 0, 0, 0,
    // Row 1: #, W, A, $, Z, S, E, (SHIFT)
    '#', 'W', 'A', '$', 'Z', 'S', 'E', 0,
    // Row 2: %, R, D, &, C, F, T, X
    '%', 'R', 'D', '&', 'C', 'F', 'T', 'X',
    // Row 3: ', Y, G, (, B, H, U, V
    '\'', 'Y', 'G', '(', 'B', 'H', 'U', 'V',
    // Row 4: ), I, J, (0), M, K, O, N
    ')', 'I', 'J', 0, 'M', 'K', 'O', 'N',
    // Row 5: (CRSR↓), P, L, (CRSR↑), >, [, (-), <
    0, 'P', 'L', 0, '>', '[', 0, '<',
    // Row 6: (CRSR←), (*), ], (CRSR→), (ESC), (=), (+), ?
    0, 0, ']', 0, 0, 0, 0, '?',
    // Row 7: !, (CLR), (CTRL), ", (SPACE), (C=), Q, (RUN/STOP)
    '!', 0, 0, '"', 0, 0, 'Q', 0,
};

static const keyboard_decode_table_t c16_decode_tables[] = {
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
