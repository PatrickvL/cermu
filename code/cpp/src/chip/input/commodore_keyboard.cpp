#include "commodore_keyboard.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <new>

// ============================================================================
// Human-readable names for debug output
// ============================================================================

static const char* keyboard_model_names[] = {
    "Unknown",
    "C64",
    "VIC-20",
    "C128",
    "Plus/4 / C16",
    "PET/CBM",
    "CBM-II",
};

static const char* keyboard_scan_chip_names[] = {
    "Unknown",
    "CIA (MOS 6526)",
    "VIA (MOS 6522)",
    "TED (MOS 7360)",
    "PIA (MOS 6520)",
    "TPI (MOS 6525)",
};

// ============================================================================
// Lifecycle
// ============================================================================

commodore_keyboard_t* commodore_keyboard_create(const keyboard_matrix_config_t* config) {
    if (!config || !config->keys || !config->decode_tables || config->num_decode_tables == 0) return NULL;
    if (config->rows == 0 || config->cols == 0) return NULL;
    if (config->rows > MAX_KEYBOARD_ROWS || config->cols > MAX_KEYBOARD_COLS) {
        printf("ERROR: Keyboard matrix %dx%d exceeds maximum %dx%d\n",
               config->rows, config->cols, MAX_KEYBOARD_ROWS, MAX_KEYBOARD_COLS);
        return NULL;
    }

    // Use placement new so the unordered_map constructor runs
    void* mem = malloc(sizeof(commodore_keyboard_t));
    if (!mem) return NULL;
    commodore_keyboard_t* keyboard = new (mem) commodore_keyboard_t();

    // Initialize chip descriptor
    keyboard->descriptor = (chip_descriptor_t){
        .description = "Commodore Keyboard Matrix Emulation",
        .create = NULL,
        .destroy = NULL,
        .bus_attach = NULL,
        .bank_change = NULL
    };

    // Store model and matrix configuration
    keyboard->model = config->model;
    keyboard->scan_chip = config->scan_chip;
    keyboard->matrix_rows = config->rows;
    keyboard->matrix_cols = config->cols;

    // Store active matrix pointers
    keyboard->active_keys = config->keys;
    keyboard->num_decode_tables = config->num_decode_tables;
    keyboard->decode_tables = config->decode_tables;

    // Initialize keyboard state
    commodore_keyboard_reset(keyboard);

    printf("Keyboard: Created %s %dx%d matrix (scan: %s)\n",
           config->description ? config->description : keyboard_model_names[config->model],
           config->rows, config->cols,
           keyboard_scan_chip_names[config->scan_chip]);

    return keyboard;
}

void commodore_keyboard_destroy(commodore_keyboard_t* keyboard) {
    if (keyboard) {
        // Manually call destructor for the unordered_map, then free
        keyboard->~commodore_keyboard_t();
        free(keyboard);
    }
}

void commodore_keyboard_reset(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;

    uint8_t rows = keyboard->matrix_rows;
    uint8_t cols = keyboard->matrix_cols;

    // Initialize all contacts as open (no keys pressed)
    for (int r = 0; r < MAX_KEYBOARD_ROWS; r++) {
        keyboard->row_open_contacts[r] = (r < rows) ? (uint8_t)((1 << cols) - 1) : 0x00;
    }
    for (int c = 0; c < MAX_KEYBOARD_COLS; c++) {
        keyboard->col_open_contacts[c] = (c < cols) ? (uint16_t)((1 << rows) - 1) : 0x0000;
    }

    // ========================================================================
    // Build optimised EmuKey → {row, col} lookup from the keys[] table
    // ========================================================================
    memset(keyboard->key_direct_valid, 0, sizeof(keyboard->key_direct_valid));
    memset(keyboard->key_direct_lookup, 0, sizeof(keyboard->key_direct_lookup));
    keyboard->key_ext_lookup.clear();

    if (!keyboard->active_keys) {
        printf("ERROR: commodore_keyboard_reset called with no active keys set!\n");
        return;
    }

    int identity_count = 0;
    int ext_count = 0;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            emu_key_t key = keyboard->active_keys[row * cols + col];

            // Skip markers and invalid entries
            if (emu_key_is_marker(key) || key == 0) continue;

            key_position_t pos = { (uint8_t)row, (uint8_t)col };

            if (emu_key_is_identity(key)) {
                // Identity-mapped key (0–511): direct array lookup
                if (!keyboard->key_direct_valid[key]) {
                    keyboard->key_direct_lookup[key] = pos;
                    keyboard->key_direct_valid[key] = true;
                    identity_count++;
                }
            } else {
                // Emulator-specific key (512+): hash map lookup
                if (keyboard->key_ext_lookup.find(key) == keyboard->key_ext_lookup.end()) {
                    keyboard->key_ext_lookup[key] = pos;
                    ext_count++;
                }
            }
        }
    }

    // Initialize special state
    keyboard->restore_key_pressed = false;
    keyboard->caps_lock_active = false;
    keyboard->auto_shift_left_active = false;
    keyboard->auto_shift_up_active = false;
    keyboard->scan_port_a_reference = NULL;
    keyboard->scan_port_b_reference = NULL;

    printf("Keyboard: Built lookup (%d identity + %d extended keys)\n",
           identity_count, ext_count);
}

// ============================================================================
// O(1) key position lookup
// ============================================================================

bool commodore_keyboard_find_key(const commodore_keyboard_t* keyboard,
                                 emu_key_t key, uint8_t* out_row, uint8_t* out_col) {
    if (!keyboard) return false;

    if (emu_key_is_identity(key)) {
        if (keyboard->key_direct_valid[key]) {
            *out_row = keyboard->key_direct_lookup[key].row;
            *out_col = keyboard->key_direct_lookup[key].col;
            return true;
        }
    } else {
        auto it = keyboard->key_ext_lookup.find(key);
        if (it != keyboard->key_ext_lookup.end()) {
            *out_row = it->second.row;
            *out_col = it->second.col;
            return true;
        }
    }
    return false;
}

// ============================================================================
// Key down / up — EmuKey based
// ============================================================================

void commodore_keyboard_key_down(commodore_keyboard_t* keyboard, emu_key_t key, bool shifted) {
    if (!keyboard) return;

    // Handle RESTORE key (special case — connects to NMI, not in matrix)
    if (key == EMUKEY_CBM_RESTORE) {
        keyboard->restore_key_pressed = true;
        return;
    }

    // Handle cursor LEFT: on C64/VIC-20 this is SHIFT + CRSR→ (no dedicated key).
    // Plus/4 and C128 have real CRSR← keys in the matrix.
    if (key == EMUKEY_LEFT &&
        keyboard->model != KEYBOARD_MODEL_PLUS4_C16 &&
        keyboard->model != KEYBOARD_MODEL_C128) {
        keyboard->auto_shift_left_active = true;
        commodore_keyboard_key_down(keyboard, EMUKEY_LSHIFT, false);
        commodore_keyboard_key_down(keyboard, EMUKEY_RIGHT, false);
        return;
    }

    // Handle cursor UP: on C64/VIC-20 this is SHIFT + CRSR↓ (no dedicated key).
    if (key == EMUKEY_UP &&
        keyboard->model != KEYBOARD_MODEL_PLUS4_C16 &&
        keyboard->model != KEYBOARD_MODEL_C128) {
        keyboard->auto_shift_up_active = true;
        commodore_keyboard_key_down(keyboard, EMUKEY_LSHIFT, false);
        commodore_keyboard_key_down(keyboard, EMUKEY_DOWN, false);
        return;
    }

    // Find the key position using optimised lookup
    uint8_t row, col;
    if (commodore_keyboard_find_key(keyboard, key, &row, &col)) {
        uint8_t row_bit = (keyboard->matrix_rows - 1) - row;
        uint8_t col_bit = (keyboard->matrix_cols - 1) - col;

        // Close the contact (key pressed)
        keyboard->row_open_contacts[row_bit] &= ~(1 << col_bit);
        keyboard->col_open_contacts[col_bit] &= ~(1 << row_bit);
    }
}

void commodore_keyboard_key_up(commodore_keyboard_t* keyboard, emu_key_t key, bool shifted) {
    if (!keyboard) return;

    // Handle RESTORE key
    if (key == EMUKEY_CBM_RESTORE) {
        keyboard->restore_key_pressed = false;
        return;
    }

    // Handle cursor LEFT release
    if (key == EMUKEY_LEFT &&
        keyboard->model != KEYBOARD_MODEL_PLUS4_C16 &&
        keyboard->model != KEYBOARD_MODEL_C128) {
        commodore_keyboard_key_up(keyboard, EMUKEY_RIGHT, false);
        if (keyboard->auto_shift_left_active) {
            keyboard->auto_shift_left_active = false;
            if (!keyboard->auto_shift_up_active) {
                commodore_keyboard_key_up(keyboard, EMUKEY_LSHIFT, false);
            }
        }
        return;
    }

    // Handle cursor UP release
    if (key == EMUKEY_UP &&
        keyboard->model != KEYBOARD_MODEL_PLUS4_C16 &&
        keyboard->model != KEYBOARD_MODEL_C128) {
        commodore_keyboard_key_up(keyboard, EMUKEY_DOWN, false);
        if (keyboard->auto_shift_up_active) {
            keyboard->auto_shift_up_active = false;
            if (!keyboard->auto_shift_left_active) {
                commodore_keyboard_key_up(keyboard, EMUKEY_LSHIFT, false);
            }
        }
        return;
    }

    // Find the key position using optimised lookup
    uint8_t row, col;
    if (commodore_keyboard_find_key(keyboard, key, &row, &col)) {
        uint8_t row_bit = (keyboard->matrix_rows - 1) - row;
        uint8_t col_bit = (keyboard->matrix_cols - 1) - col;

        // Open the contact (key released)
        keyboard->row_open_contacts[row_bit] |= (1 << col_bit);
        keyboard->col_open_contacts[col_bit] |= (1 << row_bit);
    }
}

// ============================================================================
// Utility
// ============================================================================

bool commodore_keyboard_is_special_key(emu_key_t key) {
    switch (key) {
        case EMUKEY_CBM_DEL:
        case EMUKEY_HOME:
        case EMUKEY_CBM_RUN_STOP:
        case EMUKEY_DOWN:
        case EMUKEY_RIGHT:
        case EMUKEY_LSHIFT:
        case EMUKEY_RSHIFT:
        case EMUKEY_LCTRL:
        case EMUKEY_RETURN:
        case EMUKEY_SPACE:
        case EMUKEY_CBM_COMMODORE:
        case EMUKEY_CBM_RESTORE:
        case EMUKEY_F1: case EMUKEY_F2: case EMUKEY_F3: case EMUKEY_F4:
        case EMUKEY_F5: case EMUKEY_F6: case EMUKEY_F7: case EMUKEY_F8:
        case EMUKEY_F9:            // HELP (C128)
        case EMUKEY_RALT:          // ALT (C128)
        case EMUKEY_ESCAPE:        // ESC (C128)
        case EMUKEY_CAPSLOCK:
        case EMUKEY_KP_ENTER:      // LINE FEED (C128)
            return true;
        default:
            return false;
    }
}

void commodore_keyboard_toggle_caps_lock(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;
    keyboard->caps_lock_active = !keyboard->caps_lock_active;
}

// ============================================================================
// I/O chip integration
// ============================================================================

void commodore_keyboard_update_matrix(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;
    // Matrix contact state is maintained directly by key_down/key_up.
    // This function exists for future use if explicit scanning is needed.
}

void commodore_keyboard_connect_ports(commodore_keyboard_t* keyboard,
                                      void* port_a, void* port_b) {
    if (!keyboard) return;
    keyboard->scan_port_a_reference = port_a;
    keyboard->scan_port_b_reference = port_b;
}

bool commodore_keyboard_is_row_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col) {
    if (!keyboard) return false;
    uint8_t row_mask = 1 << row;
    return !(keyboard->row_open_contacts[col] & row_mask);
}

bool commodore_keyboard_is_col_closed(commodore_keyboard_t* keyboard, uint8_t row, uint8_t col) {
    if (!keyboard) return false;
    uint8_t col_mask = 1 << col;
    return !(keyboard->col_open_contacts[row] & col_mask);
}

// ============================================================================
// Debug
// ============================================================================

void commodore_keyboard_print_matrix(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;

    uint8_t rows = keyboard->matrix_rows;
    uint8_t cols = keyboard->matrix_cols;

    printf("Commodore Keyboard Matrix State (%s, %dx%d):\n",
           keyboard_model_names[keyboard->model], rows, cols);
    printf("Row/Col |");
    for (int col = 0; col < cols; col++) printf(" %d ", col);
    printf("\n--------+");
    for (int col = 0; col < cols; col++) printf("---");
    printf("\n");

    for (int row = 0; row < rows; row++) {
        printf("  %2d    |", row);
        for (int col = 0; col < cols; col++) {
            bool contact_closed = !(keyboard->row_open_contacts[col] & (1 << row));
            printf(" %c ", contact_closed ? 'X' : 'O');
        }
        printf("\n");
    }

    printf("\nRow Contact States:\n");
    for (int r = 0; r < rows; r++) {
        printf("Row %2d: 0x%02X ", r, keyboard->row_open_contacts[r]);
    }
    printf("\nCol Contact States:\n");
    for (int c = 0; c < cols; c++) {
        printf("Col %2d: 0x%04X ", c, keyboard->col_open_contacts[c]);
    }
    printf("\n");
}

void commodore_keyboard_print_state(commodore_keyboard_t* keyboard) {
    if (!keyboard) return;

    printf("Keyboard State (%s %dx%d, scan: %s):\n",
           keyboard_model_names[keyboard->model],
           keyboard->matrix_rows, keyboard->matrix_cols,
           keyboard_scan_chip_names[keyboard->scan_chip]);
    printf("  RESTORE Key: %s\n", keyboard->restore_key_pressed ? "PRESSED" : "RELEASED");
    printf("  CAPS LOCK: %s\n", keyboard->caps_lock_active ? "ACTIVE" : "INACTIVE");
    printf("  Lookup: %d direct + %zu extended keys\n",
           [&]{ int c=0; for(int i=0;i<512;i++) if(keyboard->key_direct_valid[i]) c++; return c; }(),
           keyboard->key_ext_lookup.size());
}
