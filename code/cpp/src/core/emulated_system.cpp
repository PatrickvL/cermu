#include "emulated_system.h"
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <vector>

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

SystemConfiguration EmulatedSystem::detect_optimal_configuration(
    const char* /*filepath*/, const uint8_t* /*data*/, size_t /*size*/) {
    // Default: walk hardware traits and select the default option for each axis
    SystemConfiguration config;

    // Memory: find the default option
    for (size_t i = 0; i < hardware_traits_.memory_options.size(); i++) {
        if (hardware_traits_.memory_options[i].is_default) {
            config.memory_option_index = static_cast<int>(i);
            break;
        }
    }

    // Region: find the default option
    for (size_t i = 0; i < hardware_traits_.region_options.size(); i++) {
        if (hardware_traits_.region_options[i].is_default) {
            config.region_option_index = static_cast<int>(i);
            break;
        }
    }

    // Peripherals: apply defaults
    for (const auto& p : hardware_traits_.peripheral_options) {
        config.enabled_peripherals[p.id] = p.enabled_by_default;
    }

    return config;
}

void EmulatedSystem::apply_file_configuration(const char* filepath) {
    if (!filepath) return;

    FILE* f = fopen(filepath, "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    size_t read_size = fsize < 65536 ? (size_t)fsize : 65536;
    std::vector<uint8_t> buf(read_size);
    fread(buf.data(), 1, read_size, f);
    fclose(f);

    SystemConfiguration detected =
        detect_optimal_configuration(filepath, buf.data(), (size_t)fsize);

    // Merge: never downgrade memory, keep detected region
    SystemConfiguration merged = config_;
    if (detected.memory_option_index > merged.memory_option_index) {
        merged.memory_option_index = detected.memory_option_index;
    }
    if (detected.region_option_index >= 0) {
        merged.region_option_index = detected.region_option_index;
    }

    set_configuration(merged);
    apply_configuration();
}

uint32_t EmulatedSystem::get_audio_samples(float* /*buffer*/, uint32_t /*max_samples*/) {
    return 0; // No audio by default
}