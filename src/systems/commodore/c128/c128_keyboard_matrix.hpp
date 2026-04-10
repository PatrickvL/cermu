#pragma once

#include "chip/input/commodore_keyboard.hpp"

// C128 Keyboard Matrix — 11 columns × 8 rows
// CIA Port A ($DC00) = column select (output, active-low)
// CIA Port B ($DC01) = row read (input, active-low)
//
// Columns 0–7 are identical to the C64 matrix.
// Columns 8–10 add C128-specific keys and the numeric keypad.
//
// Array convention: array[7 - row_bit][col]
//   row_bit = CIA Port B bit (row read)
//   Cols 0–7: col index = 7 - PA_bit (bit-reversed, same as C64)
//   Cols 8–10: col index = hardware column (direct, no reversal)
//   Same bit-reversed layout as all other Commodore matrices.
//
// CAPS LOCK is handled via shift-lock flag on LSHIFT (row 1, col 7).
// 40/80 DISPLAY is a hardware key read directly by MMU MCR bit 5, not in the matrix.

#define C128_KEYBOARD_ROWS 8
#define C128_KEYBOARD_COLS 11

// Pre-built configuration struct for commodore_keyboard_t::init()
extern const keyboard_matrix_config_t c128_keyboard_config;
