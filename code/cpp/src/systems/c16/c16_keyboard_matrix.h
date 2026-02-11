#ifndef C16_KEYBOARD_MATRIX_H
#define C16_KEYBOARD_MATRIX_H

#include "../../chip/input/commodore_keyboard.h"

// C16 / Plus/4 Keyboard Matrix — 8×8
// TED 7360 integrated keyboard scanning:
//   PIO2 ($FD30) = row select (output) — active-low row drive
//   TED  ($FF08) = column read (input)  — active-low column sense
// Array convention: array[row][col]
//   Row 0 = bit 0 of row select, Row 7 = bit 7 of row select
//   Col 0 = bit 0 of column sense, Col 7 = bit 7 of column sense
//
// Key differences from C64:
//   - Dedicated cursor keys (UP, DOWN, LEFT, RIGHT) — no SHIFT required
//   - ESC key present in matrix
//   - Both SHIFTs wired to same matrix position (row 1, col 7)
//   - Function keys: F1/F4, F2/F5, F3/F6, HELP/F7

#define C16_KEYBOARD_ROWS 8
#define C16_KEYBOARD_COLS 8

extern const uint32_t keyboard_matrix_unshifted_c16[C16_KEYBOARD_ROWS][C16_KEYBOARD_COLS];
extern const uint32_t keyboard_matrix_shifted_c16[C16_KEYBOARD_ROWS][C16_KEYBOARD_COLS];

// Pre-built configuration struct for commodore_keyboard_create()
extern const keyboard_matrix_config_t c16_keyboard_config;

#endif // C16_KEYBOARD_MATRIX_H
