#pragma once

#include "chip/input/commodore_keyboard.hpp"

// VIC-20 Keyboard Matrix — 8×8
// VIA Port B ($9120) = column select (output)
// VIA Port A ($9121) = row read (input)
//
// Array convention: array[7 - row_bit][7 - col_bit]
//   row_bit = VIA Port B bit (column select port on VIC-20 hardware)
//   col_bit = VIA Port A bit (row read port on VIC-20 hardware)
//   Same bit-reversed layout as all other Commodore matrices.
//   NOTE: VIC-20 Port B/A roles are swapped vs C64 — the array
//   labels "row"/"col" refer to the code convention (first/second
//   dimension), not the hardware port semantics.

#define VIC20_KEYBOARD_ROWS 8
#define VIC20_KEYBOARD_COLS 8

// Pre-built configuration struct for commodore_keyboard_t::init()
// Contains keys[] (SDL_Keycode values) and decode tables.
extern const keyboard_matrix_config_t vic20_keyboard_config;

