#pragma once
/*
 * pet_keyboard_matrix.h — Commodore PET Keyboard Matrix Configuration
 *
 * Declares the keyboard_matrix_config_t for the PET's 10×8 matrix.
 * Scanned via PIA1: Port A selects rows (active-low), Port B reads columns.
 *
 * Covers the "graphics keyboard" (normal/business keyboard, 73 keys).
 * The original PET 2001 chiclet keyboard has the same matrix but a
 * different physical layout; the configuration data is the same.
 */

#include "chip/input/commodore_keyboard.hpp"

#define PET_KEYBOARD_ROWS 10
#define PET_KEYBOARD_COLS  8

extern const keyboard_matrix_config_t pet_keyboard_config;
