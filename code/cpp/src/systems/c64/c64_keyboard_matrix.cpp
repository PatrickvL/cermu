#include "c64_keyboard_matrix.h"

// ============================================================================
// C64 Keyboard Matrix — 8×8 (EmuKey-based)
// ============================================================================
// CIA Port A ($DC00) = column select (output)
// CIA Port B ($DC01) = row read (input)
// Array convention: array[7-PB_bit][7-PA_bit]
//
// The keys[] table stores EmuKey values identifying each physical key.
// The shifted_chars[] table stores the ASCII character produced when
// the key is pressed with SHIFT, or EMUKEY_SAME if the KERNAL handles it.
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

// Shifted character output — ASCII characters or markers
static const uint32_t c64_shifted_chars[C64_KEYBOARD_ROWS * C64_KEYBOARD_COLS] = {
    // row 7: SAME, ?, <, n, v, x, SAME, SAME(cursor up via auto-shift)
    EMUKEY_SAME, '?', '<', 'n', 'v', 'x', EMUKEY_SAME, EMUKEY_SAME,
    // row 6: q, π, SAME(@), o, u, t, e, F6
    'q', EMUKEY_CBM_PI, EMUKEY_SAME, 'o', 'u', 't', 'e', EMUKEY_F6,
    // row 5: SAME(C=), SAME(=), [, k, h, f, s, F4
    EMUKEY_SAME, EMUKEY_SAME, '[', 'k', 'h', 'f', 's', EMUKEY_F4,
    // row 4: SAME(SPACE), SAME(RSHIFT), >, m, b, c, z, F2
    EMUKEY_SAME, EMUKEY_SAME, '>', 'm', 'b', 'c', 'z', EMUKEY_F2,
    // row 3: ", SAME(CLR), SAME(-), SAME(0), (, &, $, F8
    '"', EMUKEY_SAME, EMUKEY_SAME, EMUKEY_SAME, '(', '&', '$', EMUKEY_F8,
    // row 2: SAME(CTRL), ], l, j, g, d, a, SAME(cursor left via auto-shift)
    EMUKEY_SAME, ']', 'l', 'j', 'g', 'd', 'a', EMUKEY_SAME,
    // row 1: SAME(←), SAME(*), p, i, y, r, w, SAME(RETURN)
    EMUKEY_SAME, EMUKEY_SAME, 'p', 'i', 'y', 'r', 'w', EMUKEY_SAME,
    // row 0: !, SAME(£), SAME(+), ), ', %, #, INST
    '!', EMUKEY_SAME, EMUKEY_SAME, ')', '\'', '%', '#', EMUKEY_INSERT,
};

const keyboard_matrix_config_t c64_keyboard_config = {
    .model = KEYBOARD_MODEL_C64,
    .scan_chip = KEYBOARD_SCAN_CIA,
    .rows = C64_KEYBOARD_ROWS,
    .cols = C64_KEYBOARD_COLS,
    .description = "C64 8x8 keyboard matrix",
    .keys = c64_keys,
    .shifted_chars = c64_shifted_chars,
};
