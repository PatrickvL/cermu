#include "systems/commodore/c128/c128_keyboard_matrix.hpp"

// ============================================================================
// C128 Keyboard Matrix — 11 columns × 8 rows
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

static const SDL_Keycode c128_keys[C128_KEYBOARD_ROWS * C128_KEYBOARD_COLS] = {
    // ── Columns 0–7: identical to C64.  Columns 8–10: C128 extended. ──
    //                 col 0                col 1                col 2              col 3      col 4      col 5      col 6          col 7             col 8                col 9                col 10
    // row 7 (PB7): RUN/STOP, /, ,, N, V, X, LSHIFT, CRSR↓,   KP1, KP3, NO SCROLL
    CERMU_KEY_CBM_RUN_STOP,  SDLK_SLASH,           SDLK_COMMA,       SDLK_n,  SDLK_v,  SDLK_x,  SDLK_LSHIFT,  SDLK_DOWN,      SDLK_KP_1,         SDLK_KP_3,         CERMU_KEY_CBM_NO_SCROLL,
    // row 6 (PB6): Q, ↑(char), @, O, U, T, E, F5,            KP7, KP9, →
    SDLK_q,    CERMU_KEY_CBM_ARROW_UP,    SDLK_LEFTBRACKET, SDLK_o,  SDLK_u,  SDLK_t,  SDLK_e,       SDLK_F5,        SDLK_KP_7,         SDLK_KP_9,         SDLK_RIGHT,
    // row 5 (PB5): C=, =, :, K, H, F, S, F3,                  KP4, KP6, ←
    CERMU_KEY_CBM_COMMODORE, SDLK_EQUALS,          SDLK_SEMICOLON,   SDLK_k,  SDLK_h,  SDLK_f,  SDLK_s,       SDLK_F3,        SDLK_KP_4,         SDLK_KP_6,         SDLK_LEFT,
    // row 4 (PB4): SPACE, RSHIFT, ., M, B, C, Z, F1,          KP2, KP ENTER, ↓
    SDLK_SPACE,SDLK_RSHIFT,          SDLK_PERIOD,      SDLK_m,  SDLK_b,  SDLK_c,  SDLK_z,       SDLK_F1,        SDLK_KP_2,         SDLK_KP_ENTER,     SDLK_DOWN,
    // row 3 (PB3): 2, HOME, -, 0, 8, 6, 4, F7,                TAB, LINE FEED, ↑
    SDLK_2,    SDLK_HOME,            SDLK_MINUS,       SDLK_0,  SDLK_8,  SDLK_6,  SDLK_4,       SDLK_F7,        SDLK_TAB,          CERMU_KEY_CBM_LINE_FEED,SDLK_UP,
    // row 2 (PB2): CTRL, ;, L, J, G, D, A, CRSR→,             KP5, KP−, KP.
    SDLK_LCTRL,SDLK_QUOTE,      SDLK_l,           SDLK_j,  SDLK_g,  SDLK_d,  SDLK_a,       SDLK_RIGHT,     SDLK_KP_5,         SDLK_KP_MINUS,     SDLK_KP_PERIOD,
    // row 1 (PB1): ←(char), *, P, I, Y, R, W, RETURN,         KP8, KP+, KP0
    CERMU_KEY_CBM_ARROW_LEFT, SDLK_RIGHTBRACKET, SDLK_p,    SDLK_i,  SDLK_y,  SDLK_r,  SDLK_w,       SDLK_RETURN,    SDLK_KP_8,         SDLK_KP_PLUS,      SDLK_KP_0,
    // row 0 (PB0): 1, £, +, 9, 7, 5, 3, DEL,                  HELP, ESC, ALT
    SDLK_1,    CERMU_KEY_CBM_POUND,       SDLK_BACKSLASH,   SDLK_9,  SDLK_7,  SDLK_5,  SDLK_3,       CERMU_KEY_CBM_DEL, CERMU_KEY_CBM_HELP,     SDLK_ESCAPE,       CERMU_KEY_CBM_ALT,
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
    .model = KEYBOARD_MODEL_C128,
    .scan_chip = KEYBOARD_SCAN_CIA,
    .rows = C128_KEYBOARD_ROWS,
    .cols = C128_KEYBOARD_COLS,
    .description = "C128 11x8 keyboard matrix (CIA 1, C64-compatible + extended)",
    .keys = c128_keys,
    .num_decode_tables = sizeof(c128_decode_tables) / sizeof(c128_decode_tables[0]),
    .decode_tables = c128_decode_tables,
};
