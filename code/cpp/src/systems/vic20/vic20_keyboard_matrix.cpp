#include "vic20_keyboard_matrix.h"

// ============================================================================
// VIC-20 Keyboard Matrix — 8×8 (EmuKey-based)
// ============================================================================
// VIA Port B ($9120) = column select (output)
// VIA Port A ($9121) = row read (input)
// Array convention: array[7-PB_col][7-PA_row]
//
// keys[] stores EmuKey values.  shifted_chars[] stores shifted output.
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

static const uint32_t vic20_shifted_chars[VIC20_KEYBOARD_ROWS * VIC20_KEYBOARD_COLS] = {
    // PB7: F8, SAME(CLR), SAME(-), SAME(0), (, &, $, "
    EMUKEY_F8, EMUKEY_SAME, EMUKEY_SAME, EMUKEY_SAME, '(', '&', '$', '"',
    // PB6: F6, π, SAME(@), o, u, t, e, q
    EMUKEY_F6, EMUKEY_CBM_PI, EMUKEY_SAME, 'o', 'u', 't', 'e', 'q',
    // PB5: F4, SAME(=), [, k, h, f, s, SAME(C=)
    EMUKEY_F4, EMUKEY_SAME, '[', 'k', 'h', 'f', 's', EMUKEY_SAME,
    // PB4: F2, SAME(RSHIFT), >, m, b, c, z, SAME(SPACE)
    EMUKEY_F2, EMUKEY_SAME, '>', 'm', 'b', 'c', 'z', EMUKEY_SAME,
    // PB3: SAME(CRSR↓→up), ?, <, n, v, x, SAME(LSHIFT), SAME(RUN/STOP)
    EMUKEY_SAME, '?', '<', 'n', 'v', 'x', EMUKEY_SAME, EMUKEY_SAME,
    // PB2: SAME(CRSR→→left), ], l, j, g, d, a, SAME(CTRL)
    EMUKEY_SAME, ']', 'l', 'j', 'g', 'd', 'a', EMUKEY_SAME,
    // PB1: SAME(RETURN), SAME(*), p, i, y, r, w, SAME(←)
    EMUKEY_SAME, EMUKEY_SAME, 'p', 'i', 'y', 'r', 'w', EMUKEY_SAME,
    // PB0: INST, SAME(£), SAME(+), ), ', %, #, !
    EMUKEY_INSERT, EMUKEY_SAME, EMUKEY_SAME, ')', '\'', '%', '#', '!',
};

const keyboard_matrix_config_t vic20_keyboard_config = {
    .model = KEYBOARD_MODEL_VIC20,
    .scan_chip = KEYBOARD_SCAN_VIA,
    .rows = VIC20_KEYBOARD_ROWS,
    .cols = VIC20_KEYBOARD_COLS,
    .description = "VIC-20 8x8 keyboard matrix",
    .keys = vic20_keys,
    .shifted_chars = vic20_shifted_chars,
};
