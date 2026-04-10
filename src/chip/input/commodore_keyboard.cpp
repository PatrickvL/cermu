#include "core/cermu.hpp"
#include "chip/input/commodore_keyboard.hpp"
#include "utils/guest_key_chars.hpp"
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
    if (!config || !config->entries || config->num_entries == 0) return false;
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

    // Store active matrix entries and config tables
    active_entries = config->entries;
    num_entries = config->num_entries;
    host_bindings = config->host_bindings;
    num_host_bindings = config->num_host_bindings;
    char_overrides = config->char_overrides;
    num_char_overrides = config->num_char_overrides;

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

    // Build SDL_Keycode → {row, col} lookup.
    //
    // Two sources:
    //   1. Auto-derive: for ASCII-range normal characters in the matrix,
    //      the SDL_Keycode matches the character value.  For uppercase
    //      letters, also map the lowercase SDLK (which is what keydown fires).
    //   2. Explicit host bindings: for PUA keys and non-ASCII characters
    //      that need a specific host key mapping.
    key_lookup_.clear();

    if (!active_entries || num_entries == 0) {
        log_info("ERROR: commodore_keyboard_t::reset called with no active entries!\n");
        return;
    }

    // Build char32_t → {row, col} index from matrix entries
    std::unordered_map<char32_t, key_position_t> char_to_pos;
    for (int i = 0; i < num_entries; i++) {
        const KeyMatrixEntry& e = active_entries[i];
        if (e.normal != UKEY_NONE) {
            char_to_pos[e.normal] = { e.row, e.col };
        }
        // Also index shifted chars (needed for PET reversed digit/symbol convention).
        // Normal takes priority — if there's a conflict, first-write-wins from normal pass above.
        if (e.shifted != 0 && e.shifted != UKEY_NONE) {
            if (char_to_pos.find(e.shifted) == char_to_pos.end()) {
                char_to_pos[e.shifted] = { e.row, e.col };
            }
        }
    }

    int key_count = 0;

    // Auto-derive SDL_Keycode → {row, col} for ASCII-range characters
    for (auto& [ch, pos] : char_to_pos) {
        if (ch >= 'A' && ch <= 'Z') {
            // Uppercase letter: map the lowercase SDLK (keydown fires SDLK_a for 'A')
            SDL_Keycode sdl = (SDL_Keycode)(ch + 32);
            if (key_lookup_.find(sdl) == key_lookup_.end()) {
                key_lookup_[sdl] = pos;
                key_count++;
            }
        } else if (ch >= 0x20 && ch <= 0x7E) {
            // Other printable ASCII: SDLK == char value
            SDL_Keycode sdl = (SDL_Keycode)ch;
            if (key_lookup_.find(sdl) == key_lookup_.end()) {
                key_lookup_[sdl] = pos;
                key_count++;
            }
        }
        // Non-ASCII and PUA keys require explicit host bindings (below)
    }

    // Special case: RETURN key.  SDLK_RETURN == '\r' == 0x0D, which is
    // below the printable ASCII auto-derive range (0x20-0x7E).
    if (char_to_pos.find('\r') != char_to_pos.end()) {
        if (key_lookup_.find(SDLK_RETURN) == key_lookup_.end()) {
            key_lookup_[SDLK_RETURN] = char_to_pos['\r'];
            key_count++;
        }
    }

    // Apply explicit host bindings
    for (int i = 0; i < num_host_bindings; i++) {
        auto it = char_to_pos.find(host_bindings[i].guest_key);
        if (it != char_to_pos.end()) {
            if (key_lookup_.find(host_bindings[i].sdl_key) == key_lookup_.end()) {
                key_lookup_[host_bindings[i].sdl_key] = it->second;
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
        // Row/col are hardware bit positions — use directly
        row_open_contacts[row] &= ~(1 << col);
        col_open_contacts[col] &= ~(1 << row);
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
        // Row/col are hardware bit positions — use directly
        row_open_contacts[row] |= (1 << col);
        col_open_contacts[col] |= (1 << row);
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
