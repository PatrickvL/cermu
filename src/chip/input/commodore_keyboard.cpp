#include "core/cermu.hpp"
#include "chip/input/commodore_keyboard.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>

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

bool commodore_keyboard_t::init(const keyboard_matrix_config_t* config) {
    if (!config || !config->keys || !config->decode_tables || config->num_decode_tables == 0) return false;
    if (config->rows == 0 || config->cols == 0) return false;
    if (config->rows > MAX_KEYBOARD_ROWS || config->cols > MAX_KEYBOARD_COLS) {
        log_info("ERROR: Keyboard matrix %dx%d exceeds maximum %dx%d\n",
               config->rows, config->cols, MAX_KEYBOARD_ROWS, MAX_KEYBOARD_COLS);
        return false;
    }

    // Store model and matrix configuration
    model = config->model;
    scan_chip = config->scan_chip;
    matrix_rows = config->rows;
    matrix_cols = config->cols;

    // Store active matrix pointers
    active_keys = config->keys;
    num_decode_tables = config->num_decode_tables;
    decode_tables = config->decode_tables;

    // Initialize keyboard state
    reset();

    log_info("Keyboard: Created %s %dx%d matrix (scan: %s)\n",
           config->description ? config->description : keyboard_model_names[config->model],
           config->rows, config->cols,
           keyboard_scan_chip_names[config->scan_chip]);

    return true;
}

void commodore_keyboard_t::reset() {
    uint8_t rows = matrix_rows;
    uint8_t cols = matrix_cols;

    // Initialize all contacts as open (no keys pressed)
    for (int r = 0; r < MAX_KEYBOARD_ROWS; r++) {
        row_open_contacts[r] = (r < rows) ? (uint16_t)((1 << cols) - 1) : 0x0000;
    }
    for (int c = 0; c < MAX_KEYBOARD_COLS; c++) {
        col_open_contacts[c] = (c < cols) ? (uint16_t)((1 << rows) - 1) : 0x0000;
    }

    // Build SDL_Keycode → {row, col} lookup from the keys[] table
    key_lookup_.clear();

    if (!active_keys) {
        log_info("ERROR: commodore_keyboard_t::reset called with no active keys set!\n");
        return;
    }

    int key_count = 0;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            SDL_Keycode key = active_keys[row * cols + col];

            // Skip markers and invalid entries
            if (cermu_key_is_marker(key) || key == 0) continue;

            key_position_t pos = { (uint8_t)row, (uint8_t)col };

            // First-write-wins: don't overwrite duplicate key positions
            if (key_lookup_.find(key) == key_lookup_.end()) {
                key_lookup_[key] = pos;
                key_count++;
            }
        }
    }

    // Initialize special state
    restore_key_pressed = false;
    caps_lock_active = false;
    auto_shift_left_active = false;
    auto_shift_up_active = false;
    auto_shift_fkey_count = 0;
    scan_port_a_reference = NULL;
    scan_port_b_reference = NULL;

    log_info("Keyboard: Built lookup (%d keys)\n", key_count);
}

// ============================================================================
// O(1) key position lookup
// ============================================================================

bool commodore_keyboard_t::find_key(SDL_Keycode key, uint8_t* out_row, uint8_t* out_col) const {
    auto it = key_lookup_.find(key);
    if (it != key_lookup_.end()) {
        *out_row = it->second.row;
        *out_col = it->second.col;
        return true;
    }
    return false;
}

// ============================================================================
// Auto-shift helpers
// ============================================================================

SDL_Keycode commodore_keyboard_t::resolve_fkey_physical(SDL_Keycode key) const {
    // Even F-keys → physical odd F-key that needs SHIFT.
    // Mapping varies by system: C64/VIC-20/C128 have F1/F3/F5/F7 in matrix;
    // C16/Plus4 has F1/F2/F3/F7 (HELP).
    switch (key) {
        case SDLK_F2:
            return SDLK_F1;  // Same on all systems
        case SDLK_F4:
            return (model == KEYBOARD_MODEL_PLUS4_C16) ? SDLK_F1 : SDLK_F3;
        case SDLK_F5:
            // C16/Plus4: F5 = SHIFT + F2 (F2 is in its matrix).
            // Others: F5 is a physical key — no auto-shift needed.
            return (model == KEYBOARD_MODEL_PLUS4_C16) ? SDLK_F2 : CERMU_KEY_NONE;
        case SDLK_F6:
            return (model == KEYBOARD_MODEL_PLUS4_C16) ? SDLK_F3 : SDLK_F5;
        case SDLK_F8:
            return SDLK_F7;  // Same on all systems
        default:
            return CERMU_KEY_NONE;
    }
}

// ============================================================================
// Key down / up
// ============================================================================

void commodore_keyboard_t::key_down(SDL_Keycode key, bool shifted) {
    // Handle RESTORE key (special case — connects to NMI, not in matrix)
    if (key == CERMU_KEY_CBM_RESTORE) {
        restore_key_pressed = true;
        return;
    }

    // Handle cursor LEFT: on C64/VIC-20 this is SHIFT + CRSR→ (no dedicated key).
    // Plus/4 and C128 have real CRSR← keys in the matrix.
    if (key == SDLK_LEFT && needs_cursor_auto_shift()) {
        auto_shift_left_active = true;
        key_down(SDLK_LSHIFT, false);
        key_down(SDLK_RIGHT, false);
        return;
    }

    // Handle cursor UP: on C64/VIC-20 this is SHIFT + CRSR↓ (no dedicated key).
    if (key == SDLK_UP && needs_cursor_auto_shift()) {
        auto_shift_up_active = true;
        key_down(SDLK_LSHIFT, false);
        key_down(SDLK_DOWN, false);
        return;
    }

    // Even F-keys → SHIFT + physical odd F-key.
    {
        SDL_Keycode physical = resolve_fkey_physical(key);
        if (physical != CERMU_KEY_NONE) {
            auto_shift_fkey_count++;
            key_down(SDLK_LSHIFT, false);
            key_down(physical, false);
            return;
        }
    }

    // Find the key position using optimised lookup
    uint8_t row, col;
    if (find_key(key, &row, &col)) {
        uint8_t row_bit = (matrix_rows - 1) - row;
        // All matrices use bit-reversed layout: array index 0 = highest
        // hardware bit.  Undo the reversal to get the hardware bit position.
        // Extended columns (8+, e.g. C128 numpad via VIC-IIe $D02F) are
        // stored at their natural index and not reversed.
        uint8_t col_bit = (col < 8) ? (7 - col) : col;

        // Close the contact (key pressed)
        row_open_contacts[row_bit] &= ~(1 << col_bit);
        col_open_contacts[col_bit] &= ~(1 << row_bit);
    }
}

void commodore_keyboard_t::key_up(SDL_Keycode key, bool shifted) {
    // Handle RESTORE key
    if (key == CERMU_KEY_CBM_RESTORE) {
        restore_key_pressed = false;
        return;
    }

    // Handle cursor LEFT release
    if (key == SDLK_LEFT && needs_cursor_auto_shift()) {
        key_up(SDLK_RIGHT, false);
        if (auto_shift_left_active) {
            auto_shift_left_active = false;
            if (!any_auto_shift_active()) {
                key_up(SDLK_LSHIFT, false);
            }
        }
        return;
    }

    // Handle cursor UP release
    if (key == SDLK_UP && needs_cursor_auto_shift()) {
        key_up(SDLK_DOWN, false);
        if (auto_shift_up_active) {
            auto_shift_up_active = false;
            if (!any_auto_shift_active()) {
                key_up(SDLK_LSHIFT, false);
            }
        }
        return;
    }

    // Even F-key release — uses same mapping as key_down
    {
        SDL_Keycode physical = resolve_fkey_physical(key);
        if (physical != CERMU_KEY_NONE) {
            key_up(physical, false);
            if (auto_shift_fkey_count > 0) {
                auto_shift_fkey_count--;
                if (!any_auto_shift_active()) {
                    key_up(SDLK_LSHIFT, false);
                }
            }
            return;
        }
    }

    // Find the key position using optimised lookup
    uint8_t row, col;
    if (find_key(key, &row, &col)) {
        uint8_t row_bit = (matrix_rows - 1) - row;
        uint8_t col_bit = (col < 8) ? (7 - col) : col;

        // Open the contact (key released)
        row_open_contacts[row_bit] |= (1 << col_bit);
        col_open_contacts[col_bit] |= (1 << row_bit);
    }
}

// ============================================================================
// Utility
// ============================================================================

bool commodore_keyboard_t::is_special_key(SDL_Keycode key) {
    switch (key) {
        case CERMU_KEY_CBM_DEL:
        case SDLK_HOME:
        case CERMU_KEY_CBM_RUN_STOP:
        case SDLK_DOWN:
        case SDLK_RIGHT:
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
        case SDLK_LCTRL:
        case SDLK_RETURN:
        case SDLK_SPACE:
        case CERMU_KEY_CBM_COMMODORE:
        case CERMU_KEY_CBM_RESTORE:
        case SDLK_F1: case SDLK_F2: case SDLK_F3: case SDLK_F4:
        case SDLK_F5: case SDLK_F6: case SDLK_F7: case SDLK_F8:
        case SDLK_F9:            // HELP (C128)
        case SDLK_RALT:          // ALT (C128)
        case SDLK_ESCAPE:        // ESC (C128)
        case SDLK_CAPSLOCK:
        case SDLK_KP_ENTER:      // LINE FEED (C128)
            return true;
        default:
            return false;
    }
}

void commodore_keyboard_t::toggle_caps_lock() {
    caps_lock_active = !caps_lock_active;
}

// ============================================================================
// I/O chip integration
// ============================================================================

void commodore_keyboard_t::update_matrix() {
    // Matrix contact state is maintained directly by key_down/key_up.
    // This function exists for future use if explicit scanning is needed.
}

void commodore_keyboard_t::connect_ports(void* port_a, void* port_b) {
    scan_port_a_reference = port_a;
    scan_port_b_reference = port_b;
}

bool commodore_keyboard_t::is_row_closed(uint8_t row, uint8_t col) {
    uint8_t row_mask = 1 << row;
    return !(row_open_contacts[col] & row_mask);
}

bool commodore_keyboard_t::is_col_closed(uint8_t row, uint8_t col) {
    uint8_t col_mask = 1 << col;
    return !(col_open_contacts[row] & col_mask);
}

// ============================================================================
// Debug
// ============================================================================

void commodore_keyboard_t::print_matrix() {
    uint8_t rows = matrix_rows;
    uint8_t cols = matrix_cols;

    log_info("Commodore Keyboard Matrix State (%s, %dx%d):\n",
           keyboard_model_names[model], rows, cols);
    log_info("Row/Col |");
    for (int col = 0; col < cols; col++) log_info(" %d ", col);
    log_info("\n--------+");
    for (int col = 0; col < cols; col++) log_info("---");
    log_info("\n");

    for (int row = 0; row < rows; row++) {
        log_info("  %2d    |", row);
        for (int col = 0; col < cols; col++) {
            bool contact_closed = !(row_open_contacts[col] & (1 << row));
            log_info(" %c ", contact_closed ? 'X' : 'O');
        }
        log_info("\n");
    }

    log_info("\nRow Contact States:\n");
    for (int r = 0; r < rows; r++) {
        log_info("Row %2d: 0x%04X ", r, row_open_contacts[r]);
    }
    log_info("\nCol Contact States:\n");
    for (int c = 0; c < cols; c++) {
        log_info("Col %2d: 0x%04X ", c, col_open_contacts[c]);
    }
    log_info("\n");
}

void commodore_keyboard_t::print_state() {
    log_info("Keyboard State (%s %dx%d, scan: %s):\n",
           keyboard_model_names[model],
           matrix_rows, matrix_cols,
           keyboard_scan_chip_names[scan_chip]);
    log_info("  RESTORE Key: %s\n", restore_key_pressed ? "PRESSED" : "RELEASED");
    log_info("  CAPS LOCK: %s\n", caps_lock_active ? "ACTIVE" : "INACTIVE");
    log_info("  Lookup: %zu keys\n", key_lookup_.size());
}
