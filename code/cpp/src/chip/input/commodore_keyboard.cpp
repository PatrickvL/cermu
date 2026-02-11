#include "commodore_keyboard.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Keyboard matrix data is provided by the system layer at creation time.
// See: systems/c64/c64_keyboard_matrix.cpp, systems/vic20/vic20_keyboard_matrix.cpp

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

commodore_keyboard_t* commodore_keyboard_create(
        const uint32_t (*unshifted)[KEYBOARD_COLS],
        const uint32_t (*shifted)[KEYBOARD_COLS]) {
    if (!unshifted || !shifted) return NULL;

    commodore_keyboard_t* keyboard = (commodore_keyboard_t*)malloc(sizeof(commodore_keyboard_t));
    if (!keyboard) return NULL;

    // Initialize chip descriptor
    keyboard->descriptor = (chip_descriptor_t){
        .description = "Commodore Keyboard Matrix Emulation",
        .create = NULL,
        .destroy = NULL,
        .bus_attach = NULL,
        .bank_change = NULL
    };

    // Use caller-supplied matrix
    keyboard->active_unshifted = unshifted;
    keyboard->active_shifted = shifted;

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

    // Active matrix must have been set by create()
    if (!keyboard->active_unshifted) {
        printf("ERROR: commodore_keyboard_reset called with no active matrix set!\n");
        return;
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