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
