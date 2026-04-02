#include "systems/commodore/c128/c128_keyboard_matrix.hpp"

// ============================================================================
// C128 Keyboard Matrix — 11 columns × 8 rows (EmuKey-based)
// ============================================================================
// CIA Port A ($DC00) = column select (output, active-low)
// CIA Port B ($DC01) = row read (input, active-low)
//
// Columns 0–7 are identical to the C64 keyboard matrix.
// Columns 8–10 add C128-specific keys and the numeric keypad.
//
// The 40/80 DISPLAY key is a hardware switch read by the 8722 MMU
// (MCR bit 5) and is not part of the keyboard matrix.
// CAPS LOCK is shift-lock on the LSHIFT position (row 4, col 6).

static const emu_key_t c128_keys[C128_KEYBOARD_ROWS * C128_KEYBOARD_COLS] = {
    // ── Columns 0–7: identical to C64.  Columns 8–10: C128 extended. ──
    //                 col 0                col 1                col 2              col 3      col 4      col 5      col 6          col 7             col 8                col 9                col 10
    // row 7 (PB7): RUN/STOP, /, ,, N, V, X, LSHIFT, CRSR↓,   KP1, KP3, NO SCROLL
    EMUKEY_TAB,  EMUKEY_SLASH,           EMUKEY_COMMA,       EMUKEY_N,  EMUKEY_V,  EMUKEY_X,  EMUKEY_LSHIFT,  EMUKEY_DOWN,      EMUKEY_KP_1,         EMUKEY_KP_3,         EMUKEY_CBM_NO_SCROLL,
    // row 6 (PB6): Q, ↑(char), @, O, U, T, E, F5,            KP7, KP9, →
    EMUKEY_Q,    EMUKEY_CBM_ARROW_UP,    EMUKEY_LEFTBRACKET, EMUKEY_O,  EMUKEY_U,  EMUKEY_T,  EMUKEY_E,       EMUKEY_F5,        EMUKEY_KP_7,         EMUKEY_KP_9,         EMUKEY_RIGHT,
    // row 5 (PB5): C=, =, :, K, H, F, S, F3,                  KP4, KP6, ←
    EMUKEY_LGUI, EMUKEY_EQUALS,          EMUKEY_SEMICOLON,   EMUKEY_K,  EMUKEY_H,  EMUKEY_F,  EMUKEY_S,       EMUKEY_F3,        EMUKEY_KP_4,         EMUKEY_KP_6,         EMUKEY_LEFT,
    // row 4 (PB4): SPACE, RSHIFT, ., M, B, C, Z, F1,          KP2, KP ENTER, ↓
    EMUKEY_SPACE,EMUKEY_RSHIFT,          EMUKEY_PERIOD,      EMUKEY_M,  EMUKEY_B,  EMUKEY_C,  EMUKEY_Z,       EMUKEY_F1,        EMUKEY_KP_2,         EMUKEY_KP_ENTER,     EMUKEY_DOWN,
    // row 3 (PB3): 2, HOME, -, 0, 8, 6, 4, F7,                TAB, LINE FEED, ↑
    EMUKEY_2,    EMUKEY_HOME,            EMUKEY_MINUS,       EMUKEY_0,  EMUKEY_8,  EMUKEY_6,  EMUKEY_4,       EMUKEY_F7,        EMUKEY_TAB,          EMUKEY_CBM_LINE_FEED,EMUKEY_UP,
    // row 2 (PB2): CTRL, ;, L, J, G, D, A, CRSR→,             KP5, KP−, KP.
    EMUKEY_LCTRL,EMUKEY_APOSTROPHE,      EMUKEY_L,           EMUKEY_J,  EMUKEY_G,  EMUKEY_D,  EMUKEY_A,       EMUKEY_RIGHT,     EMUKEY_KP_5,         EMUKEY_KP_MINUS,     EMUKEY_KP_PERIOD,
    // row 1 (PB1): ←(char), *, P, I, Y, R, W, RETURN,         KP8, KP+, KP0
    EMUKEY_CBM_ARROW_LEFT, EMUKEY_RIGHTBRACKET, EMUKEY_P,    EMUKEY_I,  EMUKEY_Y,  EMUKEY_R,  EMUKEY_W,       EMUKEY_RETURN,    EMUKEY_KP_8,         EMUKEY_KP_PLUS,      EMUKEY_KP_0,
    // row 0 (PB0): 1, £, +, 9, 7, 5, 3, DEL,                  HELP, ESC, ALT
    EMUKEY_1,    EMUKEY_CBM_POUND,       EMUKEY_BACKSLASH,   EMUKEY_9,  EMUKEY_7,  EMUKEY_5,  EMUKEY_3,       EMUKEY_BACKSPACE, EMUKEY_CBM_HELP,     EMUKEY_ESCAPE,       EMUKEY_CBM_ALT,
};

// ── PETSCII decode tables ───────────────────────────────────────────

// Unshifted character decode table — PETSCII codes per matrix position.
// Columns 0–7 are identical to the C64 KERNAL's normal decode table.
// Columns 8–10 cover numeric keypad and C128-specific keys.
// 0 = non-character key (modifier, function key, cursor key).
static const petscii_t c128_unshifted_chars[C128_KEYBOARD_ROWS * C128_KEYBOARD_COLS] = {
    //                col0  col1  col2  col3  col4  col5  col6  col7  col8  col9  col10
    // row 7 (PB7)
    /* RUN/STOP */  0, '/', ',', 'N', 'V', 'X',  0,   0,    '1',  '3',   0,
    // row 6 (PB6)
    'Q', 0x5E, '@', 'O', 'U', 'T', 'E',  0,    '7',  '9',   0,
    // row 5 (PB5)
     0,  '=', ':', 'K', 'H', 'F', 'S',   0,    '4',  '6',   0,
    // row 4 (PB4)
    ' ',  0,  '.', 'M', 'B', 'C', 'Z',   0,    '2',   0,    0,
    // row 3 (PB3)
    '2',  0,  '-', '0', '8', '6', '4',    0,     0,    0,    0,
    // row 2 (PB2)
     0,  ';', 'L', 'J', 'G', 'D', 'A',   0,    '5',  '-',  '.',
    // row 1 (PB1)
    0x5F,'*', 'P', 'I', 'Y', 'R', 'W',   0,    '8',  '+',  '0',
    // row 0 (PB0)
    '1', 0x5C,'+', '9', '7', '5', '3',    0,     0,    0,    0,
};

// Shifted character decode table — PETSCII codes per matrix position.
// Letters use PETSCII lowercase ($C1–$DA).
static const petscii_t c128_shifted_chars[C128_KEYBOARD_ROWS * C128_KEYBOARD_COLS] = {
    // row 7
     0,  '?', '<', 0xCE,0xD6,0xD8, 0,    0,     0,    0,    0,
    // row 6
    0xD1,0xDE, 0,  0xCF,0xD5,0xD4,0xC5,  0,     0,    0,    0,
    // row 5
     0,   0,  '[', 0xCB,0xC8,0xC6,0xD3,  0,     0,    0,    0,
    // row 4
     0,   0,  '>', 0xCD,0xC2,0xC3,0xDA,  0,     0,    0,    0,
    // row 3
    '"',  0,   0,   0,  '(', '&', '$',   0,     0,    0,    0,
    // row 2
     0,  ']', 0xCC,0xCA,0xC7,0xC4,0xC1,  0,     0,    0,    0,
    // row 1
     0,   0,  0xD0,0xC9,0xD9,0xD2,0xD7,  0,     0,    0,    0,
    // row 0
    '!',  0,   0,  ')', '\'','%', '#',   0,     0,    0,    0,
};

static const keyboard_decode_table_t c128_decode_tables[] = {
    { KEYMOD_NONE,  c128_unshifted_chars },
    { KEYMOD_SHIFT, c128_shifted_chars },
};

const keyboard_matrix_config_t c128_keyboard_config = {
    .model = KEYBOARD_MODEL_C64,  // Same scan method as C64 (CIA-based)
    .scan_chip = KEYBOARD_SCAN_CIA,
    .rows = C128_KEYBOARD_ROWS,
    .cols = C128_KEYBOARD_COLS,
    .description = "C128 11x8 keyboard matrix (CIA 1, C64-compatible + extended)",
    .keys = c128_keys,
    .num_decode_tables = sizeof(c128_decode_tables) / sizeof(c128_decode_tables[0]),
    .decode_tables = c128_decode_tables,
};
