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

// ============================================================================
// SystemRegistry Implementation
// ============================================================================

SystemRegistry& SystemRegistry::instance() {
    static SystemRegistry registry;
    return registry;
}

void SystemRegistry::register_system(const SystemDescriptor& descriptor, SystemFactory factory) {
    printf("SystemRegistry: Registering system: %s (%s)\n", descriptor.name, descriptor.short_name);
    systems_.push_back({descriptor, factory});
}
std::unique_ptr<EmulatedSystem> SystemRegistry::create_system_for_file(const char* filepath) {
    if (!filepath) {
        return nullptr;
    }
    
    printf("SystemRegistry: %zu systems registered\n", systems_.size());
    
    // Read file header for content-based detection
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open() || !file.good()) {
        printf("SystemRegistry: Failed to open file: %s\n", filepath);
        return nullptr;
    }
    
    // Read first 64KB or entire file, whichever is smaller
    std::vector<uint8_t> data;
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    size_t read_size = std::min<size_t>(file_size, 65536);
    data.resize(read_size);
    file.read(reinterpret_cast<char*>(data.data()), read_size);
    file.close();
    
    printf("SystemRegistry: File size: %zu bytes\n", file_size);
    
    // Find system with highest confidence
    float best_confidence = 0.0f;
    SystemFactory best_factory = nullptr;
    const char* best_system_name = nullptr;
    
    for (const auto& [descriptor, factory] : systems_) {
        if (descriptor.can_load_file) {
            float confidence = descriptor.can_load_file(filepath, data.data(), file_size);
            printf("SystemRegistry: %s confidence: %.2f\n", descriptor.short_name, confidence);
            if (confidence > best_confidence) {
                best_confidence = confidence;
                best_factory = factory;
                best_system_name = descriptor.short_name;
            }
        }
    }
    
    printf("SystemRegistry: Best match: %s (confidence: %.2f)\n",
           best_system_name ? best_system_name : "none", best_confidence);
    
    // Require at least 50% confidence
    if (best_confidence >= 0.5f && best_factory) {
        return best_factory();
    }
    
    return nullptr;
}

std::unique_ptr<EmulatedSystem> SystemRegistry::create_system_by_name(const char* short_name) {
    if (!short_name) {
        return nullptr;
    }
    
    for (const auto& [descriptor, factory] : systems_) {
        if (strcmp(descriptor.short_name, short_name) == 0) {
            return factory();
        }
    }
    
    return nullptr;
}