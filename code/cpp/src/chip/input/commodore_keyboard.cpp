#include "commodore_keyboard.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Keyboard matrix definition (C64/VIC-20 layout)
// Unshifted keys - using ASCII-compatible values
const char keyboard_matrix_unshifted[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {'1',   0x7F, '+',   '9',   '7',   '5',   '3',   0x1B},  // ROW0 (DEL, ARROW_LEFT)
    {'2',   0x0C, '-', '0',   '8',   '6',   '4',   0x01},  // ROW1 (HOME, F7)
    {'3',   0x18, '$', 'P',   'I',   'Y',   'R',   0x02},  // ROW2 (RUN/STOP, F1)
    {'4',   0x19, '*', 'L',   'J',   'G',   'D',   0x03},  // ROW3 (CURSOR_DOWN, F3)
    {0x20, 0x1A, '.', 'M',   'B',   'C',   'Z',   0x04},  // ROW4 (SPACE, SHIFT_RIGHT, F5)
    {'[', '=', ':', 'K',   'H',   'F',   'S',   0x05},  // ROW5 (F2)
    {'Q',   0x1C, '@', 'O',   'U',   'T',   'E',   0x06},  // ROW6 (ARROW_UP, F4)
    {'W',   0x1D, '\\', 'A',   ';',   'X',   0x0D, 0x07}   // ROW7 (CTRL, RETURN, F6)
};

// Shifted keys - using ASCII-compatible values
const char keyboard_matrix_shifted[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {'!',   0x12, '$', ')', '\'', '%', '#',   0x1E},  // ROW0 (INST, PI)
    {'"',   0x12, '~', '0',   '8',   '6',   '4',   0x08},  // ROW1 (CLR, F8)
    {'#',   0x01, '$', 'P',   'I',   'Y',   'R',   0x02},  // ROW2 (F1)
    {'$',   0x1F, '*', 'L',   'J',   'G',   'D',   0x03},  // ROW3 (CURSOR_RIGHT, F3)
    {0x01, 0x01, '>', 'M',   'B',   'C',   'Z',   0x05},  // ROW4 (F2)
    {']', '+', '[', 'K',   'H',   'F',   'S',   0x06},  // ROW5 (F4)
    {'Q',   0x1E, '$', 'O',   'U',   'T',   'E',   0x07},  // ROW6 (PI, F6)
    {'W',   0x01, '|', 'A',   ':',   'X',   0x01, 0x07}   // ROW7 (F6)
};

// Special key mappings - ASCII-compatible
static const char special_key_mapping[][2] = {
    {0x7F, 0x7F},  // DEL
    {0x0C, 0x0C},  // HOME
    {0x18, 0x18},  // RUN/STOP
    {0x19, 0x19},  // CURSOR DOWN
    {0x1A, 0x1A},  // SHIFT RIGHT
    {0x02, 0x02},  // F1
    {0x03, 0x03},  // F3
    {0x04, 0x04},  // F5
    {0x01, 0x01},  // F7
    {0x05, 0x05},  // F2
    {0x06, 0x06},  // F4
    {0x07, 0x07},  // F6
    {0x08, 0x08},  // F8
    {0x12, 0x12},  // CLR/INST
    {0x1E, 0x1E},  // PI
    {0x1B, 0x1B},  // ARROW LEFT
    {0x1C, 0x1C},  // ARROW UP
    {0x1F, 0x1F},  // CURSOR RIGHT
    {0x1D, 0x1D},  // CTRL
    {0x0D, 0x0D},  // RETURN
    {0x20, 0x20},  // SPACE
    {0x10, 0x10},  // SHIFT LEFT
    {0x11, 0x11},  // COMMODORE
    {0x13, 0x13},  // RESTORE
    {0x1C, 0x1C},  // CURSOR UP
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
    if (key_char == 0x13) { // RESTORE
        keyboard->restore_key_pressed = true;
        return;
    }

    // Handle CAPS LOCK
    if (key_char == 0x11) { // COMMODORE key toggles CAPS LOCK
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
    if (key_char == 0x13) { // RESTORE
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