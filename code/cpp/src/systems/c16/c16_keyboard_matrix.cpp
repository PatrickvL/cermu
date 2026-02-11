#include "c16_keyboard_matrix.h"

// ============================================================================
// C16 / Plus/4 Keyboard Matrix — 8×8 (TED 7360)
// ============================================================================
// TED PIO2 ($FD30) selects which row(s) to scan (active-low output).
// TED register $FF08 reads the column result (active-low input).
// Joystick 1 and 2 signals are also mixed into the TED scan lines.
//
// Matrix layout verified against VICE emulator (data/PLUS4/gtk3_pos.vkm)
// and Commodore 264 Hardware Specification.
//
// Key differences from C64:
//   - Dedicated cursor keys: UP (5,3), DOWN (5,0), LEFT (6,0), RIGHT (6,3)
//   - ESC key at (6,4) — no equivalent on C64
//   - Both SHIFT keys wired to same position (1,7)
//   - Function keys: F1/F4 (0,4), F2/F5 (0,5), F3/F6 (0,6), HELP/F7 (0,3)
//   - No separate RESTORE key in matrix (wired directly to NMI like C64)

// Unshifted keys — using SDL keycodes and CbmKeys constants
//
//         Col 0           Col 1        Col 2     Col 3           Col 4     Col 5     Col 6     Col 7
//       +-----------+----------+----------+-----------+---------+---------+---------+-----------+
// Row 0 | INST/DEL  | RETURN   | POUND    | HELP/F7   | F1/F4   | F2/F5   | F3/F6   | @         |
// Row 1 | 3 #       | W        | A        | 4 $       | Z       | S       | E       | SHIFT     |
// Row 2 | 5 %       | R        | D        | 6 &       | C       | F       | T       | X         |
// Row 3 | 7 '       | Y        | G        | 8 (       | B       | H       | U       | V         |
// Row 4 | 9 )       | I        | J        | 0 ^       | M       | K       | O       | N         |
// Row 5 | DOWN      | P        | L        | UP        | . >     | : [     | -       | , <       |
// Row 6 | LEFT      | *        | ; ]      | RIGHT     | ESC     | =       | +       | / ?       |
// Row 7 | 1 !       | CLR/HOME | CTRL     | 2 "       | SPACE   | CBM     | Q       | RUN/STOP  |
//       +-----------+----------+----------+-----------+---------+---------+---------+-----------+
const uint32_t keyboard_matrix_unshifted_c16[C16_KEYBOARD_ROWS][C16_KEYBOARD_COLS] = {
    // Row 0: DEL, RETURN, POUND, HELP, F1, F2, F3, @
    {CbmKeys::DEL, CbmKeys::RETURN, CbmKeys::POUND, CbmKeys::F7, CbmKeys::F1, CbmKeys::F2, CbmKeys::F3, '@'},
    // Row 1: 3, W, A, 4, Z, S, E, SHIFT (both L/R share this position)
    {'3', 'W', 'A', '4', 'Z', 'S', 'E', CbmKeys::SHIFT_LEFT},
    // Row 2: 5, R, D, 6, C, F, T, X
    {'5', 'R', 'D', '6', 'C', 'F', 'T', 'X'},
    // Row 3: 7, Y, G, 8, B, H, U, V
    {'7', 'Y', 'G', '8', 'B', 'H', 'U', 'V'},
    // Row 4: 9, I, J, 0, M, K, O, N
    {'9', 'I', 'J', '0', 'M', 'K', 'O', 'N'},
    // Row 5: CRSR↓, P, L, CRSR↑, ., :, -, ,
    {CbmKeys::CURSOR_DOWN, 'P', 'L', CbmKeys::CURSOR_UP, '.', ':', '-', ','},
    // Row 6: CRSR←, *, ;, CRSR→, ESC, =, +, /
    {CbmKeys::CURSOR_LEFT, '*', ';', CbmKeys::CURSOR_RIGHT, CbmKeys::ESC, '=', '+', '/'},
    // Row 7: 1, HOME, CTRL, 2, SPACE, COMMODORE, Q, RUN/STOP
    {'1', CbmKeys::HOME, CbmKeys::CTRL, '2', CbmKeys::SPACE, CbmKeys::COMMODORE, 'Q', CbmKeys::RUN_STOP},
};

// Shifted keys — using SDL keycodes and CbmKeys constants
// SAME means the shifted version is handled by the KERNAL (same matrix position + SHIFT).
// Lowercase letters in shifted array allow host lowercase SDL keycodes to find
// their matrix position (SDL always sends SDLK_a='a' regardless of shift state).
// Function keys: shifted F-key variants (F4, F5, F6, HELP) auto-press SHIFT.
const uint32_t keyboard_matrix_shifted_c16[C16_KEYBOARD_ROWS][C16_KEYBOARD_COLS] = {
    // Row 0: INST(shift+DEL), SAME, SAME, HELP(shift+F7), F4(shift+F1), F5(shift+F2), F6(shift+F3), SAME
    {CbmKeys::INST, CbmKeys::SAME, CbmKeys::SAME, CbmKeys::HELP, CbmKeys::F4, CbmKeys::F5, CbmKeys::F6, CbmKeys::SAME},
    // Row 1: #, w, a, $, z, s, e, SAME (shift key itself)
    {'#', 'w', 'a', '$', 'z', 's', 'e', CbmKeys::SAME},
    // Row 2: %, r, d, &, c, f, t, x
    {'%', 'r', 'd', '&', 'c', 'f', 't', 'x'},
    // Row 3: ', y, g, (, b, h, u, v
    {'\'', 'y', 'g', '(', 'b', 'h', 'u', 'v'},
    // Row 4: ), i, j, SAME(^/↑ via KERNAL), m, k, o, n
    {')', 'i', 'j', CbmKeys::SAME, 'm', 'k', 'o', 'n'},
    // Row 5: SAME, p, l, SAME, >, [, SAME, <
    {CbmKeys::SAME, 'p', 'l', CbmKeys::SAME, '>', '[', CbmKeys::SAME, '<'},
    // Row 6: SAME, SAME, ], SAME, SAME, SAME(←/π via KERNAL), SAME, ?
    {CbmKeys::SAME, CbmKeys::SAME, ']', CbmKeys::SAME, CbmKeys::SAME, CbmKeys::SAME, CbmKeys::SAME, '?'},
    // Row 7: !, SAME(CLR via KERNAL), SAME, ", SAME, SAME, q, SAME
    {'!', CbmKeys::SAME, CbmKeys::SAME, '"', CbmKeys::SAME, CbmKeys::SAME, 'q', CbmKeys::SAME},
};

// Pre-built configuration for commodore_keyboard_create()
const keyboard_matrix_config_t c16_keyboard_config = {
    .model = KEYBOARD_MODEL_PLUS4_C16,
    .scan_chip = KEYBOARD_SCAN_TED,
    .rows = C16_KEYBOARD_ROWS,
    .cols = C16_KEYBOARD_COLS,
    .description = "C16/Plus4 8x8 keyboard matrix (TED 7360)",
    .unshifted = (const uint32_t*)keyboard_matrix_unshifted_c16,
    .shifted = (const uint32_t*)keyboard_matrix_shifted_c16,
};
