#pragma once

#include "commodore_keyboard.h"

// C64 Keyboard Matrix — 8×8
// CIA Port A ($DC00) = column select (output)
// CIA Port B ($DC01) = row read (input)
// Array convention: array[7-PB_bit][7-PA_bit]
//   Row index 0 = PB7, Row index 7 = PB0
//   Col index 0 = PA7, Col index 7 = PA0

#define C64_KEYBOARD_ROWS 8
#define C64_KEYBOARD_COLS 8

// Pre-built configuration struct for commodore_keyboard_create()
// Contains keys[] (EmuKey values) and decode tables.
extern const keyboard_matrix_config_t c64_keyboard_config;

