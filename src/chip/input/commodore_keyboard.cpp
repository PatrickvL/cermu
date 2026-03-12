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
        printf("ERROR: Keyboard matrix %dx%d exceeds maximum %dx%d\n",
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

    printf("Keyboard: Created %s %dx%d matrix (scan: %s)\n",
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
        row_open_contacts[r] = (r < rows) ? (uint8_t)((1 << cols) - 1) : 0x00;
    }
    for (int c = 0; c < MAX_KEYBOARD_COLS; c++) {
        col_open_contacts[c] = (c < cols) ? (uint16_t)((1 << rows) - 1) : 0x0000;
    }

    // ========================================================================
    // Build optimised EmuKey → {row, col} lookup from the keys[] table
    // ========================================================================
    memset(key_direct_valid, 0, sizeof(key_direct_valid));
    memset(key_direct_lookup, 0, sizeof(key_direct_lookup));
    key_ext_lookup.clear();

    if (!active_keys) {
        printf("ERROR: commodore_keyboard_t::reset called with no active keys set!\n");
        return;
    }

    int identity_count = 0;
    int ext_count = 0;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            emu_key_t key = active_keys[row * cols + col];

            // Skip markers and invalid entries
            if (emu_key_is_marker(key) || key == 0) continue;

            key_position_t pos = { (uint8_t)row, (uint8_t)col };

            if (emu_key_is_identity(key)) {
                // Identity-mapped key (0–511): direct array lookup
                if (!key_direct_valid[key]) {
                    key_direct_lookup[key] = pos;
                    key_direct_valid[key] = true;
                    identity_count++;
                }
            } else {
                // Emulator-specific key (EMUKEY_EMU_BASE+): hash map lookup
                if (key_ext_lookup.find(key) == key_ext_lookup.end()) {
                    key_ext_lookup[key] = pos;
                    ext_count++;
                }
            }
        }
    }

    // Initialize special state
    restore_key_pressed = false;
    caps_lock_active = false;
    auto_shift_left_active = false;
    auto_shift_up_active = false;
    scan_port_a_reference = NULL;
    scan_port_b_reference = NULL;

    printf("Keyboard: Built lookup (%d identity + %d extended keys)\n",
           identity_count, ext_count);
}

// ============================================================================
// O(1) key position lookup
// ============================================================================

bool commodore_keyboard_t::find_key(emu_key_t key, uint8_t* out_row, uint8_t* out_col) const {
    if (emu_key_is_identity(key)) {
        if (key_direct_valid[key]) {
            *out_row = key_direct_lookup[key].row;
            *out_col = key_direct_lookup[key].col;
            return true;
        }
    } else {
        auto it = key_ext_lookup.find(key);
        if (it != key_ext_lookup.end()) {
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

void commodore_keyboard_t::key_down(emu_key_t key, bool shifted) {
    // Handle RESTORE key (special case — connects to NMI, not in matrix)
    if (key == EMUKEY_CBM_RESTORE) {
        restore_key_pressed = true;
        return;
    }

    // Handle cursor LEFT: on C64/VIC-20 this is SHIFT + CRSR→ (no dedicated key).
    // Plus/4 and C128 have real CRSR← keys in the matrix.
    if (key == EMUKEY_LEFT &&
        model != KEYBOARD_MODEL_PLUS4_C16 &&
        model != KEYBOARD_MODEL_C128) {
        auto_shift_left_active = true;
        key_down(EMUKEY_LSHIFT, false);
        key_down(EMUKEY_RIGHT, false);
        return;
    }

    // Handle cursor UP: on C64/VIC-20 this is SHIFT + CRSR↓ (no dedicated key).
    if (key == EMUKEY_UP &&
        model != KEYBOARD_MODEL_PLUS4_C16 &&
        model != KEYBOARD_MODEL_C128) {
        auto_shift_up_active = true;
        key_down(EMUKEY_LSHIFT, false);
        key_down(EMUKEY_DOWN, false);
        return;
    }

    // Find the key position using optimised lookup
    uint8_t row, col;
    if (find_key(key, &row, &col)) {
        uint8_t row_bit = (matrix_rows - 1) - row;
        uint8_t col_bit = (matrix_cols - 1) - col;

        // Close the contact (key pressed)
        row_open_contacts[row_bit] &= ~(1 << col_bit);
        col_open_contacts[col_bit] &= ~(1 << row_bit);
    }
}

void commodore_keyboard_t::key_up(emu_key_t key, bool shifted) {
    // Handle RESTORE key
    if (key == EMUKEY_CBM_RESTORE) {
        restore_key_pressed = false;
        return;
    }

    // Handle cursor LEFT release
    if (key == EMUKEY_LEFT &&
        model != KEYBOARD_MODEL_PLUS4_C16 &&
        model != KEYBOARD_MODEL_C128) {
        key_up(EMUKEY_RIGHT, false);
        if (auto_shift_left_active) {
            auto_shift_left_active = false;
            if (!auto_shift_up_active) {
                key_up(EMUKEY_LSHIFT, false);
            }
        }
        return;
    }

    // Handle cursor UP release
    if (key == EMUKEY_UP &&
        model != KEYBOARD_MODEL_PLUS4_C16 &&
        model != KEYBOARD_MODEL_C128) {
        key_up(EMUKEY_DOWN, false);
        if (auto_shift_up_active) {
            auto_shift_up_active = false;
            if (!auto_shift_left_active) {
                key_up(EMUKEY_LSHIFT, false);
            }
        }
        return;
    }

    // Find the key position using optimised lookup
    uint8_t row, col;
    if (find_key(key, &row, &col)) {
        uint8_t row_bit = (matrix_rows - 1) - row;
        uint8_t col_bit = (matrix_cols - 1) - col;

        // Open the contact (key released)
        row_open_contacts[row_bit] |= (1 << col_bit);
        col_open_contacts[col_bit] |= (1 << row_bit);
    }
}

// ============================================================================
// Utility
// ============================================================================

bool commodore_keyboard_t::is_special_key(emu_key_t key) {
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

    printf("Commodore Keyboard Matrix State (%s, %dx%d):\n",
           keyboard_model_names[model], rows, cols);
    printf("Row/Col |");
    for (int col = 0; col < cols; col++) printf(" %d ", col);
    printf("\n--------+");
    for (int col = 0; col < cols; col++) printf("---");
    printf("\n");

    for (int row = 0; row < rows; row++) {
        printf("  %2d    |", row);
        for (int col = 0; col < cols; col++) {
            bool contact_closed = !(row_open_contacts[col] & (1 << row));
            printf(" %c ", contact_closed ? 'X' : 'O');
        }
        printf("\n");
    }

    printf("\nRow Contact States:\n");
    for (int r = 0; r < rows; r++) {
        printf("Row %2d: 0x%02X ", r, row_open_contacts[r]);
    }
    printf("\nCol Contact States:\n");
    for (int c = 0; c < cols; c++) {
        printf("Col %2d: 0x%04X ", c, col_open_contacts[c]);
    }
    printf("\n");
}

void commodore_keyboard_t::print_state() {
    printf("Keyboard State (%s %dx%d, scan: %s):\n",
           keyboard_model_names[model],
           matrix_rows, matrix_cols,
           keyboard_scan_chip_names[scan_chip]);
    printf("  RESTORE Key: %s\n", restore_key_pressed ? "PRESSED" : "RELEASED");
    printf("  CAPS LOCK: %s\n", caps_lock_active ? "ACTIVE" : "INACTIVE");
    printf("  Lookup: %d direct + %zu extended keys\n",
           [&]{ int c=0; for(int i=0;i<EMUKEY_EMU_BASE;i++) if(key_direct_valid[i]) c++; return c; }(),
           key_ext_lookup.size());
}
