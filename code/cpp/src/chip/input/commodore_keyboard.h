#ifndef COMMODORE_KEYBOARD_H
#define COMMODORE_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#include "../../core/chip.h"
#include "../../core/system_lines.h"

// Commodore Keyboard Matrix Layout
// 8x8 matrix representing the C64/VIC-20 keyboard
// Rows are connected to CIA/VIA Port B (read)
// Columns are connected to CIA/VIA Port A (write)

#define KEYBOARD_ROWS 8
#define KEYBOARD_COLS 8
#define MAX_KEY_LOOKUP 256

// Special key codes (using ASCII range)
#define KEY_NONE        0x00
#define KEY_SAME        0x01  // Shifted key same as unshifted
#define KEY_RUN_STOP    0x02
#define KEY_ARROW_LEFT  0x03
#define KEY_ARROW_UP    0x04
#define KEY_CLR         0x05
#define KEY_COMMODORE   0x06
#define KEY_CTRL        0x07
#define KEY_CURSOR_DOWN 0x08
#define KEY_CURSOR_LEFT  0x09
#define KEY_CURSOR_RIGHT 0x0A
#define KEY_CURSOR_UP    0x0B
#define KEY_DEL         0x0C
#define KEY_F1          0x0D
#define KEY_F2          0x0E
#define KEY_F3          0x0F
#define KEY_F4          0x10
#define KEY_F5          0x11
#define KEY_F6          0x12
#define KEY_F7          0x13
#define KEY_F8          0x14
#define KEY_HOME        0x15
#define KEY_INST        0x16
#define KEY_PI          0x17
#define KEY_RESTORE     0x18
#define KEY_RETURN      0x19
#define KEY_SHIFT_LEFT  0x1A
#define KEY_SHIFT_RIGHT 0x1B
#define KEY_SPACE       0x1C
#define KEY_POUND       0x1D

typedef struct {
    uint8_t row;
    uint8_t col;
    bool shifted;
    char key_char;
} key_matrix_info_t;

typedef struct {
    chip_descriptor_t descriptor;

    // Keyboard matrix state
    uint8_t row_open_contacts[KEYBOARD_COLS];  // Column-based row contacts
    uint8_t col_open_contacts[KEYBOARD_ROWS];  // Row-based column contacts

    // Keyboard matrix lookup
    key_matrix_info_t key_matrix[KEYBOARD_ROWS][KEYBOARD_COLS];
    key_matrix_info_t key_lookup[MAX_KEY_LOOKUP];  // Simple array-based lookup

    // Current keyboard state
    bool restore_key_pressed;
    bool caps_lock_active;

    // Reference to connected CIA/VIA chips
    void* cia_port_a_reference;  // For column scanning
    void* cia_port_b_reference;  // For row reading

} commodore_keyboard_t;

// Keyboard matrix definition (C64/VIC-20 layout)
// Unshifted keys
extern const char keyboard_matrix_unshifted[KEYBOARD_ROWS][KEYBOARD_COLS];
// Shifted keys
extern const char keyboard_matrix_shifted[KEYBOARD_ROWS][KEYBOARD_COLS];

// Function declarations
commodore_keyboard_t* commodore_keyboard_create();
void commodore_keyboard_destroy(commodore_keyboard_t* keyboard);
void commodore_keyboard_reset(commodore_keyboard_t* keyboard);

// Keyboard input handling
void commodore_keyboard_key_down(commodore_keyboard_t* keyboard, char key_char);
void commodore_keyboard_key_up(commodore_keyboard_t* keyboard, char key_char);

// Keyboard scanning and CIA/VIA integration
void commodore_keyboard_update_matrix(commodore_keyboard_t* keyboard);
void commodore_keyboard_connect_ports(commodore_keyboard_t* keyboard, void* port_a, void* port_b);

// Utility functions
char commodore_keyboard_map_host_key(char host_key);
bool commodore_keyboard_is_special_key(char key_char);
void commodore_keyboard_toggle_caps_lock(commodore_keyboard_t* keyboard);

// Keyboard matrix state functions for CIA/VIA integration
bool commodore_keyboard_is_row_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col);
bool commodore_keyboard_is_col_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col);

// Debug functions
void commodore_keyboard_print_matrix(commodore_keyboard_t* keyboard);
void commodore_keyboard_print_state(commodore_keyboard_t* keyboard);

#endif // COMMODORE_KEYBOARD_H