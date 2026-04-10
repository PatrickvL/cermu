#pragma once
/*
 * pet_keyboard_matrix.h — Commodore PET Keyboard Matrix Configuration
 *
 * Declares the keyboard_matrix_config_t for the PET's 10×8 matrix.
 * Scanned via PIA1: Port A selects rows (active-low decoded), Port B reads columns.
 *
 * Array convention: array[9 - hw_row][7 - PB_bit]
 *   Row index 0 = hw row 9, Row index 9 = hw row 0
 *   Col index 0 = PB7,      Col index 7 = PB0
 *   Same bit-reversed layout as all other Commodore matrices.
 *
 * Covers the "graphics keyboard" (normal/business keyboard, 73 keys).
 * The original PET 2001 chiclet keyboard has the same matrix but a
 * different physical layout; the configuration data is the same.
 */

#include "chip/input/commodore_keyboard.hpp"

#define PET_KEYBOARD_ROWS 10
#define PET_KEYBOARD_COLS  8

extern const keyboard_matrix_config_t pet_keyboard_config;
