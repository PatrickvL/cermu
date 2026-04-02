#pragma once

#include "chip/input/commodore_keyboard.hpp"

// C128 Keyboard Matrix — 11 columns × 8 rows
// CIA Port A ($DC00) = column select (output, active-low)
// CIA Port B ($DC01) = row read (input, active-low)
//
// Columns 0–7 are identical to the C64 matrix.
// Columns 8–10 add C128-specific keys and the numeric keypad.
// Array convention: array[row][col] where row 0 = PB7, col 0 = PA7
//
// CAPS LOCK is handled via shift-lock flag on LSHIFT (row 1, col 7).
// 40/80 DISPLAY is a hardware key read directly by MMU MCR bit 5, not in the matrix.

#define C128_KEYBOARD_ROWS 8
#define C128_KEYBOARD_COLS 11

// Pre-built configuration struct for commodore_keyboard_t::init()
extern const keyboard_matrix_config_t c128_keyboard_config;
