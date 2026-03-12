#include "systems/commodore/c16/c16_keyboard_matrix.hpp"

// ============================================================================
// C16 / Plus/4 Keyboard Matrix — 8×8 (EmuKey-based)
// ============================================================================
// TED PIO2 ($FD30) selects which row(s) to scan (active-low output).
// TED register $FF08 reads the column result (active-low input).
//
// Array convention: array[7 - row_bit][7 - col_bit]
// Same reversed-bit convention as the C64 CIA matrix.  The shared
// key_down/key_up bit-reversal maps array indices back to the correct
// hardware bit positions so that row_open_contacts[hw_row] contains
// column bitmask data suitable for direct use by the scan callback.
//
// Hardware matrix (VICE Plus4 gtk3_pos.vkm reference):
//       col 0       col 1      col 2      col 3    col 4    col 5    col 6     col 7
// row 0: INST/DEL   RETURN     POUND      HELP/F7  F1       F2       F3        @
// row 1: 3          W          A          4        Z        S        E         SHIFTs
// row 2: 5          R          D          6        C        F        T         X
// row 3: 7          Y          G          8        B        H        U         V
// row 4: 9          I          J          0        M        K        O         N
// row 5: CRSR↓      P          L          CRSR↑    .        :        -         ,
// row 6: CRSR←      *          ;          CRSR→    ESC      =        +         /
// row 7: 1          HOME       CTRL       2        SPACE    C=       Q         RUN/STOP
//
// Key differences from C64:
//   - Dedicated cursor keys (UP, DOWN, LEFT, RIGHT — no SHIFT required)
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
    // array[0] = row 7 reversed: RUN/STOP, Q, C=, SPACE, 2, CTRL, HOME, 1
    EMUKEY_TAB,  EMUKEY_Q,  EMUKEY_LGUI,  EMUKEY_SPACE,  EMUKEY_2,  EMUKEY_LCTRL,  EMUKEY_HOME,  EMUKEY_1,
    // array[1] = row 6 reversed: /, +, =, ESC, CRSR→, ;, *, CRSR←
    EMUKEY_SLASH,  EMUKEY_BACKSLASH,  EMUKEY_EQUALS,  EMUKEY_ESCAPE,  EMUKEY_RIGHT,  EMUKEY_APOSTROPHE,  EMUKEY_RIGHTBRACKET,  EMUKEY_LEFT,
    // array[2] = row 5 reversed: ,, -, :, ., CRSR↑, L, P, CRSR↓
    EMUKEY_COMMA,  EMUKEY_MINUS,  EMUKEY_SEMICOLON,  EMUKEY_PERIOD,  EMUKEY_UP,  EMUKEY_L,  EMUKEY_P,  EMUKEY_DOWN,
    // array[3] = row 4 reversed: N, O, K, M, 0, J, I, 9
    EMUKEY_N,  EMUKEY_O,  EMUKEY_K,  EMUKEY_M,  EMUKEY_0,  EMUKEY_J,  EMUKEY_I,  EMUKEY_9,
    // array[4] = row 3 reversed: V, U, H, B, 8, G, Y, 7
    EMUKEY_V,  EMUKEY_U,  EMUKEY_H,  EMUKEY_B,  EMUKEY_8,  EMUKEY_G,  EMUKEY_Y,  EMUKEY_7,
    // array[5] = row 2 reversed: X, T, F, C, 6, D, R, 5
    EMUKEY_X,  EMUKEY_T,  EMUKEY_F,  EMUKEY_C,  EMUKEY_6,  EMUKEY_D,  EMUKEY_R,  EMUKEY_5,
    // array[6] = row 1 reversed: SHIFT, E, S, Z, 4, A, W, 3
    EMUKEY_LSHIFT,  EMUKEY_E,  EMUKEY_S,  EMUKEY_Z,  EMUKEY_4,  EMUKEY_A,  EMUKEY_W,  EMUKEY_3,
    // array[7] = row 0 reversed: @, F3, F2, F1, HELP(F7), POUND, RETURN, DEL
    EMUKEY_LEFTBRACKET,  EMUKEY_F3,  EMUKEY_F2,  EMUKEY_F1,  EMUKEY_F7,  EMUKEY_CBM_POUND,  EMUKEY_RETURN,  EMUKEY_BACKSPACE,
};

// Unshifted character decode table — PETSCII codes per matrix position.
// Same reversed-bit convention as c16_keys[] above.
// 0 = non-character key (modifier, function key, cursor key, RETURN, DEL).
// C16/Plus4: £ = $5C.  No ← or ↑ dedicated keys in the matrix.
static const petscii_t c16_unshifted_chars[C16_KEYBOARD_ROWS * C16_KEYBOARD_COLS] = {
    // array[0] = row 7 reversed: (RUN/STOP), Q, (C=), SPACE, 2, (CTRL), (HOME), 1
    0, 'Q', 0, ' ', '2', 0, 0, '1',
    // array[1] = row 6 reversed: /, +, =, (ESC), (CRSR→), ;, *, (CRSR←)
    '/', '+', '=', 0, 0, ';', '*', 0,
    // array[2] = row 5 reversed: ,, -, :, ., (CRSR↑), L, P, (CRSR↓)
    ',', '-', ':', '.', 0, 'L', 'P', 0,
    // array[3] = row 4 reversed: N, O, K, M, 0, J, I, 9
    'N', 'O', 'K', 'M', '0', 'J', 'I', '9',
    // array[4] = row 3 reversed: V, U, H, B, 8, G, Y, 7
    'V', 'U', 'H', 'B', '8', 'G', 'Y', '7',
    // array[5] = row 2 reversed: X, T, F, C, 6, D, R, 5
    'X', 'T', 'F', 'C', '6', 'D', 'R', '5',
    // array[6] = row 1 reversed: (SHIFT), E, S, Z, 4, A, W, 3
    0, 'E', 'S', 'Z', '4', 'A', 'W', '3',
    // array[7] = row 0 reversed: @, (F3), (F2), (F1), (HELP/F7), £($5C), (RETURN), (DEL)
    '@', 0, 0, 0, 0, 0x5C, 0, 0,
};

// Shifted character decode table — PETSCII codes per matrix position.
// Same reversed-bit convention as c16_keys[] above.
// Letters use PETSCII lowercase ($C1–$DA) for character-accurate mapping.
// 0 = no distinct character (modifier key, function key, cursor key,
//     or same character as unshifted — handled by KERNAL/TED at runtime).
static const petscii_t c16_shifted_chars[C16_KEYBOARD_ROWS * C16_KEYBOARD_COLS] = {
    // array[0] = row 7 reversed: (RUN/STOP), q($D1), (C=), (SPACE), ", (CTRL), (CLR), !
    0, 0xD1, 0, 0, '"', 0, 0, '!',
    // array[1] = row 6 reversed: ?, (+), (=), (ESC), (CRSR→), ], (*), (CRSR←)
    '?', 0, 0, 0, 0, ']', 0, 0,
    // array[2] = row 5 reversed: <, (-), [, >, (CRSR↑), l($CC), p($D0), (CRSR↓)
    '<', 0, '[', '>', 0, 0xCC, 0xD0, 0,
    // array[3] = row 4 reversed: n($CE), o($CF), k($CB), m($CD), (0), j($CA), i($C9), )
    0xCE, 0xCF, 0xCB, 0xCD, 0, 0xCA, 0xC9, ')',
    // array[4] = row 3 reversed: v($D6), u($D5), h($C8), b($C2), (, g($C7), y($D9), '
    0xD6, 0xD5, 0xC8, 0xC2, '(', 0xC7, 0xD9, '\'',
    // array[5] = row 2 reversed: x($D8), t($D4), f($C6), c($C3), &, d($C4), r($D2), %
    0xD8, 0xD4, 0xC6, 0xC3, '&', 0xC4, 0xD2, '%',
    // array[6] = row 1 reversed: (SHIFT), e($C5), s($D3), z($DA), $, a($C1), w($D7), #
    0, 0xC5, 0xD3, 0xDA, '$', 0xC1, 0xD7, '#',
    // array[7] = row 0 reversed: (@), (F6), (F5), (F4), (HELP), (£), (RETURN), (INST)
    0, 0, 0, 0, 0, 0, 0, 0,
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
