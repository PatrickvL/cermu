#include "emulated_system.h"
#include <cstring>
#include <algorithm>
#include <fstream>

// ============================================================================
// EmulatedSystem Base Class Implementation
// ============================================================================

EmulatedSystem::EmulatedSystem()
    : rgba_framebuffer_(nullptr)
    , rgba_width_(0)
    , rgba_height_(0)
    , total_cycles_(0)
    , speed_multiplier_(1.0f)
{
}

// Final implementations (identical for all systems)
const SystemConfiguration& EmulatedSystem::get_configuration() const {
    return config_;
}

const HardwareTraits& EmulatedSystem::get_hardware_traits() const {
    return hardware_traits_;
}

const SystemTiming& EmulatedSystem::get_current_timing() const {
    int idx = config_.region_option_index;
    if (idx >= 0 && idx < static_cast<int>(hardware_traits_.region_options.size())) {
        return hardware_traits_.region_options[idx].timing;
    }
    return hardware_traits_.timing;
}

const DisplayTraits& EmulatedSystem::get_display_traits() const {
    return hardware_traits_.display;
}

const AudioTraits& EmulatedSystem::get_audio_traits() const {
    return hardware_traits_.audio;
}

uint64_t EmulatedSystem::get_total_cycles() const {
    return total_cycles_;
}

float EmulatedSystem::get_speed_multiplier() const {
    return speed_multiplier_;
}

// Default implementations (can be overridden)
void EmulatedSystem::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
}

bool EmulatedSystem::initialize() {
    reset();
    return true;
}

void EmulatedSystem::shutdown() {
    // Default: nothing to clean up
}

void EmulatedSystem::handle_controller_event(int controller, int button, bool pressed) {
    // Default: no controller support
    (void)controller;
    (void)button;
    (void)pressed;
}

void EmulatedSystem::render_debug_windows(void* gui_state) {
    // Default: no debug windows
    (void)gui_state;
}

void EmulatedSystem::handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) {
    // Default: fall back to the simple handle_keyboard_event (ignoring extra info)
    (void)scancode;
    (void)mod;
    (void)repeat;
    if (!repeat) {
        handle_keyboard_event(key, pressed);
    }
}

void EmulatedSystem::handle_text_input(const char* text) {
    // Default: no text input handling (systems using KeyboardMapper override this)
    (void)text;
}

void EmulatedSystem::release_all_keys() {
    // Default: nothing to release
}