#include "core/input/keyboard_mapper.hpp"
#include "core/input/emu_key_sdl_map.hpp"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <ctype.h>

// ============================================================================
// KeyboardMapper implementation
// ============================================================================

KeyboardMapper::KeyboardMapper()
    : keyboard_(nullptr)
    , model_(KEYBOARD_MODEL_UNKNOWN)
    , emu_modifier_key_(SDLK_RALT)
    , emu_modifier_held_(false)
    , host_lshift_held_(false)
    , host_rshift_held_(false)
    , host_ctrl_held_(false)
    , host_cbm_held_(false)
    , text_input_enabled_(true)
    , has_pending_key_(false)
    , matrix_rows_(0)
    , matrix_cols_(0)
{
    // Clear character map
    for (int i = 0; i < 128; i++) {
        char_map_[i] = GuestKeyAction();
    }

    pending_key_ = {};
}

KeyboardMapper::~KeyboardMapper() {
    // We don't own the keyboard pointer
}

// ============================================================================
// Configuration
// ============================================================================

void KeyboardMapper::set_guest_keyboard(commodore_keyboard_t* keyboard) {
    keyboard_ = keyboard;
    if (keyboard) {
        model_ = keyboard->model;
        matrix_rows_ = keyboard->matrix_rows;
        matrix_cols_ = keyboard->matrix_cols;
    }
}

void KeyboardMapper::build_character_map_from_matrix(const keyboard_matrix_config_t* config) {
    if (!config || !config->keys || !config->decode_tables || config->num_decode_tables == 0) return;

    model_ = config->model;
    matrix_rows_ = config->rows;
    matrix_cols_ = config->cols;

    // Clear existing character map
    for (int i = 0; i < 128; i++) {
        char_map_[i] = GuestKeyAction();
    }

    uint8_t rows = config->rows;
    uint8_t cols = config->cols;

    // Build the character map from PETSCII decode tables.
    //
    // Each decode table maps every matrix position to a PETSCII code for
    // a specific modifier combination (KEYMOD_NONE, KEYMOD_SHIFT, etc.).
    // We convert each PETSCII code to a host character via
    // petscii_to_host_char(), then store the reverse mapping:
    //   char_map_[host_char] = { row, col, table.modifiers }
    //
    // Tables are processed in order.  First-write-wins: the first table
    // that maps a host character claims that char_map_ slot.  This means
    // the KEYMOD_NONE (unshifted) table should come first, followed by
    // KEYMOD_SHIFT, then KEYMOD_CBM, etc.
    //
    // After the decode table loop, a position-accurate letter fixup copies
    // the unshifted 'A'-'Z' mappings to 'a'-'z'.  This is necessary
    // because the Commodore default character set shows UPPERCASE for
    // unshifted keys.  Without the fixup, typing 'a' would force SHIFT
    // (from the PETSCII $C1 shifted-table entry), and the VIC-20/C64
    // KERNAL would produce a graphics character instead of a letter.
    //
    // Commodore-specific characters are handled automatically:
    //   £ ($5C) → petscii_to_host_char → '^' → char_map_['^'] = KEYMOD_NONE
    //   ↑ ($5E) → petscii_to_host_char → '|' → char_map_['|'] = KEYMOD_NONE
    //   ← ($5F) → petscii_to_host_char → '\\' → char_map_['\\'] = KEYMOD_NONE
    //   π ($DE) → petscii_to_host_char → '~' → char_map_['~'] = KEYMOD_SHIFT

    for (int t = 0; t < config->num_decode_tables; t++) {
        const keyboard_decode_table_t& table = config->decode_tables[t];
        if (!table.petscii) continue;

        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                petscii_t p = table.petscii[row * cols + col];
                if (p == 0) continue;

                char host_char = petscii_to_host_char(p);
                if (host_char == 0) continue;

                unsigned char uc = (unsigned char)host_char;
                if (uc >= 128) continue;

                // First-write-wins: don't overwrite existing mappings
                if (!char_map_[uc].valid) {
                    char_map_[uc] = GuestKeyAction(row, col, table.modifiers);
                }
            }
        }
    }

    // Position-accurate letter fixup: map both 'a' and 'A' to KEYMOD_NONE.
    //
    // The Commodore default character set (uppercase/graphics) shows uppercase
    // letters for unshifted keypresses.  Shift+letter produces a graphics
    // character, NOT a lowercase letter.  Lowercase letters only appear after
    // toggling to the alternate charset via C=+SHIFT.
    //
    // The decode table loop above creates:
    //   char_map_['A'] = { A-pos, KEYMOD_NONE }   ← from unshifted table ($41)
    //   char_map_['a'] = { A-pos, KEYMOD_SHIFT }   ← from shifted table ($C1)
    //
    // The 'a' → KEYMOD_SHIFT mapping is character-accurate PETSCII but causes
    // inject_press to FORCE SHIFT for every unshifted host letter, producing
    // graphics characters instead of letters.  Override 'a'-'z' with the
    // position-accurate KEYMOD_NONE mapping from 'A'-'Z':
    for (int i = 0; i < 26; i++) {
        if (char_map_['A' + i].valid) {
            char_map_['a' + i] = char_map_['A' + i];
        }
    }

    // Underscore fixup: host '_' (Shift+minus) → PETSCII $A4 (▁).
    //
    // The Commodore graphics character $A4 (LOWER ONE EIGHTH BLOCK) is the
    // closest visual match to an underscore.  It's produced by C= + @, i.e.
    // the '@' key position with KEYMOD_CBM.  Since $A4 lives in the $A0–$BF
    // graphics range, it doesn't appear in the standard KEYMOD_NONE/SHIFT
    // decode tables and can't be picked up by the loop above.
    //
    // We derive the position from char_map_['@'] (already populated by the
    // unshifted decode table) and override the modifier to KEYMOD_CBM.
    if (!char_map_['_'].valid && char_map_['@'].valid) {
        char_map_['_'] = GuestKeyAction(char_map_['@'].row, char_map_['@'].col, KEYMOD_CBM);
    }

    // Cache modifier key positions using the keys[] table
    shift_left_pos_ = GuestKeyAction();
    shift_right_pos_ = GuestKeyAction();
    cbm_key_pos_ = GuestKeyAction();
    ctrl_key_pos_ = GuestKeyAction();

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            emu_key_t key = config->keys[row * cols + col];
            if (key == EMUKEY_LSHIFT) {
                shift_left_pos_ = GuestKeyAction(row, col, KEYMOD_NONE);
            }
            if (key == EMUKEY_RSHIFT) {
                shift_right_pos_ = GuestKeyAction(row, col, KEYMOD_NONE);
            }
            if (key == EMUKEY_LGUI) {
                cbm_key_pos_ = GuestKeyAction(row, col, KEYMOD_NONE);
            }
            if (key == EMUKEY_LCTRL) {
                ctrl_key_pos_ = GuestKeyAction(row, col, KEYMOD_NONE);
            }
        }
    }

    printf("KeyboardMapper: Built character map for %s (%d×%d matrix)\n",
           config->description ? config->description : "unknown",
           rows, cols);

    // Count valid mappings for debug
    int count = 0;
    for (int i = 0; i < 128; i++) {
        if (char_map_[i].valid) count++;
    }
    printf("KeyboardMapper: %d character mappings active\n", count);
}

void KeyboardMapper::set_emulator_modifier(SDL_Keycode key) {
    emu_modifier_key_ = key;
}

void KeyboardMapper::add_synthetic_mapping(SDL_Keycode trigger, GuestKeyAction action, const char* description) {
    SyntheticKeyMapping mapping;
    mapping.trigger_key = trigger;
    mapping.action = action;
    mapping.description = description;

    synthetic_map_[trigger] = mapping;
    synthetic_mappings_.push_back(mapping);
}

void KeyboardMapper::add_direct_mapping(SDL_Keycode host_key, GuestKeyAction action) {
    direct_map_[host_key] = action;
}

void KeyboardMapper::add_char_mapping(char c, GuestKeyAction action) {
    unsigned char uc = (unsigned char)c;
    if (uc < 128) {
        char_map_[uc] = action;
    }
}

void KeyboardMapper::register_default_synthetic_mappings() {
    if (!keyboard_) return;

    // Use the optimised lookup to find matrix positions for guest-specific keys
    auto find_in_matrix = [this](emu_key_t target_key) -> GuestKeyAction {
        if (!keyboard_) return GuestKeyAction();
        uint8_t row, col;
        if (keyboard_->find_key(target_key, &row, &col)) {
            return GuestKeyAction(row, col, KEYMOD_NONE);
        }
        return GuestKeyAction();
    };

    // Common Commodore synthetic mappings
    // Emulator modifier (Right Alt) + key → guest-specific key

    // RUN/STOP — Escape is intuitive (Escape → stop)
    // But Escape is already used for C128 ESC key, so we also provide
    // the synthetic mapping for systems where Tab = RUN/STOP
    GuestKeyAction run_stop = find_in_matrix(EMUKEY_CBM_RUN_STOP);
    if (run_stop.valid) {
        add_synthetic_mapping(SDLK_ESCAPE, run_stop, "RUN/STOP");
    }

    // RESTORE — also available as emu+R
    GuestKeyAction restore_action;
    restore_action.valid = true;
    restore_action.row = 0; restore_action.col = 0;
    restore_action.modifiers = KEYMOD_NONE;
    // RESTORE is special — it's not in the matrix, it triggers NMI
    // We handle it through the existing keyboard->restore_key_pressed flag
    add_synthetic_mapping(SDLK_r, restore_action, "RESTORE (NMI)");

    // Commodore key (C= key)
    GuestKeyAction c_key = find_in_matrix(EMUKEY_CBM_COMMODORE);
    if (c_key.valid) {
        add_synthetic_mapping(SDLK_c, c_key, "Commodore (C=) key");
    }

    // CTRL key (in its C64 role — color selection etc.)
    GuestKeyAction ctrl = find_in_matrix(EMUKEY_LCTRL);
    if (ctrl.valid) {
        add_synthetic_mapping(SDLK_x, ctrl, "CTRL (C64)");
    }

    // CLR/HOME
    GuestKeyAction home = find_in_matrix(EMUKEY_HOME);
    if (home.valid) {
        add_synthetic_mapping(SDLK_h, home, "CLR/HOME");
    }

    // INST/DEL
    GuestKeyAction del = find_in_matrix(EMUKEY_CBM_DEL);
    if (del.valid) {
        add_synthetic_mapping(SDLK_d, del, "INST/DEL");
    }

    printf("KeyboardMapper: Registered %zu synthetic mappings (modifier: %s)\n",
           synthetic_mappings_.size(),
           SDL_GetKeyName(emu_modifier_key_));
}

// ============================================================================
// Event processing
// ============================================================================

bool KeyboardMapper::process_key_down(SDL_Keycode sym, SDL_Scancode scancode,
                                       uint16_t mod, bool repeat) {
    if (!keyboard_) return false;

    // Ignore key repeats — the matrix contact is already closed
    if (repeat) return true;

    // Track host modifier state
    if (sym == SDLK_LSHIFT) host_lshift_held_ = true;
    if (sym == SDLK_RSHIFT) host_rshift_held_ = true;
    if (sym == SDLK_LCTRL || sym == SDLK_RCTRL) {
        host_ctrl_held_ = true;
    }
    if (sym == SDLK_LGUI) {
        host_cbm_held_ = true;
    }

    // ====================================================================
    // Modifier reconciliation: detect "stuck" modifiers from lost key-up
    // ====================================================================
    // The SDL `mod` field contains the modifier state BEFORE this key event.
    // If our tracking variable says a modifier is held but SDL disagrees,
    // the key-up was lost (e.g., Linux WM intercepted LGUI / Super).
    // Open the stuck matrix contact and reset tracking — but only when the
    // current key is NOT the modifier itself (its own key-down legitimately
    // sets tracking before we get here, and `mod` doesn't include it yet).
    if (host_cbm_held_ && !(mod & KMOD_LGUI) && sym != SDLK_LGUI) {
        host_cbm_held_ = false;
        if (cbm_key_pos_.valid) {
            open_contact(cbm_key_pos_.row, cbm_key_pos_.col);
        }
    }
    if (host_ctrl_held_ && !(mod & (KMOD_LCTRL | KMOD_RCTRL))
        && sym != SDLK_LCTRL && sym != SDLK_RCTRL) {
        host_ctrl_held_ = false;
        if (ctrl_key_pos_.valid) {
            open_contact(ctrl_key_pos_.row, ctrl_key_pos_.col);
        }
    }
    if (host_lshift_held_ && !(mod & KMOD_LSHIFT) && sym != SDLK_LSHIFT) {
        host_lshift_held_ = false;
        if (shift_left_pos_.valid) open_contact(shift_left_pos_.row, shift_left_pos_.col);
    }
    if (host_rshift_held_ && !(mod & KMOD_RSHIFT) && sym != SDLK_RSHIFT) {
        host_rshift_held_ = false;
        if (shift_right_pos_.valid) open_contact(shift_right_pos_.row, shift_right_pos_.col);
    }

    // Check if this is the emulator modifier key itself
    if (sym == emu_modifier_key_) {
        emu_modifier_held_ = true;
        return true;  // Consume — don't pass to guest
    }

    // ====================================================================
    // Layer 3: Emulator modifier combos
    // ====================================================================
    if (emu_modifier_held_) {
        auto it = synthetic_map_.find(sym);
        if (it != synthetic_map_.end()) {
            const SyntheticKeyMapping& mapping = it->second;

            // Special case: RESTORE is not a matrix key
            if (strcmp(mapping.description, "RESTORE (NMI)") == 0) {
                keyboard_->restore_key_pressed = true;
                ActiveInjection inj;
                inj.action = mapping.action;
                inj.host_scancode = scancode;
                inj.from_text_input = false;
                active_injections_[scancode] = inj;
                return true;
            }

            inject_press(mapping.action, scancode, false);
            return true;
        }
        return false;  // Unrecognized combo — let it through
    }

    // ====================================================================
    // Layer 2: Direct key mapping (non-printable keys, modifiers)
    // ====================================================================

    // Check explicit direct mapping overrides first
    auto direct_it = direct_map_.find(sym);
    if (direct_it != direct_map_.end()) {
        inject_press(direct_it->second, scancode, false);
        return true;
    }

    // Modifier keys — convert to EmuKey and pass to commodore_keyboard
    if (is_modifier_key(sym)) {
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(sym);
        if (ek != EMUKEY_NONE) {
            keyboard_->key_down(ek, false);
        }
        return true;
    }

    // Non-printable keys — convert to EmuKey and pass through
    if (!is_printable_key(sym)) {
        // Special case: backtick with shift → treat as printable so TEXTINPUT "~"
        // can be mapped (e.g., to π on C64/VIC-20). Without shift, backtick
        // falls through to key_down which handles RESTORE.
        if (sym == SDLK_BACKQUOTE && (mod & (KMOD_LSHIFT | KMOD_RSHIFT))) {
            // Fall through to the printable key / text input path below
        } else {
            emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(sym);
            if (ek != EMUKEY_NONE) {
                keyboard_->key_down(ek, false);
            }
            return true;
        }
    }

    // ====================================================================
    // Guest modifier pass-through: CBM + key or CTRL + key
    // ====================================================================
    // When the host user holds LGUI (→ Commodore key) or LCTRL (→ CTRL)
    // and presses a printable key, we bypass the TEXTINPUT path entirely.
    // The modifier contact is already closed in the matrix from its own
    // key_down event.  We just close the printable key's contact and let
    // the guest KERNAL see the combined modifier + key state.
    //
    // This enables graphics characters (C= + letter), colour codes
    // (CTRL + digit), and other modifier-specific outputs that have no
    // host TEXTINPUT equivalent.
    //
    // The inject_press call uses a modifier bitmask built from the ACTUAL
    // host modifier state, so it won't force or suppress anything — the
    // modifiers are already down.
    if ((host_cbm_held_ || host_ctrl_held_) && is_printable_key(sym)) {
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(sym);
        if (ek != EMUKEY_NONE) {
            uint8_t row, col;
            if (keyboard_->find_key(ek, &row, &col)) {
                // Build modifier mask from actual host state — no forcing/suppressing
                uint8_t mods = KEYMOD_NONE;
                if (host_cbm_held_)   mods |= KEYMOD_CBM;
                if (host_ctrl_held_)  mods |= KEYMOD_CTRL;
                if (host_shift_held()) mods |= KEYMOD_SHIFT;
                inject_press(GuestKeyAction(row, col, mods), scancode, false);
                return true;
            }
        }
    }

    // ====================================================================
    // Layer 1: Printable keys — use character-based mapping via TEXTINPUT
    // ====================================================================
    if (text_input_enabled_) {
        // Record this as a pending key — we'll map it when TEXTINPUT arrives.
        // If TEXTINPUT doesn't arrive (e.g., text input disabled by OS),
        // the key won't be mapped. This is intentional — the TEXTINPUT path
        // gives us the correct character for the host layout.
        pending_key_.sym = sym;
        pending_key_.scancode = scancode;
        pending_key_.mod = mod;
        has_pending_key_ = true;
        return true;
    }

    // Fallback: text input disabled, use direct SDL keycode → EmuKey mapping
    emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(sym);
    if (ek != EMUKEY_NONE) {
        keyboard_->key_down(ek, false);
    }
    return true;
}

bool KeyboardMapper::process_key_up(SDL_Keycode sym, SDL_Scancode scancode, uint16_t mod) {
    if (!keyboard_) return false;

    // Track host modifier state
    if (sym == SDLK_LSHIFT) {
        host_lshift_held_ = (mod & KMOD_LSHIFT) != 0;
    }
    if (sym == SDLK_RSHIFT) {
        host_rshift_held_ = (mod & KMOD_RSHIFT) != 0;
    }
    if (sym == SDLK_LCTRL || sym == SDLK_RCTRL) {
        host_ctrl_held_ = (mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0;
    }
    if (sym == SDLK_LGUI) {
        host_cbm_held_ = (mod & KMOD_LGUI) != 0;
    }

    // Emulator modifier release
    if (sym == emu_modifier_key_) {
        emu_modifier_held_ = false;
        return true;
    }

    // Check if we have an active injection for this scancode
    auto it = active_injections_.find(scancode);
    if (it != active_injections_.end()) {
        // Special case: RESTORE
        if (it->second.action.row == 0 && it->second.action.col == 0 &&
            !it->second.action.valid) {
            // This was a RESTORE injection via synthetic mapping
        }
        // Check if this was a RESTORE synthetic (we stored the injection)
        // RESTORE is handled through keyboard_->restore_key_pressed
        bool is_restore = false;
        if (emu_modifier_held_ || it->second.from_text_input == false) {
            // Check synthetic map
            auto syn_it = synthetic_map_.find(sym);
            if (syn_it != synthetic_map_.end() &&
                strcmp(syn_it->second.description, "RESTORE (NMI)") == 0) {
                keyboard_->restore_key_pressed = false;
                is_restore = true;
            }
        }

        if (!is_restore) {
            release_injection(it->second);
        }
        active_injections_.erase(it);
        return true;
    }

    // Clear pending key if it matches
    if (has_pending_key_ && pending_key_.scancode == scancode) {
        has_pending_key_ = false;
    }

    // Modifier keys — convert to EmuKey and pass through
    if (is_modifier_key(sym)) {
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(sym);
        if (ek != EMUKEY_NONE) {
            keyboard_->key_up(ek, false);
        }
        return true;
    }

    // Non-printable keys — convert to EmuKey and pass through
    if (!is_printable_key(sym)) {
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(sym);
        if (ek != EMUKEY_NONE) {
            keyboard_->key_up(ek, false);
        }
        return true;
    }

    // Fallback: direct release if text input is disabled
    if (!text_input_enabled_) {
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(sym);
        if (ek != EMUKEY_NONE) {
            keyboard_->key_up(ek, false);
        }
    }

    return true;
}

bool KeyboardMapper::process_text_input(const char* text) {
    if (!keyboard_ || !text || !text_input_enabled_) return false;

    // Only process text input if we have a pending printable key from a
    // preceding SDL_KEYDOWN that was identified as printable by is_printable_key().
    // This guards against spurious SDL_TEXTINPUT events that some Linux
    // input method frameworks may generate for non-printable keys (cursor keys,
    // function keys, etc.). Without this guard, such events would inject matrix
    // contacts with SDL_SCANCODE_UNKNOWN that are never tracked and never
    // released — causing stuck keys.
    if (!has_pending_key_) return false;

    // Process each character in the text input
    // Usually just one character, but SDL can batch them
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];

        // Only handle printable ASCII for now
        unsigned char uc = (unsigned char)c;
        if (uc >= 128) continue;

        const GuestKeyAction& action = char_map_[uc];
        if (!action.valid) {
            // No mapping for this character — try fallback alternatives.
            // 1. Uppercase/lowercase variant (host layouts vary)
            char alt = 0;
            if (c >= 'a' && c <= 'z') alt = c - 32;  // try uppercase
            else if (c >= 'A' && c <= 'Z') alt = c + 32;  // try lowercase

            if (alt > 0 && char_map_[(unsigned char)alt].valid) {
                SDL_Scancode sc = has_pending_key_ ? pending_key_.scancode : SDL_SCANCODE_UNKNOWN;
                inject_press(char_map_[(unsigned char)alt], sc, true);
                has_pending_key_ = false;
                continue;
            }

            // 2. Curly-brace family: { and } don't exist on Commodore
            //    keyboards.  Map to ( and ) — the visual pairing is
            //    closer than [ ], and using the shifted variant signals
            //    that the host Shift key was involved.
            char bracket_alt = 0;
            if (c == '{') bracket_alt = '(';
            else if (c == '}') bracket_alt = ')';

            if (bracket_alt && char_map_[(unsigned char)bracket_alt].valid) {
                SDL_Scancode sc = has_pending_key_ ? pending_key_.scancode : SDL_SCANCODE_UNKNOWN;
                inject_press(char_map_[(unsigned char)bracket_alt], sc, true);
                has_pending_key_ = false;
                continue;
            }

            // No mapping at all — drop the character
            has_pending_key_ = false;
            continue;
        }

        // Inject the guest key action
        SDL_Scancode sc = has_pending_key_ ? pending_key_.scancode : SDL_SCANCODE_UNKNOWN;
        inject_press(action, sc, true);
        has_pending_key_ = false;
    }

    return true;
}

// ============================================================================
// State management
// ============================================================================

void KeyboardMapper::release_all() {
    if (!keyboard_) return;

    // Release all active injections
    for (auto& pair : active_injections_) {
        release_injection(pair.second);
    }
    active_injections_.clear();

    // Release RESTORE if active
    keyboard_->restore_key_pressed = false;

    // Open modifier matrix contacts — these may have been closed via
    // commodore_keyboard_key_down (Layer 2) and won't be in active_injections_.
    if (shift_left_pos_.valid) open_contact(shift_left_pos_.row, shift_left_pos_.col);
    if (shift_right_pos_.valid) open_contact(shift_right_pos_.row, shift_right_pos_.col);
    if (cbm_key_pos_.valid) open_contact(cbm_key_pos_.row, cbm_key_pos_.col);
    if (ctrl_key_pos_.valid) open_contact(ctrl_key_pos_.row, ctrl_key_pos_.col);

    // Clear modifier tracking
    emu_modifier_held_ = false;
    host_lshift_held_ = false;
    host_rshift_held_ = false;
    host_ctrl_held_ = false;
    host_cbm_held_ = false;
    has_pending_key_ = false;
}

void KeyboardMapper::reset_state() {
    release_all();
    // Reset the underlying keyboard matrix
    keyboard_->reset();
}

// ============================================================================
// Queries
// ============================================================================

GuestKeyAction KeyboardMapper::lookup_character(char c) const {
    unsigned char uc = (unsigned char)c;
    if (uc < 128) {
        return char_map_[uc];
    }
    return GuestKeyAction();
}

// ============================================================================
// Internal helpers
// ============================================================================

void KeyboardMapper::inject_press(const GuestKeyAction& action, SDL_Scancode host_scancode, bool from_text) {
    if (!keyboard_ || !action.valid) return;

    ActiveInjection inj;
    inj.action = action;
    inj.host_scancode = host_scancode;
    inj.from_text_input = from_text;
    inj.forced_modifiers = 0;
    inj.suppressed_modifiers = 0;

    // Handle modifier state manipulation for each modifier type.
    // For each modifier bit in action.modifiers:
    //   - If the action REQUIRES it and host doesn't have it held → force it (press)
    //   - If the action does NOT require it and host DOES have it held → suppress it (release)

    // SHIFT modifier
    if (action.modifiers & KEYMOD_SHIFT) {
        // Guest needs shift — press it if not already held on host
        if (!host_shift_held()) {
            close_contact(shift_left_pos_.row, shift_left_pos_.col);
            inj.forced_modifiers |= KEYMOD_SHIFT;
        }
    } else {
        // Guest needs NO shift — suppress only the shift(s) actually held.
        // Opening only the held contact(s) ensures release_injection can
        // re-close the correct one(s) without creating stuck contacts.
        if (host_lshift_held_ && shift_left_pos_.valid) {
            open_contact(shift_left_pos_.row, shift_left_pos_.col);
            inj.suppressed_modifiers |= KEYMOD_SHIFT;
        }
        if (host_rshift_held_ && shift_right_pos_.valid) {
            open_contact(shift_right_pos_.row, shift_right_pos_.col);
            inj.suppressed_modifiers |= KEYMOD_SHIFT;
        }
    }

    // CBM modifier (Commodore key)
    if (action.modifiers & KEYMOD_CBM) {
        if (!host_cbm_held_ && cbm_key_pos_.valid) {
            close_contact(cbm_key_pos_.row, cbm_key_pos_.col);
            inj.forced_modifiers |= KEYMOD_CBM;
        }
    } else {
        if (host_cbm_held_ && cbm_key_pos_.valid) {
            open_contact(cbm_key_pos_.row, cbm_key_pos_.col);
            inj.suppressed_modifiers |= KEYMOD_CBM;
        }
    }

    // CTRL modifier
    if (action.modifiers & KEYMOD_CTRL) {
        if (!host_ctrl_held_ && ctrl_key_pos_.valid) {
            close_contact(ctrl_key_pos_.row, ctrl_key_pos_.col);
            inj.forced_modifiers |= KEYMOD_CTRL;
        }
    } else {
        if (host_ctrl_held_ && ctrl_key_pos_.valid) {
            open_contact(ctrl_key_pos_.row, ctrl_key_pos_.col);
            inj.suppressed_modifiers |= KEYMOD_CTRL;
        }
    }

    // Close the main key contact
    close_contact(action.row, action.col);

    // Track this injection
    if (host_scancode != SDL_SCANCODE_UNKNOWN) {
        active_injections_[host_scancode] = inj;
    }
}

void KeyboardMapper::release_injection(const ActiveInjection& injection) {
    if (!keyboard_) return;

    // Open the main key contact
    open_contact(injection.action.row, injection.action.col);

    // Restore forced modifiers — we pressed these for the injection, now release them
    if (injection.forced_modifiers & KEYMOD_SHIFT) {
        open_contact(shift_left_pos_.row, shift_left_pos_.col);
    }
    if (injection.forced_modifiers & KEYMOD_CBM) {
        if (cbm_key_pos_.valid) open_contact(cbm_key_pos_.row, cbm_key_pos_.col);
    }
    if (injection.forced_modifiers & KEYMOD_CTRL) {
        if (ctrl_key_pos_.valid) open_contact(ctrl_key_pos_.row, ctrl_key_pos_.col);
    }

    // Restore suppressed modifiers — re-close them if the host key is still physically held
    if (injection.suppressed_modifiers & KEYMOD_SHIFT) {
        if (host_lshift_held_ && shift_left_pos_.valid) {
            close_contact(shift_left_pos_.row, shift_left_pos_.col);
        }
        if (host_rshift_held_ && shift_right_pos_.valid) {
            close_contact(shift_right_pos_.row, shift_right_pos_.col);
        }
    }
    if (injection.suppressed_modifiers & KEYMOD_CBM) {
        if (host_cbm_held_ && cbm_key_pos_.valid) {
            close_contact(cbm_key_pos_.row, cbm_key_pos_.col);
        }
    }
    if (injection.suppressed_modifiers & KEYMOD_CTRL) {
        if (host_ctrl_held_ && ctrl_key_pos_.valid) {
            close_contact(ctrl_key_pos_.row, ctrl_key_pos_.col);
        }
    }
}

GuestKeyAction KeyboardMapper::find_shift_key_position() const {
    return shift_left_pos_;
}

void KeyboardMapper::close_contact(uint8_t row, uint8_t col) {
    if (!keyboard_) return;
    if (row >= matrix_rows_ || col >= matrix_cols_) return;

    // Convert from array indices to hardware port bit numbers
    // Same convention as commodore_keyboard_key_down:
    //   row_bit = (matrix_rows - 1) - row
    //   col_bit = (matrix_cols - 1) - col
    uint8_t row_bit = (matrix_rows_ - 1) - row;
    uint8_t col_bit = (matrix_cols_ - 1) - col;

    keyboard_->row_open_contacts[row_bit] &= ~(1 << col_bit);
    keyboard_->col_open_contacts[col_bit] &= ~(1 << row_bit);
}

void KeyboardMapper::open_contact(uint8_t row, uint8_t col) {
    if (!keyboard_) return;
    if (row >= matrix_rows_ || col >= matrix_cols_) return;

    uint8_t row_bit = (matrix_rows_ - 1) - row;
    uint8_t col_bit = (matrix_cols_ - 1) - col;

    keyboard_->row_open_contacts[row_bit] |= (1 << col_bit);
    keyboard_->col_open_contacts[col_bit] |= (1 << row_bit);
}

bool KeyboardMapper::is_printable_key(SDL_Keycode sym) const {
    // A key is "printable" if pressing it (possibly with shift) produces a visible character.
    // These are the keys that should go through the character-based TEXTINPUT path.

    // ASCII letters
    if (sym >= SDLK_a && sym <= SDLK_z) return true;

    // Digits
    if (sym >= SDLK_0 && sym <= SDLK_9) return true;

    // Common punctuation/symbols
    switch (sym) {
        case SDLK_SPACE:
        case SDLK_EXCLAIM:
        case SDLK_QUOTEDBL:
        case SDLK_HASH:
        case SDLK_DOLLAR:
        case SDLK_PERCENT:
        case SDLK_AMPERSAND:
        case SDLK_QUOTE:
        case SDLK_LEFTPAREN:
        case SDLK_RIGHTPAREN:
        case SDLK_ASTERISK:
        case SDLK_PLUS:
        case SDLK_COMMA:
        case SDLK_MINUS:
        case SDLK_PERIOD:
        case SDLK_SLASH:
        case SDLK_COLON:
        case SDLK_SEMICOLON:
        case SDLK_LESS:
        case SDLK_EQUALS:
        case SDLK_GREATER:
        case SDLK_QUESTION:
        case SDLK_AT:
        case SDLK_LEFTBRACKET:
        case SDLK_BACKSLASH:
        case SDLK_RIGHTBRACKET:
        case SDLK_CARET:
        case SDLK_UNDERSCORE:
            // Note: SDLK_BACKQUOTE intentionally excluded — it maps to RESTORE (NMI)
            // on Commodore systems, which is not a matrix key.
            return true;
        default:
            return false;
    }
}

bool KeyboardMapper::is_modifier_key(SDL_Keycode sym) const {
    switch (sym) {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
        case SDLK_LCTRL:
        case SDLK_RCTRL:
        case SDLK_LGUI:   // Commodore key
        case SDLK_RGUI:
        case SDLK_LALT:
        case SDLK_CAPSLOCK:
            return true;
        default:
            return false;
    }
}

bool KeyboardMapper::has_direct_mapping(SDL_Keycode sym) const {
    return direct_map_.find(sym) != direct_map_.end();
}

// ============================================================================
// Factory functions
// ============================================================================

// Forward declarations — matrix configs are in system-specific files
extern const keyboard_matrix_config_t c64_keyboard_config;
extern const keyboard_matrix_config_t vic20_keyboard_config;
extern const keyboard_matrix_config_t c16_keyboard_config;

KeyboardMapper* create_c64_keyboard_mapper(commodore_keyboard_t* keyboard) {
    KeyboardMapper* mapper = new KeyboardMapper();
    mapper->set_guest_keyboard(keyboard);
    mapper->build_character_map_from_matrix(&c64_keyboard_config);

    // Register default emulator modifier mappings
    mapper->register_default_synthetic_mappings();

    // Commodore-specific character mappings (£, ↑, ←, π) are now handled
    // automatically by the PETSCII decode tables + petscii_to_host_char().
    // No manual add_char_mapping calls needed.

    return mapper;
}

KeyboardMapper* create_vic20_keyboard_mapper(commodore_keyboard_t* keyboard) {
    KeyboardMapper* mapper = new KeyboardMapper();
    mapper->set_guest_keyboard(keyboard);
    mapper->build_character_map_from_matrix(&vic20_keyboard_config);

    mapper->register_default_synthetic_mappings();

    // Commodore-specific character mappings (£, ↑, ←, π) are now handled
    // automatically by the PETSCII decode tables + petscii_to_host_char().

    return mapper;
}

KeyboardMapper* create_c16_keyboard_mapper(commodore_keyboard_t* keyboard) {
    KeyboardMapper* mapper = new KeyboardMapper();
    mapper->set_guest_keyboard(keyboard);
    mapper->build_character_map_from_matrix(&c16_keyboard_config);

    mapper->register_default_synthetic_mappings();

    // Commodore-specific character mappings are now handled automatically
    // by the PETSCII decode tables + petscii_to_host_char().

    return mapper;
}
