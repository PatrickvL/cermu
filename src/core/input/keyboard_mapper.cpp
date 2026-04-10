#include "core/cermu.hpp"
#include "core/input/keyboard_mapper.hpp"
#include "utils/guest_key_chars.hpp"
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
    if (!config || !config->entries || config->num_entries == 0) return;

    model_ = config->model;
    matrix_rows_ = config->rows;
    matrix_cols_ = config->cols;

    // Clear existing character map
    for (int i = 0; i < 128; i++) {
        char_map_[i] = GuestKeyAction();
    }

    // ────────────────────────────────────────────────────────────────────
    // Step 1: Auto-derive char_map_ from matrix entry normal/shifted chars.
    //
    // Each KeyMatrixEntry declares the Unicode character(s) the key
    // produces:  normal (unshifted) and shifted.  For ASCII-range chars,
    // populate char_map_[] directly.
    //
    // Letters: both 'A' and 'a' map with KEYMOD_NONE because Commodore's
    // default charset shows uppercase for unshifted keys.
    //
    // Shifted chars: populate with KEYMOD_SHIFT so that TEXTINPUT of the
    // shifted character presses the key + SHIFT on the guest.
    // ────────────────────────────────────────────────────────────────────
    for (int i = 0; i < config->num_entries; i++) {
        const KeyMatrixEntry& e = config->entries[i];

        // Map normal (unshifted) character
        if (e.normal > 0 && e.normal < 0x80) {
            char c = static_cast<char>(e.normal);

            if (c >= 'A' && c <= 'Z') {
                // Uppercase letter: map both cases to KEYMOD_NONE.
                // Commodore letter fixup — unshifted = uppercase on screen.
                if (!char_map_[(unsigned char)c].valid)
                    char_map_[(unsigned char)c] = GuestKeyAction(e.row, e.col, KEYMOD_NONE);
                char lower = c + 32;
                if (!char_map_[(unsigned char)lower].valid)
                    char_map_[(unsigned char)lower] = GuestKeyAction(e.row, e.col, KEYMOD_NONE);
            } else if (c >= 0x20) {
                // Printable non-letter: map with KEYMOD_NONE.
                if (!char_map_[(unsigned char)c].valid)
                    char_map_[(unsigned char)c] = GuestKeyAction(e.row, e.col, KEYMOD_NONE);
            }
        }

        // Map shifted character — KEYMOD_SHIFT so the mapper presses SHIFT
        if (e.shifted > 0 && e.shifted < 0x80) {
            char sc = static_cast<char>(e.shifted);

            if (sc >= 'a' && sc <= 'z') {
                // Lowercase shifted letter: already mapped above via the
                // uppercase normal letter path.  Skip to avoid overwriting
                // the KEYMOD_NONE mapping with KEYMOD_SHIFT.
            } else if (sc >= 0x20) {
                if (!char_map_[(unsigned char)sc].valid)
                    char_map_[(unsigned char)sc] = GuestKeyAction(e.row, e.col, KEYMOD_SHIFT);
            }
        }
    }

    // ────────────────────────────────────────────────────────────────────
    // Step 2: Apply character overrides.
    //
    // These handle host characters that need explicit TEXTINPUT mapping:
    //   Host '\' → guest ← key (non-ASCII, not auto-derived)
    //   Host '~' → guest π (Shift+↑)
    //   Host '{' → guest ( (no Commodore equivalent → fallback)
    //
    // Overrides always win (write over any auto-derived mapping).
    // ────────────────────────────────────────────────────────────────────
    if (config->char_overrides) {
        for (int i = 0; i < config->num_char_overrides; i++) {
            const KeyCharOverride& ov = config->char_overrides[i];
            if (ov.character == 0 || ov.character >= 128) continue;
            char_map_[ov.character] = GuestKeyAction(ov.row, ov.col, ov.modifier);
        }
    }

    // Underscore fixup: host '_' (Shift+minus) → C= + @ position.
    // The Commodore graphics character $A4 (▁) is the closest visual match.
    if (!char_map_['_'].valid && char_map_['@'].valid) {
        char_map_['_'] = GuestKeyAction(char_map_['@'].row, char_map_['@'].col, KEYMOD_CBM);
    }

    // ────────────────────────────────────────────────────────────────────
    // Step 3: Cache modifier key positions from matrix entries.
    //
    // Search the matrix for entries whose normal char32_t matches the
    // PUA identities of modifier keys.
    // ────────────────────────────────────────────────────────────────────
    shift_left_pos_ = GuestKeyAction();
    shift_right_pos_ = GuestKeyAction();
    cbm_key_pos_ = GuestKeyAction();
    ctrl_key_pos_ = GuestKeyAction();

    for (int i = 0; i < config->num_entries; i++) {
        const KeyMatrixEntry& e = config->entries[i];
        if (e.normal == UKEY_CBM_SHIFT_L) {
            shift_left_pos_ = GuestKeyAction(e.row, e.col, KEYMOD_NONE);
        } else if (e.normal == UKEY_CBM_SHIFT_R) {
            shift_right_pos_ = GuestKeyAction(e.row, e.col, KEYMOD_NONE);
        } else if (e.normal == UKEY_CBM_COMMODORE) {
            cbm_key_pos_ = GuestKeyAction(e.row, e.col, KEYMOD_NONE);
        } else if (e.normal == UKEY_CBM_CTRL) {
            ctrl_key_pos_ = GuestKeyAction(e.row, e.col, KEYMOD_NONE);
        }
    }

    log_info("KeyboardMapper: Built character map for %s (%d×%d matrix)\n",
           config->description ? config->description : "unknown",
           config->rows, config->cols);

    // Count valid mappings for debug
    int count = 0;
    for (int i = 0; i < 128; i++) {
        if (char_map_[i].valid) count++;
    }
    log_info("KeyboardMapper: %d character mappings active\n", count);
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

void KeyboardMapper::register_key_redirect(SDL_Keycode host_key, SDL_Keycode guest_key) {
    key_redirects_[host_key] = guest_key;
}

void KeyboardMapper::register_default_synthetic_mappings() {
    if (!keyboard_) return;

    // Use the optimised lookup to find matrix positions for guest-specific keys
    auto find_in_matrix = [this](SDL_Keycode target_key) -> GuestKeyAction {
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
    GuestKeyAction run_stop = find_in_matrix(CERMU_KEY_CBM_RUN_STOP);
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
    GuestKeyAction c_key = find_in_matrix(CERMU_KEY_CBM_COMMODORE);
    if (c_key.valid) {
        add_synthetic_mapping(SDLK_c, c_key, "Commodore (C=) key");
    }

    // CTRL key (in its C64 role — color selection etc.)
    GuestKeyAction ctrl = find_in_matrix(SDLK_LCTRL);
    if (ctrl.valid) {
        add_synthetic_mapping(SDLK_x, ctrl, "CTRL (C64)");
    }

    // CLR/HOME
    GuestKeyAction home = find_in_matrix(SDLK_HOME);
    if (home.valid) {
        add_synthetic_mapping(SDLK_h, home, "CLR/HOME");
    }

    // INST/DEL
    GuestKeyAction del = find_in_matrix(CERMU_KEY_CBM_DEL);
    if (del.valid) {
        add_synthetic_mapping(SDLK_d, del, "INST/DEL");
    }

            log_debug("KeyboardMapper: Registered %zu synthetic mappings (modifier: %s)\n",
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
    if (sym == SDLK_LALT || sym == SDLK_LGUI) {
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
    if (host_cbm_held_
        && !(mod & (KMOD_LALT | KMOD_LGUI))
        && sym != SDLK_LALT && sym != SDLK_LGUI) {
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

            // Special case: RESTORE is not a matrix key — it sets a
            // flag that the system polls for NMI.  No matrix contact.
            if (strcmp(mapping.description, "RESTORE (NMI)") == 0) {
                keyboard_->restore_key_pressed = true;
                ActiveInjection inj;
                inj.action = mapping.action;
                inj.host_scancode = scancode;
                inj.from_text_input = false;
                inj.is_restore = true;
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

    // Modifier keys — pass to commodore_keyboard (with redirect)
    if (is_modifier_key(sym)) {
        keyboard_->key_down(resolve_redirect(sym), false);
        return true;
    }

    // Non-printable keys — pass through (with redirect)
    if (!is_printable_key(sym)) {
        // Special case: backtick with shift → treat as printable so TEXTINPUT "~"
        // can be mapped (e.g., to π on C64/VIC-20). Without shift, backtick
        // falls through to key_down which handles RESTORE.
        if (sym == SDLK_BACKQUOTE && (mod & (KMOD_LSHIFT | KMOD_RSHIFT))) {
            // Fall through to the printable key / text input path below
        } else {
            keyboard_->key_down(resolve_redirect(sym), false);
            return true;
        }
    }

    // ====================================================================
    // Guest modifier pass-through: CBM, CTRL, or Shift+letter
    // ====================================================================
    // When the host user holds a guest-meaningful modifier and presses a
    // printable key, we bypass the TEXTINPUT path entirely.  The modifier
    // contact is already closed in the matrix from its own key_down event.
    // We just close the printable key's contact and let the guest KERNAL
    // see the combined modifier + key state.
    //
    // CBM and CTRL always pass through — they enable graphics characters
    // (C= + letter), colour codes (CTRL + digit), and other outputs that
    // have no host TEXTINPUT equivalent.
    //
    // Shift is included ONLY for letter keys: Commodore Shift+letter
    // produces graphics characters (in uppercase charset) or uppercase
    // letters (in lowercase charset), NOT the host notion of "shifted
    // character".  The TEXTINPUT path would suppress the guest Shift
    // contact, losing this behavior.
    //
    // Shift is NOT included for non-letter keys (digits, symbols) because
    // the host's shifted character maps to a DIFFERENT guest key:
    //   host Shift+= → '+' (a separate key on Commodore)
    //   host Shift+; → ':' (a separate key on Commodore)
    //   host Shift+' → '"' (Shift+2 on Commodore)
    //   host Shift+\ → '|' (maps to ↑ on Commodore)
    // These must go through the TEXTINPUT path so the mapper selects the
    // correct guest key based on the resulting character, not the raw key.
    if (is_printable_key(sym)) {
        bool is_letter = (sym >= SDLK_a && sym <= SDLK_z);
        bool want_passthrough = host_cbm_held_ || host_ctrl_held_
                             || (host_shift_held() && is_letter);
        if (want_passthrough) {
            // Try char_map first: for remapped characters (e.g., host '\' → guest ←,
            // host '|' → guest ↑) the char_map has the correct guest key position,
            // while the scancode-based EmuKey lookup would hit the host's physical
            // key position (which may be a different guest key entirely).
            char c = 0;
            if (sym >= 0 && sym < 128) c = static_cast<char>(sym);
            if (c && char_map_[(unsigned char)c].valid) {
                uint8_t mods = KEYMOD_NONE;
                if (host_cbm_held_)    mods |= KEYMOD_CBM;
                if (host_ctrl_held_)   mods |= KEYMOD_CTRL;
                if (host_shift_held()) mods |= KEYMOD_SHIFT;
                inject_press(GuestKeyAction(char_map_[(unsigned char)c].row,
                                            char_map_[(unsigned char)c].col, mods),
                             scancode, false);
                return true;
            }
            // Fallback: direct key → matrix lookup (with redirect)
            {
                uint8_t row, col;
                if (keyboard_->find_key(resolve_redirect(sym), &row, &col)) {
                    uint8_t mods = KEYMOD_NONE;
                    if (host_cbm_held_)    mods |= KEYMOD_CBM;
                    if (host_ctrl_held_)   mods |= KEYMOD_CTRL;
                    if (host_shift_held()) mods |= KEYMOD_SHIFT;
                    inject_press(GuestKeyAction(row, col, mods), scancode, false);
                    return true;
                }
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

    // Fallback: text input disabled, use direct SDL keycode mapping
    keyboard_->key_down(resolve_redirect(sym), false);
    return true;
}

bool KeyboardMapper::process_key_up(SDL_Keycode sym, SDL_Scancode scancode, uint16_t mod) {
    if (!keyboard_) return false;

    // Track host modifier state.
    // NOTE: We unconditionally clear the flag here.  SDL's `mod` field in
    // key-up events contains the state BEFORE the release, so the previous
    // approach of `flag = (mod & KMOD_FOO) != 0` left the flag TRUE after
    // key-up (because `mod` still included the released modifier).  That
    // caused the reconciliation in process_key_down to clean up one key
    // late, producing stuck-modifier artifacts under rapid typing.
    if (sym == SDLK_LSHIFT) host_lshift_held_ = false;
    if (sym == SDLK_RSHIFT) host_rshift_held_ = false;
    if (sym == SDLK_LCTRL || sym == SDLK_RCTRL) host_ctrl_held_ = false;
    if (sym == SDLK_LALT || sym == SDLK_LGUI) host_cbm_held_ = false;

    // Emulator modifier release
    if (sym == emu_modifier_key_) {
        emu_modifier_held_ = false;
        return true;
    }

    // Check if we have an active injection for this scancode
    auto it = active_injections_.find(scancode);
    if (it != active_injections_.end()) {
        // Copy and erase BEFORE releasing, so release_injection can scan
        // the remaining active injections to avoid clobbering shared
        // forced/suppressed modifiers.
        ActiveInjection inj = it->second;
        active_injections_.erase(it);

        if (inj.is_restore) {
            // RESTORE is not a matrix key — just clear the NMI flag.
            keyboard_->restore_key_pressed = false;
        } else {
            release_injection(inj);
        }
        return true;
    }

    // Clear pending key if it matches
    if (has_pending_key_ && pending_key_.scancode == scancode) {
        has_pending_key_ = false;
    }

    // Modifier keys — pass through (with redirect)
    if (is_modifier_key(sym)) {
        keyboard_->key_up(resolve_redirect(sym), false);
        return true;
    }

    // Non-printable keys — pass through (with redirect)
    if (!is_printable_key(sym)) {
        keyboard_->key_up(resolve_redirect(sym), false);
        return true;
    }

    // Fallback: direct release if text input is disabled
    if (!text_input_enabled_) {
        keyboard_->key_up(resolve_redirect(sym), false);
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
    //   - If the action REQUIRES it → always close the contact (a previous
    //     injection may have suppressed it). Mark as forced only if the host
    //     doesn't hold it (so release_injection knows to open it).
    //   - If the action does NOT require it and host DOES have it held →
    //     suppress it (release), but only if no other active injection still
    //     needs it.

    // Collect which modifiers other active injections need, to avoid
    // clobbering a modifier still in use by an overlapping injection.
    uint16_t others_need = 0;
    for (const auto& [_, other] : active_injections_) {
        others_need |= other.action.modifiers;
    }

    // SHIFT modifier
    if (action.modifiers & KEYMOD_SHIFT) {
        // Guest needs shift — always ensure the contact is closed.
        // A previous injection may have suppressed it even though the
        // host physically holds the key.
        close_contact(shift_left_pos_.row, shift_left_pos_.col);
        if (!host_shift_held()) {
            inj.forced_modifiers |= KEYMOD_SHIFT;
        }
    } else {
        // Guest needs NO shift — suppress only if no other active injection
        // requires it, and only the shift(s) actually held by the host.
        if (!(others_need & KEYMOD_SHIFT)) {
            if (host_lshift_held_ && shift_left_pos_.valid) {
                open_contact(shift_left_pos_.row, shift_left_pos_.col);
                inj.suppressed_modifiers |= KEYMOD_SHIFT;
            }
            if (host_rshift_held_ && shift_right_pos_.valid) {
                open_contact(shift_right_pos_.row, shift_right_pos_.col);
                inj.suppressed_modifiers |= KEYMOD_SHIFT;
            }
        }
    }

    // CBM modifier (Commodore key)
    if (action.modifiers & KEYMOD_CBM) {
        if (cbm_key_pos_.valid) {
            close_contact(cbm_key_pos_.row, cbm_key_pos_.col);
        }
        if (!host_cbm_held_) {
            inj.forced_modifiers |= KEYMOD_CBM;
        }
    } else {
        if (!(others_need & KEYMOD_CBM)) {
            if (host_cbm_held_ && cbm_key_pos_.valid) {
                open_contact(cbm_key_pos_.row, cbm_key_pos_.col);
                inj.suppressed_modifiers |= KEYMOD_CBM;
            }
        }
    }

    // CTRL modifier
    if (action.modifiers & KEYMOD_CTRL) {
        if (ctrl_key_pos_.valid) {
            close_contact(ctrl_key_pos_.row, ctrl_key_pos_.col);
        }
        if (!host_ctrl_held_) {
            inj.forced_modifiers |= KEYMOD_CTRL;
        }
    } else {
        if (!(others_need & KEYMOD_CTRL)) {
            if (host_ctrl_held_ && ctrl_key_pos_.valid) {
                open_contact(ctrl_key_pos_.row, ctrl_key_pos_.col);
                inj.suppressed_modifiers |= KEYMOD_CTRL;
            }
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

    // Check which forced/suppressed modifiers are still needed by other
    // active injections.  The caller has already erased *this* injection
    // from the map, so we only see the remaining ones.
    uint16_t still_forced     = 0;
    uint16_t still_suppressed = 0;
    for (const auto& [_, other] : active_injections_) {
        still_forced     |= other.forced_modifiers;
        still_suppressed |= other.suppressed_modifiers;
    }

    // Restore forced modifiers — only if no other injection still needs them
    if ((injection.forced_modifiers & KEYMOD_SHIFT) && !(still_forced & KEYMOD_SHIFT)) {
        open_contact(shift_left_pos_.row, shift_left_pos_.col);
    }
    if ((injection.forced_modifiers & KEYMOD_CBM) && !(still_forced & KEYMOD_CBM)) {
        if (cbm_key_pos_.valid) open_contact(cbm_key_pos_.row, cbm_key_pos_.col);
    }
    if ((injection.forced_modifiers & KEYMOD_CTRL) && !(still_forced & KEYMOD_CTRL)) {
        if (ctrl_key_pos_.valid) open_contact(ctrl_key_pos_.row, ctrl_key_pos_.col);
    }

    // Restore suppressed modifiers — only if no other injection still suppresses them
    if ((injection.suppressed_modifiers & KEYMOD_SHIFT) && !(still_suppressed & KEYMOD_SHIFT)) {
        if (host_lshift_held_ && shift_left_pos_.valid) {
            close_contact(shift_left_pos_.row, shift_left_pos_.col);
        }
        if (host_rshift_held_ && shift_right_pos_.valid) {
            close_contact(shift_right_pos_.row, shift_right_pos_.col);
        }
    }
    if ((injection.suppressed_modifiers & KEYMOD_CBM) && !(still_suppressed & KEYMOD_CBM)) {
        if (host_cbm_held_ && cbm_key_pos_.valid) {
            close_contact(cbm_key_pos_.row, cbm_key_pos_.col);
        }
    }
    if ((injection.suppressed_modifiers & KEYMOD_CTRL) && !(still_suppressed & KEYMOD_CTRL)) {
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

    // Row/col are hardware bit positions — same convention as
    // commodore_keyboard_t::key_down().
    keyboard_->row_open_contacts[row] &= ~(1 << col);
    keyboard_->col_open_contacts[col] &= ~(1 << row);
}

void KeyboardMapper::open_contact(uint8_t row, uint8_t col) {
    if (!keyboard_) return;
    if (row >= matrix_rows_ || col >= matrix_cols_) return;

    keyboard_->row_open_contacts[row] |= (1 << col);
    keyboard_->col_open_contacts[col] |= (1 << row);
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


