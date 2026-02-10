#include "commodore_keyboard.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Keyboard matrix definition (C64 layout)
// Unshifted keys - using SDL keycodes
// Array convention: array[7-PB_bit][7-PA_bit]
//   Row index 0 = PB7, Row index 7 = PB0
//   Col index 0 = PA7, Col index 7 = PA0
// Note: CRSR→/← key position stores CURSOR_RIGHT (unshifted function)
//       CRSR↓/↑ key position stores CURSOR_DOWN (unshifted function)
//       Host LEFT/UP arrows are handled via auto-shift in key_down/key_up.
const uint32_t keyboard_matrix_unshifted[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {CbmKeys::RUN_STOP, '/', ',', 'N', 'V', 'X', CbmKeys::SHIFT_LEFT, CbmKeys::CURSOR_DOWN}, // row 7 (CRSR↓ key)
    {'Q', CbmKeys::ARROW_UP, '@', 'O', 'U', 'T', 'E', CbmKeys::F5}, // row 6 (↑ char)
    {CbmKeys::COMMODORE, '=', ':', 'K', 'H', 'F', 'S', CbmKeys::F3}, // row 5
    {CbmKeys::SPACE, CbmKeys::SHIFT_RIGHT, '.', 'M', 'B', 'C', 'Z', CbmKeys::F1}, // row 4
    {'2', CbmKeys::HOME, '-', '0', '8', '6', '4', CbmKeys::F7}, // row 3
    {CbmKeys::CTRL, ';', 'L', 'J', 'G', 'D', 'A', CbmKeys::CURSOR_RIGHT}, // row 2 (CRSR→ key)
    {CbmKeys::ARROW_LEFT, '*', 'P', 'I', 'Y', 'R', 'W', CbmKeys::RETURN}, // row 1 (← char)
    {'1', CbmKeys::POUND, '+', '9', '7', '5', '3', CbmKeys::DEL}, // row 0
};

// Shifted keys (C64 layout) - using SDL keycodes
// Note: Cursor/HOME shifted functions are handled by KERNAL when SHIFT is held.
//       SAME is used for cursor positions since auto-shift handles host LEFT/UP.
const uint32_t keyboard_matrix_shifted[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {CbmKeys::SAME, '?', '<', 'n', 'v', 'x', CbmKeys::SAME, CbmKeys::SAME}, // row 7 (CRSR↓ shifted=cursor up, handled by auto-shift)
    {'q', CbmKeys::PI, CbmKeys::SAME, 'o', 'u', 't', 'e', CbmKeys::F6}, // row 6
    {CbmKeys::SAME, CbmKeys::SAME, '[', 'k', 'h', 'f', 's', CbmKeys::F4}, // row 5
    {CbmKeys::SAME, CbmKeys::SAME, '>', 'm', 'b', 'c', 'z', CbmKeys::F2}, // row 4
    {'"', CbmKeys::SAME, CbmKeys::SAME, CbmKeys::SAME, '(', '&', '$', CbmKeys::F8}, // row 3 (HOME shifted=CLR, KERNAL handles it)
    {CbmKeys::SAME, ']', 'l', 'j', 'g', 'd', 'a', CbmKeys::SAME}, // row 2 (CRSR→ shifted=cursor left, handled by auto-shift)
    {CbmKeys::SAME, CbmKeys::SAME, 'p', 'i', 'y', 'r', 'w', CbmKeys::SAME}, // row 1
    {'!', CbmKeys::SAME, CbmKeys::SAME, ')', '\'', '%', '#', CbmKeys::INST}, // row 0
};

// ============================================================================
// VIC-20 Keyboard Matrix
// ============================================================================
// VIC-20 has different matrix wiring than C64:
//   VIA Port B ($9120) = column select (output)
//   VIA Port A ($9121) = row read (input)
// Array convention: array[7-PB_col][7-PA_row]
// Transform from C64: swap array rows 0↔4, swap columns 0↔7 within each row.

// Unshifted keys (VIC-20 layout)
const uint32_t keyboard_matrix_unshifted_vic20[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {CbmKeys::F7, CbmKeys::HOME, '-', '0', '8', '6', '4', '2'},             // PB7
    {CbmKeys::F5, CbmKeys::ARROW_UP, '@', 'O', 'U', 'T', 'E', 'Q'},        // PB6 (↑ char)
    {CbmKeys::F3, '=', ':', 'K', 'H', 'F', 'S', CbmKeys::COMMODORE},       // PB5
    {CbmKeys::F1, CbmKeys::SHIFT_RIGHT, '.', 'M', 'B', 'C', 'Z', CbmKeys::SPACE}, // PB4
    {CbmKeys::CURSOR_DOWN, '/', ',', 'N', 'V', 'X', CbmKeys::SHIFT_LEFT, CbmKeys::RUN_STOP}, // PB3 (CRSR↓ key)
    {CbmKeys::CURSOR_RIGHT, ';', 'L', 'J', 'G', 'D', 'A', CbmKeys::CTRL},  // PB2 (CRSR→ key)
    {CbmKeys::RETURN, '*', 'P', 'I', 'Y', 'R', 'W', CbmKeys::ARROW_LEFT},  // PB1 (← char)
    {CbmKeys::DEL, CbmKeys::POUND, '+', '9', '7', '5', '3', '1'},           // PB0
};

// Shifted keys (VIC-20 layout)
const uint32_t keyboard_matrix_shifted_vic20[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {CbmKeys::F8, CbmKeys::SAME, CbmKeys::SAME, CbmKeys::SAME, '(', '&', '$', '"'},  // PB7 (HOME shifted=CLR, KERNAL handles it)
    {CbmKeys::F6, CbmKeys::PI, CbmKeys::SAME, 'o', 'u', 't', 'e', 'q'},             // PB6
    {CbmKeys::F4, CbmKeys::SAME, '[', 'k', 'h', 'f', 's', CbmKeys::SAME},           // PB5
    {CbmKeys::F2, CbmKeys::SAME, '>', 'm', 'b', 'c', 'z', CbmKeys::SAME},           // PB4
    {CbmKeys::SAME, '?', '<', 'n', 'v', 'x', CbmKeys::SAME, CbmKeys::SAME},         // PB3 (CRSR↓ shifted=cursor up, handled by auto-shift)
    {CbmKeys::SAME, ']', 'l', 'j', 'g', 'd', 'a', CbmKeys::SAME},                   // PB2 (CRSR→ shifted=cursor left, handled by auto-shift)
    {CbmKeys::SAME, CbmKeys::SAME, 'p', 'i', 'y', 'r', 'w', CbmKeys::SAME},         // PB1
    {CbmKeys::INST, CbmKeys::SAME, CbmKeys::SAME, ')', '\'', '%', '#', '!'},         // PB0
};

// Special key mappings - SDL keycode compatible
static const uint32_t special_key_mapping[][2] = {
    {CbmKeys::DEL, CbmKeys::DEL},  // DEL
    {CbmKeys::HOME, CbmKeys::HOME},  // HOME
    {CbmKeys::RUN_STOP, CbmKeys::RUN_STOP},  // RUN/STOP
    {CbmKeys::CURSOR_DOWN, CbmKeys::CURSOR_DOWN},  // CURSOR DOWN
    {CbmKeys::CURSOR_RIGHT, CbmKeys::CURSOR_RIGHT},  // CURSOR RIGHT
    {CbmKeys::SHIFT_RIGHT, CbmKeys::SHIFT_RIGHT},  // SHIFT RIGHT
    {CbmKeys::F1, CbmKeys::F1},  // F1
    {CbmKeys::F3, CbmKeys::F3},  // F3
    {CbmKeys::F5, CbmKeys::F5},  // F5
    {CbmKeys::F7, CbmKeys::F7},  // F7
    {CbmKeys::F2, CbmKeys::F2},  // F2
    {CbmKeys::F4, CbmKeys::F4},  // F4
    {CbmKeys::F6, CbmKeys::F6},  // F6
    {CbmKeys::F8, CbmKeys::F8},  // F8
    {CbmKeys::CTRL, CbmKeys::CTRL},  // CTRL
    {CbmKeys::RETURN, CbmKeys::RETURN},  // RETURN
    {CbmKeys::SPACE, CbmKeys::SPACE},  // SPACE
    {CbmKeys::SHIFT_LEFT, CbmKeys::SHIFT_LEFT},  // SHIFT LEFT
    {CbmKeys::COMMODORE, CbmKeys::COMMODORE},  // COMMODORE
    {CbmKeys::RESTORE, CbmKeys::RESTORE},  // RESTORE
    {0, 0}         // Terminator
};

commodore_keyboard_t* commodore_keyboard_create() {
    commodore_keyboard_t* keyboard = (commodore_keyboard_t*)malloc(sizeof(commodore_keyboard_t));
    if (!keyboard) return NULL;

    // Initialize chip descriptor
    keyboard->descriptor = (chip_descriptor_t){
        .description = "C64/VIC-20 Keyboard Matrix Emulation",
        .create = NULL,
        .destroy = NULL,
        .bus_attach = NULL,
        .bank_change = NULL
    };

    // Default to C64 matrix (must be set before reset builds lookup table)
    keyboard->active_unshifted = keyboard_matrix_unshifted;
    keyboard->active_shifted = keyboard_matrix_shifted;

    // Initialize keyboard state
    commodore_keyboard_reset(keyboard);

    return keyboard;
}

void commodore_keyboard_destroy(commodore_keyboard_t* keyboard) {
    if (keyboard) {
        free(keyboard);
    }
}

void commodore_keyboard_reset(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;

    // Initialize all contacts as open (no keys pressed)
    for (int col = 0; col < KEYBOARD_COLS; col++) {
        keyboard->row_open_contacts[col] = 0xFF; // All rows open
    }

    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        keyboard->col_open_contacts[row] = 0xFF; // All columns open
    }

    // Preserve active matrix selection (set by create or set_vic20_mode)
    // If somehow unset, default to C64
    if (!keyboard->active_unshifted) {
        keyboard->active_unshifted = keyboard_matrix_unshifted;
        keyboard->active_shifted = keyboard_matrix_shifted;
    }

    // Initialize matrix lookup using active matrix
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            keyboard->key_matrix[row][col] = (key_matrix_info_t){
                .row = (uint8_t)row,
                .col = (uint8_t)col,
                .shifted = false,
                .key_code = keyboard->active_unshifted[row][col]
            };
        }
    }

    // Initialize key lookup table
    for (int i = 0; i < MAX_KEY_LOOKUP; i++) {
        keyboard->key_lookup[i] = (key_matrix_info_t){
            .row = 0,
            .col = 0,
            .shifted = false,
            .key_code = 0
        };
    }

    // Build key lookup table from active matrix
    // Note: SDL keycodes can be very large (e.g., SDLK_F1 = 0x4000003A)
    // so we only add keys within bounds and use linear search for the rest
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            uint32_t unshifted_key = keyboard->active_unshifted[row][col];
            uint32_t shifted_key = keyboard->active_shifted[row][col];

            // Only add to lookup table if within bounds (ASCII range)
            if (unshifted_key != CbmKeys::SAME && unshifted_key != 0 && unshifted_key < MAX_KEY_LOOKUP) {
                keyboard->key_lookup[unshifted_key] = (key_matrix_info_t){
                    .row = (uint8_t)row,
                    .col = (uint8_t)col,
                    .shifted = false,
                    .key_code = unshifted_key
                };
            }

            if (shifted_key != CbmKeys::SAME && shifted_key != 0 && shifted_key != unshifted_key && shifted_key < MAX_KEY_LOOKUP) {
                keyboard->key_lookup[shifted_key] = (key_matrix_info_t){
                    .row = (uint8_t)row,
                    .col = (uint8_t)col,
                    .shifted = true,
                    .key_code = shifted_key
                };
            }
        }
    }

    // Initialize special state
    keyboard->restore_key_pressed = false;
    keyboard->caps_lock_active = false;
    keyboard->auto_shift_left_active = false;
    keyboard->auto_shift_up_active = false;
    keyboard->cia_port_a_reference = NULL;
    keyboard->cia_port_b_reference = NULL;
}

void commodore_keyboard_set_vic20_mode(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;

    // Switch to VIC-20 keyboard matrix
    // VIC-20 has different matrix wiring than C64
    keyboard->active_unshifted = keyboard_matrix_unshifted_vic20;
    keyboard->active_shifted = keyboard_matrix_shifted_vic20;

    // Rebuild the lookup table and key_matrix with the VIC-20 matrix
    commodore_keyboard_reset(keyboard);

    printf("Keyboard: Switched to VIC-20 matrix mode\n");
}

uint32_t commodore_keyboard_map_host_key(uint32_t host_key, bool shifted) {
    // Map host keyboard keys to Commodore keys
    // This is a simplified mapping - would need to be expanded for full functionality
    for (int i = 0; special_key_mapping[i][0] != 0; i++) {
        if (host_key == special_key_mapping[i][0]) {
            return special_key_mapping[i][1];
        }
    }
    return host_key; // Regular character
}

bool commodore_keyboard_is_special_key(uint32_t key_code) {
    // Check if key is a special function key
    for (int i = 0; special_key_mapping[i][0] != 0; i++) {
        if (key_code == special_key_mapping[i][0]) {
            return true;
        }
    }
    return false;
}

// Helper: find key position by linear search through the active keyboard matrix
// Used for keys with SDL keycodes >= MAX_KEY_LOOKUP (special keys like Shift, F-keys, cursors)
static bool find_key_in_matrix(const uint32_t (*unshifted)[KEYBOARD_COLS],
                               const uint32_t (*shifted)[KEYBOARD_COLS],
                               uint32_t key_code, uint8_t* out_row, uint8_t* out_col) {
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            if (unshifted[row][col] == key_code ||
                shifted[row][col] == key_code) {
                *out_row = (uint8_t)row;
                *out_col = (uint8_t)col;
                return true;
            }
        }
    }
    return false;
}

void commodore_keyboard_key_down(commodore_keyboard_t* keyboard, uint32_t key_code, bool shifted) {
    if (!keyboard) return;

    // Handle RESTORE key (special case - connects to NMI)
    if (key_code == CbmKeys::RESTORE) {
        keyboard->restore_key_pressed = true;
        return;
    }

    // Handle cursor LEFT: on real hardware this is SHIFT + CRSR→
    // Auto-press SHIFT and redirect to the CRSR→ key (SDLK_RIGHT)
    if (key_code == SDLK_LEFT) {
        keyboard->auto_shift_left_active = true;
        commodore_keyboard_key_down(keyboard, CbmKeys::SHIFT_LEFT, false);
        commodore_keyboard_key_down(keyboard, SDLK_RIGHT, false);
        return;
    }

    // Handle cursor UP: on real hardware this is SHIFT + CRSR↓
    // Auto-press SHIFT and redirect to the CRSR↓ key (SDLK_DOWN)
    if (key_code == SDLK_UP) {
        keyboard->auto_shift_up_active = true;
        commodore_keyboard_key_down(keyboard, CbmKeys::SHIFT_LEFT, false);
        commodore_keyboard_key_down(keyboard, SDLK_DOWN, false);
        return;
    }

    // Map host key to Commodore key
    uint32_t commodore_key = commodore_keyboard_map_host_key(key_code, shifted);

    // Find the key position - use lookup table for ASCII keys, linear search for extended SDL keycodes
    uint8_t row, col;
    bool found = false;

    if (commodore_key < MAX_KEY_LOOKUP) {
        key_matrix_info_t* key_info = &keyboard->key_lookup[commodore_key];
        if (key_info->key_code != 0) {
            row = key_info->row;
            col = key_info->col;
            found = true;
        }
    }

    if (!found) {
        found = find_key_in_matrix(keyboard->active_unshifted, keyboard->active_shifted,
                                   commodore_key, &row, &col);
    }

    if (found) {
        // Convert from array indices to hardware CIA/VIA bit numbers.
        // C64 matrix: array[7-PB_bit][7-PA_bit], so:
        //   pa_bit = 7 - row  (actually the PB/row-read bit for C64)
        //   pb_bit = 7 - col  (actually the PA/column-select bit for C64)
        // VIC-20 matrix: array[7-PB_col][7-PA_row] — same variable mapping.
        // The naming is kept for compatibility; the CIA/VIA callbacks use
        // the correct contacts array (col_open_contacts for forward scan,
        // row_open_contacts for reverse scan).
        uint8_t pa_bit = 7 - row;
        uint8_t pb_bit = 7 - col;

        // Close the contact (key pressed)
        // row_open_contacts[pa_bit] stores pb_bit flags
        // col_open_contacts[pb_bit] stores pa_bit flags (transpose)
        keyboard->row_open_contacts[pa_bit] &= ~(1 << pb_bit);
        keyboard->col_open_contacts[pb_bit] &= ~(1 << pa_bit);
    }
}

void commodore_keyboard_key_up(commodore_keyboard_t* keyboard, uint32_t key_code, bool shifted) {
    if (!keyboard) return;

    // Handle RESTORE key
    if (key_code == CbmKeys::RESTORE) {
        keyboard->restore_key_pressed = false;
        return;
    }

    // Handle cursor LEFT release: release CRSR→ and auto-release SHIFT
    if (key_code == SDLK_LEFT) {
        commodore_keyboard_key_up(keyboard, SDLK_RIGHT, false);
        if (keyboard->auto_shift_left_active) {
            keyboard->auto_shift_left_active = false;
            // Only auto-release SHIFT if cursor-up isn't also holding it
            if (!keyboard->auto_shift_up_active) {
                commodore_keyboard_key_up(keyboard, CbmKeys::SHIFT_LEFT, false);
            }
        }
        return;
    }

    // Handle cursor UP release: release CRSR↓ and auto-release SHIFT
    if (key_code == SDLK_UP) {
        commodore_keyboard_key_up(keyboard, SDLK_DOWN, false);
        if (keyboard->auto_shift_up_active) {
            keyboard->auto_shift_up_active = false;
            // Only auto-release SHIFT if cursor-left isn't also holding it
            if (!keyboard->auto_shift_left_active) {
                commodore_keyboard_key_up(keyboard, CbmKeys::SHIFT_LEFT, false);
            }
        }
        return;
    }

    // Map host key to Commodore key
    uint32_t commodore_key = commodore_keyboard_map_host_key(key_code, shifted);

    // Find the key position - use lookup table for ASCII keys, linear search for extended SDL keycodes
    uint8_t row, col;
    bool found = false;

    if (commodore_key < MAX_KEY_LOOKUP) {
        key_matrix_info_t* key_info = &keyboard->key_lookup[commodore_key];
        if (key_info->key_code != 0) {
            row = key_info->row;
            col = key_info->col;
            found = true;
        }
    }

    if (!found) {
        found = find_key_in_matrix(keyboard->active_unshifted, keyboard->active_shifted,
                                   commodore_key, &row, &col);
    }

    if (found) {
        // Convert from array indices to hardware CIA/VIA bit numbers.
        // See key_down for the full explanation of the transposed mapping.
        uint8_t pa_bit = 7 - row;
        uint8_t pb_bit = 7 - col;

        // Open the contact (key released)
        keyboard->row_open_contacts[pa_bit] |= (1 << pb_bit);
        keyboard->col_open_contacts[pb_bit] |= (1 << pa_bit);
    }
}

void commodore_keyboard_update_matrix(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;

    // Update matrix based on current contact states
    // This would be called during CIA/VIA scanning cycles
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            uint8_t row_mask = 1 << row;
            uint8_t col_mask = 1 << col;

            bool row_contact_closed = !(keyboard->row_open_contacts[col] & row_mask);
            bool col_contact_closed = !(keyboard->col_open_contacts[row] & col_mask);

            // Contact is closed if both row and column contacts are closed
            bool contact_closed = row_contact_closed && col_contact_closed;

            // Update matrix info
            keyboard->key_matrix[row][col].shifted = contact_closed &&
                (keyboard->active_shifted[row][col] != CbmKeys::SAME);
        }
    }
}

void commodore_keyboard_connect_ports(commodore_keyboard_t* keyboard,
                                    void* port_a, void* port_b) {
    if (!keyboard) return;

    keyboard->cia_port_a_reference = port_a;
    keyboard->cia_port_b_reference = port_b;

    // If ports are provided, update the keyboard matrix based on current port states
    if (port_a && port_b) {
        commodore_keyboard_update_matrix(keyboard);
    }
}

void commodore_keyboard_toggle_caps_lock(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;
    keyboard->caps_lock_active = !keyboard->caps_lock_active;
}

// Keyboard matrix state functions for CIA/VIA integration
bool commodore_keyboard_is_row_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col) {
    if (!keyboard) return false;

    // Check if the contact at the specified row/col is closed
    uint8_t row_mask = 1 << row;
    return !(keyboard->row_open_contacts[col] & row_mask);
}

bool commodore_keyboard_is_col_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col) {
    if (!keyboard) return false;

    // Check if the contact at the specified row/col is closed
    uint8_t col_mask = 1 << col;
    return !(keyboard->col_open_contacts[row] & col_mask);
}

void commodore_keyboard_print_matrix(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;

    printf("Commodore Keyboard Matrix State:\n");
    printf("Row/Col | 0  1  2  3  4  5  6  7\n");
    printf("--------------------------------\n");

    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        printf("   %d    |", row);
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            bool contact_closed = !(keyboard->row_open_contacts[col] & (1 << row));
            printf(" %c ", contact_closed ? 'X' : 'O');
        }
        printf("\n");
    }

    printf("\nContact States:\n");
    for (int col = 0; col < KEYBOARD_COLS; col++) {
        printf("Col %d: 0x%02X ", col, keyboard->row_open_contacts[col]);
    }
    printf("\n");
}

void commodore_keyboard_print_state(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;

    printf("Keyboard State:\n");
    printf("  RESTORE Key: %s\n", keyboard->restore_key_pressed ? "PRESSED" : "RELEASED");
    printf("  CAPS LOCK: %s\n", keyboard->caps_lock_active ? "ACTIVE" : "INACTIVE");
    printf("  CIA Port A: %p\n", keyboard->cia_port_a_reference);
    printf("  CIA Port B: %p\n", keyboard->cia_port_b_reference);
}