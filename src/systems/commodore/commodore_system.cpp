#include "commodore_system.h"

// ============================================================================
// CommodoreSystem — shared Commodore 8-bit base class implementation
// ============================================================================

bool CommodoreSystem::set_configuration(const SystemConfiguration& config) {
    config_ = config;

    // Update cached target FPS from region config
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.video_standard_configs.size())) {
        cached_target_fps_ = hardware_traits_.video_standard_configs[config_.region_option_index].timing.target_fps;
    } else {
        cached_target_fps_ = 50;  // Default PAL
    }

    return true;
}

void CommodoreSystem::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

void CommodoreSystem::handle_text_input(const char* text) {
    if (keyboard_mapper_) {
        keyboard_mapper_->process_text_input(text);
    }
}

void CommodoreSystem::release_all_keys() {
    if (keyboard_mapper_) {
        keyboard_mapper_->release_all();
    }
}

void CommodoreSystem::handle_keyboard_event_ex(
        SDL_Keycode key, SDL_Scancode scancode,
        uint16_t mod, bool pressed, bool repeat) {
    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down(key, scancode, mod, repeat);
        } else {
            keyboard_mapper_->process_key_up(key, scancode, mod);
        }
    } else if (!repeat) {
        handle_keyboard_event(key, pressed);
    }
}

// ============================================================================
// Guest keyboard scancodes — superset across C64, VIC-20, C16/Plus4
//
// Every SDL_Scancode listed here closes at least one keyboard matrix
// contact (via TEXTINPUT or direct-map) on at least one Commodore system.
// Controller keymap presets that overlap these keys will report collisions,
// guiding auto_assign_controller_keymaps() toward safer choices.
// ============================================================================

int CommodoreSystem::get_guest_keyboard_scancodes(const SDL_Scancode** out) const {
    static const SDL_Scancode scancodes[] = {
        // --- Letters (a–z, all close matrix contacts via TEXTINPUT) ---
        SDL_SCANCODE_A, SDL_SCANCODE_B, SDL_SCANCODE_C, SDL_SCANCODE_D,
        SDL_SCANCODE_E, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H,
        SDL_SCANCODE_I, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L,
        SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_O, SDL_SCANCODE_P,
        SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T,
        SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_SCANCODE_X,
        SDL_SCANCODE_Y, SDL_SCANCODE_Z,

        // --- Digits (0–9) ---
        SDL_SCANCODE_0, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
        SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7,
        SDL_SCANCODE_8, SDL_SCANCODE_9,

        // --- Symbols (close matrix contacts via TEXTINPUT char mapping) ---
        SDL_SCANCODE_SPACE,        SDL_SCANCODE_COMMA,
        SDL_SCANCODE_MINUS,        SDL_SCANCODE_PERIOD,
        SDL_SCANCODE_SLASH,        SDL_SCANCODE_SEMICOLON,
        SDL_SCANCODE_EQUALS,       SDL_SCANCODE_LEFTBRACKET,
        SDL_SCANCODE_BACKSLASH,    SDL_SCANCODE_RIGHTBRACKET,
        SDL_SCANCODE_APOSTROPHE,

        // --- Non-printable keys with matrix positions ---
        SDL_SCANCODE_RETURN,       SDL_SCANCODE_BACKSPACE,
        SDL_SCANCODE_TAB,
        SDL_SCANCODE_RIGHT,        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_DOWN,         SDL_SCANCODE_UP,
        SDL_SCANCODE_HOME,
        SDL_SCANCODE_F1,           SDL_SCANCODE_F3,
        SDL_SCANCODE_F5,           SDL_SCANCODE_F7,

        // --- Modifier keys with matrix positions ---
        SDL_SCANCODE_LSHIFT,       SDL_SCANCODE_RSHIFT,
        SDL_SCANCODE_LCTRL,        // CTRL
        SDL_SCANCODE_LGUI,         // C= (Commodore) key

        // --- C16/Plus4 extras (matrix positions unique to 264 series) ---
        SDL_SCANCODE_ESCAPE,       // ESC key on C16/Plus4
        SDL_SCANCODE_F2,           // F2 key on C16/Plus4
    };

    if (out) *out = scancodes;
    return static_cast<int>(sizeof(scancodes) / sizeof(scancodes[0]));
}
