#include "commodore_keyboard.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Keyboard matrix definition (C64/VIC-20 layout)
// Unshifted keys - using ASCII-compatible values
// Note: This matches the C# version exactly for compatibility
const char keyboard_matrix_unshifted[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {KEY_RUN_STOP, '/', ',', 'N', 'V', 'X', KEY_SHIFT_LEFT, KEY_CURSOR_DOWN}, // row 7
    {'Q', KEY_ARROW_UP, '@', 'O', 'U', 'T', 'E', KEY_F5}, // row 6
    {KEY_COMMODORE, '=', ':', 'K', 'H', 'F', 'S', KEY_F3}, // row 5
    {KEY_SPACE, KEY_SHIFT_RIGHT, '.', 'M', 'B', 'C', 'Z', KEY_F1}, // row 4
    {'2', KEY_HOME, '-', '0', '8', '6', '4', KEY_F7}, // row 3
    {KEY_CTRL, ';', 'L', 'J', 'G', 'D', 'A', KEY_CURSOR_LEFT}, // row 2
    {KEY_ARROW_LEFT, '*', 'P', 'I', 'Y', 'R', 'W', KEY_RETURN}, // row 1
    {'1', KEY_POUND, '+', '9', '7', '5', '3', KEY_DEL}, // row 0
};

// Shifted keys - using ASCII-compatible values
// Note: This matches the C# version exactly for compatibility
const char keyboard_matrix_shifted[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {KEY_SAME, '?', '<', 'n', 'v', 'x', KEY_SAME, KEY_CURSOR_UP}, // row 7
    {'q', KEY_PI, KEY_SAME, 'o', 'u', 't', 'e', KEY_F6}, // row 6
    {KEY_SAME, KEY_SAME, '[', 'k', 'h', 'f', 's', KEY_F4}, // row 5
    {KEY_SAME, KEY_SAME, '>', 'm', 'b', 'c', 'z', KEY_F2}, // row 4
    {'"', KEY_CLR, KEY_SAME, KEY_SAME, '(', '&', '$', KEY_F8}, // row 3
    {KEY_SAME, ']', 'l', 'j', 'g', 'd', 'a', KEY_CURSOR_RIGHT}, // row 2
    {KEY_SAME, KEY_SAME, 'p', 'i', 'y', 'r', 'w', KEY_SAME}, // row 1
    {'!', KEY_SAME, KEY_SAME, ')', '\'', '%', '#', KEY_INST}, // row 0
};

// Special key mappings - ASCII-compatible
static const char special_key_mapping[][2] = {
    {KEY_DEL, KEY_DEL},  // DEL
    {KEY_HOME, KEY_HOME},  // HOME
    {KEY_RUN_STOP, KEY_RUN_STOP},  // RUN/STOP
    {KEY_CURSOR_DOWN, KEY_CURSOR_DOWN},  // CURSOR DOWN
    {KEY_SHIFT_RIGHT, KEY_SHIFT_RIGHT},  // SHIFT RIGHT
    {KEY_F1, KEY_F1},  // F1
    {KEY_F3, KEY_F3},  // F3
    {KEY_F5, KEY_F5},  // F5
    {KEY_F7, KEY_F7},  // F7
    {KEY_F2, KEY_F2},  // F2
    {KEY_F4, KEY_F4},  // F4
    {KEY_F6, KEY_F6},  // F6
    {KEY_F8, KEY_F8},  // F8
    {KEY_CLR, KEY_CLR},  // CLR/INST
    {KEY_PI, KEY_PI},  // PI
    {KEY_ARROW_LEFT, KEY_ARROW_LEFT},  // ARROW LEFT
    {KEY_ARROW_UP, KEY_ARROW_UP},  // ARROW UP
    {KEY_CURSOR_RIGHT, KEY_CURSOR_RIGHT},  // CURSOR RIGHT
    {KEY_CTRL, KEY_CTRL},  // CTRL
    {KEY_RETURN, KEY_RETURN},  // RETURN
    {KEY_SPACE, KEY_SPACE},  // SPACE
    {KEY_SHIFT_LEFT, KEY_SHIFT_LEFT},  // SHIFT LEFT
    {KEY_COMMODORE, KEY_COMMODORE},  // COMMODORE
    {KEY_RESTORE, KEY_RESTORE},  // RESTORE
    {KEY_CURSOR_UP, KEY_CURSOR_UP},  // CURSOR UP
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
                .key_char = keyboard_matrix_unshifted[row][col]
            };
        }
    }

    // Initialize key lookup table
    for (int i = 0; i < MAX_KEY_LOOKUP; i++) {
        keyboard->key_lookup[i] = (key_matrix_info_t){
            .row = 0,
            .col = 0,
            .shifted = false,
            .key_char = 0
        };
    }

    // Build key lookup table
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            char unshifted_key = keyboard_matrix_unshifted[row][col];
            char shifted_key = keyboard_matrix_shifted[row][col];

            if (unshifted_key != KEY_SAME && unshifted_key != KEY_NONE) {
                keyboard->key_lookup[(uint8_t)unshifted_key] = (key_matrix_info_t){
                    .row = (uint8_t)row,
                    .col = (uint8_t)col,
                    .shifted = false,
                    .key_char = unshifted_key
                };
            }

            if (shifted_key != KEY_SAME && shifted_key != KEY_NONE && shifted_key != unshifted_key) {
                keyboard->key_lookup[(uint8_t)shifted_key] = (key_matrix_info_t){
                    .row = (uint8_t)row,
                    .col = (uint8_t)col,
                    .shifted = true,
                    .key_char = shifted_key
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

char commodore_keyboard_map_host_key(char host_key) {
    // Map host keyboard keys to Commodore keys
    // This is a simplified mapping - would need to be expanded for full functionality
    for (int i = 0; special_key_mapping[i][0] != 0; i++) {
        if (host_key == special_key_mapping[i][0]) {
            return special_key_mapping[i][1];
        }
    }
    return host_key; // Regular character
}

bool commodore_keyboard_is_special_key(char key_char) {
    // Check if key is a special function key
    for (int i = 0; special_key_mapping[i][0] != 0; i++) {
        if (key_char == special_key_mapping[i][0]) {
            return true;
        }
    }
    return false;
}

void commodore_keyboard_key_down(commodore_keyboard_t* keyboard, char key_char) {
    if (!keyboard) return;

    // Handle RESTORE key (special case - connects to NMI)
    if (key_char == KEY_RESTORE) { // RESTORE
        keyboard->restore_key_pressed = true;
        return;
    }

    // Handle CAPS LOCK
    if (key_char == KEY_COMMODORE) { // COMMODORE key toggles CAPS LOCK
        keyboard->caps_lock_active = !keyboard->caps_lock_active;
        return;
    }

    // Map host key to Commodore key
    char commodore_key = commodore_keyboard_map_host_key(key_char);

    // Find the key in lookup table
    key_matrix_info_t* key_info = &keyboard->key_lookup[(uint8_t)commodore_key];

    if (key_info->key_char != 0) {
        // Close the contact (key pressed)
        uint8_t row_mask = 1 << key_info->row;
        uint8_t col_mask = 1 << key_info->col;

        // Update row contacts (column-based)
        keyboard->row_open_contacts[key_info->col] &= ~row_mask;

        // Update column contacts (row-based)
        keyboard->col_open_contacts[key_info->row] &= ~col_mask;
    }
}

void commodore_keyboard_key_up(commodore_keyboard_t* keyboard, char key_char) {
    if (!keyboard) return;

    // Handle RESTORE key
    if (key_char == KEY_RESTORE) { // RESTORE
        keyboard->restore_key_pressed = false;
        return;
    }

    // Map host key to Commodore key
    char commodore_key = commodore_keyboard_map_host_key(key_char);

    // Find the key in lookup table
    key_matrix_info_t* key_info = &keyboard->key_lookup[(uint8_t)commodore_key];

    if (key_info->key_char != 0) {
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
                (keyboard_matrix_shifted[row][col] != KEY_SAME);
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