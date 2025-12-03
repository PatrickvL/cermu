#ifndef COMMODORE_KEYBOARD_H
#define COMMODORE_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL_keycode.h>

#include "../../core/chip.h"
#include "../../core/system_lines.h"

// Commodore Keyboard SDL-based implementation
// Uses SDL keycodes for host-native key information

// SDL keycode namespace - using direct SDL_Keycode values
namespace C64Keys {
    // Regular ASCII keys (0-127)
    const uint32_t SAME = 0;              // Used when shifted key equals unshifted
    const uint32_t ARROW_LEFT = SDLK_LEFT;        // Left arrow
    const uint32_t ARROW_UP = SDLK_UP;            // Up arrow
    const uint32_t CLR = SDLK_HOME;               // CLR/HOME
    const uint32_t COMMODORE = SDLK_LGUI;         // Commodore key (Windows/Command)
    const uint32_t CTRL = SDLK_LCTRL;             // Control key
    const uint32_t CURSOR_DOWN = SDLK_DOWN;       // Down arrow
    const uint32_t CURSOR_LEFT = SDLK_LEFT;       // Left arrow (alias)
    const uint32_t CURSOR_RIGHT = SDLK_RIGHT;     // Right arrow
    const uint32_t CURSOR_UP = SDLK_UP;           // Up arrow (alias)
    const uint32_t DEL = SDLK_BACKSPACE;          // Delete/Backspace
    const uint32_t F1 = SDLK_F1;                  // Function keys
    const uint32_t F2 = SDLK_F2;
    const uint32_t F3 = SDLK_F3;
    const uint32_t F4 = SDLK_F4;
    const uint32_t F5 = SDLK_F5;
    const uint32_t F6 = SDLK_F6;
    const uint32_t F7 = SDLK_F7;
    const uint32_t F8 = SDLK_F8;
    const uint32_t HOME = SDLK_HOME;              // Home key
    const uint32_t INST = SDLK_INSERT;            // Insert key
    const uint32_t PI = 0;                       // Pi symbol (not directly mappable)
    const uint32_t POUND = 0;                    // Pound symbol (not directly mappable)
    const uint32_t RESTORE = SDLK_BACKQUOTE;     // Restore key (backtick/tilde)
    const uint32_t RETURN = SDLK_RETURN;          // Return/Enter
    const uint32_t RUN_STOP = SDLK_TAB;          // Run/Stop key
    const uint32_t SHIFT_LEFT = SDLK_LSHIFT;     // Left shift
    const uint32_t SHIFT_RIGHT = SDLK_RSHIFT;    // Right shift
    const uint32_t SPACE = SDLK_SPACE;            // Space bar
}

#define KEYBOARD_ROWS 8
#define KEYBOARD_COLS 8
#define MAX_KEY_LOOKUP 512  // Increased for SDL keycode range

typedef struct {
    uint8_t row;
    uint8_t col;
    bool shifted;
    uint32_t key_code;  // SDL keycode (32-bit to handle all SDLK_* values)
} key_matrix_info_t;

typedef struct {
    chip_descriptor_t descriptor;

    // Keyboard matrix state
    uint8_t row_open_contacts[KEYBOARD_COLS];  // Column-based row contacts
    uint8_t col_open_contacts[KEYBOARD_ROWS];  // Row-based column contacts

    // Keyboard matrix lookup
    key_matrix_info_t key_matrix[KEYBOARD_ROWS][KEYBOARD_COLS];
    key_matrix_info_t key_lookup[MAX_KEY_LOOKUP];  // SDL keycode lookup

    // Current keyboard state
    bool restore_key_pressed;
    bool caps_lock_active;

    // Reference to connected CIA/VIA chips
    void* cia_port_a_reference;  // For column scanning
    void* cia_port_b_reference;  // For row reading

} commodore_keyboard_t;

// Keyboard matrix definition (C64/VIC-20 layout)
// Using SDL keycodes for all keys
extern const uint32_t keyboard_matrix_unshifted[KEYBOARD_ROWS][KEYBOARD_COLS];
extern const uint32_t keyboard_matrix_shifted[KEYBOARD_ROWS][KEYBOARD_COLS];

// Function declarations
commodore_keyboard_t* commodore_keyboard_create();
void commodore_keyboard_destroy(commodore_keyboard_t* keyboard);
void commodore_keyboard_reset(commodore_keyboard_t* keyboard);

// SDL-based keyboard input handling
void commodore_keyboard_key_down(commodore_keyboard_t* keyboard, uint32_t key_code, bool shifted);
void commodore_keyboard_key_up(commodore_keyboard_t* keyboard, uint32_t key_code, bool shifted);

// SDL keycode to Commodore key mapping
uint32_t commodore_keyboard_map_host_key(uint32_t sdl_key, bool shifted);
bool commodore_keyboard_is_special_key(uint32_t key_code);

// Keyboard scanning and CIA/VIA integration
void commodore_keyboard_update_matrix(commodore_keyboard_t* keyboard);
void commodore_keyboard_connect_ports(commodore_keyboard_t* keyboard, void* port_a, void* port_b);

// Utility functions
void commodore_keyboard_toggle_caps_lock(commodore_keyboard_t* keyboard);

// Keyboard matrix state functions for CIA/VIA integration
bool commodore_keyboard_is_row_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col);
bool commodore_keyboard_is_col_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col);

// Debug functions
void commodore_keyboard_print_matrix(commodore_keyboard_t* keyboard);
void commodore_keyboard_print_state(commodore_keyboard_t* keyboard);

#endif // COMMODORE_KEYBOARD_H