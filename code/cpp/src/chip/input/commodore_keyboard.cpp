#include "commodore_keyboard.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Keyboard matrix definition (C64/VIC-20 layout)
// Unshifted keys - using SDL keycodes
// Note: This matches the C# version exactly for compatibility
const uint32_t keyboard_matrix_unshifted[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {C64Keys::RUN_STOP, '/', ',', 'N', 'V', 'X', C64Keys::SHIFT_LEFT, C64Keys::CURSOR_DOWN}, // row 7
    {'Q', C64Keys::ARROW_UP, '@', 'O', 'U', 'T', 'E', C64Keys::F5}, // row 6
    {C64Keys::COMMODORE, '=', ':', 'K', 'H', 'F', 'S', C64Keys::F3}, // row 5
    {C64Keys::SPACE, C64Keys::SHIFT_RIGHT, '.', 'M', 'B', 'C', 'Z', C64Keys::F1}, // row 4
    {'2', C64Keys::HOME, '-', '0', '8', '6', '4', C64Keys::F7}, // row 3
    {C64Keys::CTRL, ';', 'L', 'J', 'G', 'D', 'A', C64Keys::CURSOR_LEFT}, // row 2
    {C64Keys::ARROW_LEFT, '*', 'P', 'I', 'Y', 'R', 'W', C64Keys::RETURN}, // row 1
    {'1', C64Keys::POUND, '+', '9', '7', '5', '3', C64Keys::DEL}, // row 0
};

// Shifted keys - using SDL keycodes
// Note: This matches the C# version exactly for compatibility
const uint32_t keyboard_matrix_shifted[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {C64Keys::SAME, '?', '<', 'n', 'v', 'x', C64Keys::SAME, C64Keys::CURSOR_UP}, // row 7
    {'q', C64Keys::PI, C64Keys::SAME, 'o', 'u', 't', 'e', C64Keys::F6}, // row 6
    {C64Keys::SAME, C64Keys::SAME, '[', 'k', 'h', 'f', 's', C64Keys::F4}, // row 5
    {C64Keys::SAME, C64Keys::SAME, '>', 'm', 'b', 'c', 'z', C64Keys::F2}, // row 4
    {'"', C64Keys::CLR, C64Keys::SAME, C64Keys::SAME, '(', '&', '$', C64Keys::F8}, // row 3
    {C64Keys::SAME, ']', 'l', 'j', 'g', 'd', 'a', C64Keys::CURSOR_RIGHT}, // row 2
    {C64Keys::SAME, C64Keys::SAME, 'p', 'i', 'y', 'r', 'w', C64Keys::SAME}, // row 1
    {'!', C64Keys::SAME, C64Keys::SAME, ')', '\'', '%', '#', C64Keys::INST}, // row 0
};

// Special key mappings - SDL keycode compatible
static const uint32_t special_key_mapping[][2] = {
    {C64Keys::DEL, C64Keys::DEL},  // DEL
    {C64Keys::HOME, C64Keys::HOME},  // HOME
    {C64Keys::RUN_STOP, C64Keys::RUN_STOP},  // RUN/STOP
    {C64Keys::CURSOR_DOWN, C64Keys::CURSOR_DOWN},  // CURSOR DOWN
    {C64Keys::SHIFT_RIGHT, C64Keys::SHIFT_RIGHT},  // SHIFT RIGHT
    {C64Keys::F1, C64Keys::F1},  // F1
    {C64Keys::F3, C64Keys::F3},  // F3
    {C64Keys::F5, C64Keys::F5},  // F5
    {C64Keys::F7, C64Keys::F7},  // F7
    {C64Keys::F2, C64Keys::F2},  // F2
    {C64Keys::F4, C64Keys::F4},  // F4
    {C64Keys::F6, C64Keys::F6},  // F6
    {C64Keys::F8, C64Keys::F8},  // F8
    {C64Keys::CLR, C64Keys::CLR},  // CLR/INST
    {C64Keys::PI, C64Keys::PI},  // PI
    {C64Keys::ARROW_LEFT, C64Keys::ARROW_LEFT},  // ARROW LEFT
    {C64Keys::ARROW_UP, C64Keys::ARROW_UP},  // ARROW UP
    {C64Keys::CURSOR_RIGHT, C64Keys::CURSOR_RIGHT},  // CURSOR RIGHT
    {C64Keys::CTRL, C64Keys::CTRL},  // CTRL
    {C64Keys::RETURN, C64Keys::RETURN},  // RETURN
    {C64Keys::SPACE, C64Keys::SPACE},  // SPACE
    {C64Keys::SHIFT_LEFT, C64Keys::SHIFT_LEFT},  // SHIFT LEFT
    {C64Keys::COMMODORE, C64Keys::COMMODORE},  // COMMODORE
    {C64Keys::RESTORE, C64Keys::RESTORE},  // RESTORE
    {C64Keys::CURSOR_UP, C64Keys::CURSOR_UP},  // CURSOR UP
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

    // Initialize matrix lookup
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            keyboard->key_matrix[row][col] = (key_matrix_info_t){
                .row = (uint8_t)row,
                .col = (uint8_t)col,
                .shifted = false,
                .key_code = keyboard_matrix_unshifted[row][col]
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

    // Build key lookup table - only for keys within bounds
    // Note: SDL keycodes can be very large (e.g., SDLK_F1 = 0x4000003A)
    // so we skip the lookup table for now and will use linear search
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            uint32_t unshifted_key = keyboard_matrix_unshifted[row][col];
            uint32_t shifted_key = keyboard_matrix_shifted[row][col];

            // Only add to lookup table if within bounds (ASCII range)
            if (unshifted_key != C64Keys::SAME && unshifted_key != 0 && unshifted_key < MAX_KEY_LOOKUP) {
                keyboard->key_lookup[unshifted_key] = (key_matrix_info_t){
                    .row = (uint8_t)row,
                    .col = (uint8_t)col,
                    .shifted = false,
                    .key_code = unshifted_key
                };
            }

            if (shifted_key != C64Keys::SAME && shifted_key != 0 && shifted_key != unshifted_key && shifted_key < MAX_KEY_LOOKUP) {
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

void commodore_keyboard_key_down(commodore_keyboard_t* keyboard, uint32_t key_code, bool shifted) {
    if (!keyboard) return;

    // Handle RESTORE key (special case - connects to NMI)
    if (key_code == C64Keys::RESTORE) { // RESTORE
        keyboard->restore_key_pressed = true;
        return;
    }

    // Handle CAPS LOCK
    if (key_code == C64Keys::COMMODORE) { // COMMODORE key toggles CAPS LOCK
        keyboard->caps_lock_active = !keyboard->caps_lock_active;
        return;
    }

    // Map host key to Commodore key
    uint32_t commodore_key = commodore_keyboard_map_host_key(key_code, shifted);

    // Find the key in lookup table
    key_matrix_info_t* key_info = &keyboard->key_lookup[commodore_key];

    if (key_info->key_code != 0) {
        // Close the contact (key pressed)
        uint8_t row_mask = 1 << key_info->row;
        uint8_t col_mask = 1 << key_info->col;

        // Update row contacts (column-based)
        keyboard->row_open_contacts[key_info->col] &= ~row_mask;

        // Update column contacts (row-based)
        keyboard->col_open_contacts[key_info->row] &= ~col_mask;
    }
}

void commodore_keyboard_key_up(commodore_keyboard_t* keyboard, uint32_t key_code, bool shifted) {
    if (!keyboard) return;

    // Handle RESTORE key
    if (key_code == C64Keys::RESTORE) { // RESTORE
        keyboard->restore_key_pressed = false;
        return;
    }

    // Map host key to Commodore key
    uint32_t commodore_key = commodore_keyboard_map_host_key(key_code, shifted);

    // Find the key in lookup table
    key_matrix_info_t* key_info = &keyboard->key_lookup[commodore_key];

    if (key_info->key_code != 0) {
        // Open the contact (key released)
        uint8_t row_mask = 1 << key_info->row;
        uint8_t col_mask = 1 << key_info->col;

        // Update row contacts (column-based)
        keyboard->row_open_contacts[key_info->col] |= row_mask;

        // Update column contacts (row-based)
        keyboard->col_open_contacts[key_info->row] |= col_mask;
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
                (keyboard_matrix_shifted[row][col] != C64Keys::SAME);
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