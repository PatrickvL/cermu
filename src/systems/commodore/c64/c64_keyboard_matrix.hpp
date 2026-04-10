#pragma once

#include "chip/input/commodore_keyboard.hpp"

// C64 Keyboard Matrix — 8×8
// CIA Port A ($DC00) = column select (output)
// CIA Port B ($DC01) = row read (input)
//
// Array convention: array[7 - row_bit][7 - col_bit]
//   row_bit = CIA Port B bit (row read), col_bit = CIA Port A bit (column select)
//   Row index 0 = PB7, Row index 7 = PB0
//   Col index 0 = PA7, Col index 7 = PA0
//   Same bit-reversed layout as all other Commodore matrices.

#define C64_KEYBOARD_ROWS 8
#define C64_KEYBOARD_COLS 8

// Pre-built configuration struct for commodore_keyboard_t::init()
// Contains keys[] (SDL_Keycode values) and decode tables.
extern const keyboard_matrix_config_t c64_keyboard_config;

