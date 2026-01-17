#include "c64_system_wrapper.h"
#include "c64_test_loader.h"
#include "../../chip/input/commodore_keyboard.h"
#include <cstring>
#include <cstdio>

// C64 file detection
static float c64_can_load_file(const char* filepath, const uint8_t* data, size_t size) {
    // Check extensions
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
            // PRG files - check for valid load address
            if (size >= 2) {
                return 0.95f;  // Very high confidence for .prg files
            }
        }
        if (strcmp(ext, ".d64") == 0 || strcmp(ext, ".D64") == 0) {
            // D64 disk images - exactly 174848 bytes
            if (size == 174848) {
                return 0.95f;
            }
        }
        if (strcmp(ext, ".crt") == 0 || strcmp(ext, ".CRT") == 0) {
            // CRT cartridge files
            if (size >= 64 && memcmp(data, "C64 CARTRIDGE   ", 16) == 0) {
                return 1.0f;  // Perfect match
            }
        }
    }
    
    return 0.0f;
}

static const char* c64_extensions[] = {".prg", ".d64", ".crt", ".t64", ".tap", nullptr};

static HardwareTraits create_c64_hardware_traits() {
    HardwareTraits traits;
    
    // Display
    traits.display.native_width = 403;
    traits.display.native_height = 284;
    traits.display.visible_width = 403;
    traits.display.visible_height = 284;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 0;  // Direct RGB
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = true;
    
    // Audio
    traits.audio.format = AudioFormat::STEREO_16BIT;
    traits.audio.sample_rate_hz = 44100;
    traits.audio.channels = 2;
    traits.audio.chip_name = "SID 6581";
    
    // Timing (PAL default)
    traits.timing.cpu_frequency_hz = 985248;
    traits.timing.video_frequency_hz = 985248;
    traits.timing.audio_sample_rate_hz = 44100;
    traits.timing.target_fps = 50;
    traits.timing.cycles_per_frame = 19705;
    traits.timing.region = VideoRegion::PAL;
    
    return traits;
}

static SystemDescriptor c64_descriptor = {
    "Commodore 64",
    "C64",
    "8-bit home computer with VIC-II graphics and SID sound chip (1982)",
    c64_extensions,
    create_c64_hardware_traits(),
    c64_can_load_file
};

C64SystemWrapper::C64SystemWrapper()
    : c64_(nullptr)
    , speed_multiplier_(1.0f)
    , cycles_per_frame_(19705)  // PAL: 985248 Hz / 50 fps
{
    // Initialize config with defaults
    config_.vicii_standard = VIC_PAL;
    config_.rom_config = nullptr;
    config_.test_mode = C64_TEST_MODE_NORMAL;
    config_.test_binary_config = nullptr;
    config_.roml_present = false;
    config_.romh_present = false;
    config_.roml_filename = nullptr;
    config_.romh_filename = nullptr;
    config_.initial_exrom_state = true;
    config_.initial_game_state = true;
    
    // Get hardware traits from descriptor
    hardware_traits_ = c64_descriptor.hardware_traits;
}

C64SystemWrapper::~C64SystemWrapper() {
    shutdown();
}

const SystemDescriptor& C64SystemWrapper::get_descriptor() const {
    return c64_descriptor;
}

bool C64SystemWrapper::initialize() {
    if (c64_) {
        return true;  // Already initialized
    }
    
    c64_ = c64_system_create(&config_);
    if (!c64_) {
        printf("C64: Failed to create system\n");
        return false;
    }
    
    printf("C64: System initialized successfully\n");
    return true;
}

void C64SystemWrapper::shutdown() {
    if (c64_) {
        c64_system_destroy(c64_);
        c64_ = nullptr;
    }
}

void C64SystemWrapper::reset() {
    if (c64_) {
        c64_system_reset(c64_);
    }
}

void C64SystemWrapper::tick() {
    if (c64_) {
        c64_system_tick(c64_);
    }
}

void C64SystemWrapper::run_frame() {
    if (!c64_) return;
    
    // Execute one frame worth of cycles
    for (uint32_t i = 0; i < cycles_per_frame_; i++) {
        c64_system_tick(c64_);
    }
}

bool C64SystemWrapper::load_file(const char* filepath) {
    if (!c64_) {
        printf("C64: System not initialized\n");
        return false;
    }
    
    // Check file extension to determine type
    const char* ext = strrchr(filepath, '.');
    if (!ext) {
        printf("C64: No file extension found\n");
        return false;
    }
    
    if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
        // Load PRG file
        uint16_t load_address = 0;
        uint16_t sys_address = 0;
        
        if (!c64_test_load_prg_file(filepath, c64_->ram, &load_address, &sys_address)) {
            printf("C64: Failed to load PRG file: %s\n", filepath);
            return false;
        }
        
        printf("C64: Loaded PRG file: %s\n", filepath);
        printf("  Load address: $%04X\n", load_address);
        if (sys_address != 0) {
            printf("  SYS address: $%04X\n", sys_address);
            // Set PC to sys address if available
            // This requires access to the CPU, which we'll need to add
        }
        
        return true;
    }
    
    // Other file types (D64, CRT, etc.) would be handled here
    printf("C64: Unsupported file type: %s\n", ext);
    return false;
}

uint32_t* C64SystemWrapper::get_framebuffer() {
    // C64 uses a different framebuffer management system
    // This would return the VIC-II framebuffer
    return nullptr;  // TODO: Implement proper framebuffer access
}

void C64SystemWrapper::get_display_dimensions(int* width, int* height) const {
    // VIC-II visible area
    *width = 403;
    *height = 284;
}

void C64SystemWrapper::set_framebuffer(uint32_t* buffer, int width, int height) {
    if (c64_) {
        c64_set_framebuffer(c64_, buffer, width, height);
    }
}

void C64SystemWrapper::handle_keyboard_event(int key, bool pressed) {
    if (c64_ && c64_->keyboard) {
        // SDL provides Shift modifier state separately
        bool shift_pressed = false;  // TODO: Get actual shift state from SDL
        if (pressed) {
            commodore_keyboard_key_down(c64_->keyboard, key, shift_pressed);
        } else {
            commodore_keyboard_key_up(c64_->keyboard, key, shift_pressed);
        }
    }
}

void C64SystemWrapper::handle_controller_event(int controller, int button, bool pressed) {
    // C64 joystick support would go here
    // TODO: Implement joystick handling
}

void C64SystemWrapper::render_system_menu_items() {
    // C64-specific menu items would go here
    // This will be implemented when we update the GUI
}

void C64SystemWrapper::render_debug_windows(void* gui_state) {
    // C64 debug windows (chip visualization, etc.) would go here
    // This will use the existing chip debug system
}

uint64_t C64SystemWrapper::get_total_cycles() const {
    return c64_ ? c64_->total_cycles : 0;
}

uint32_t C64SystemWrapper::get_target_fps() const {
    return 50;  // PAL
}

void C64SystemWrapper::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
    cycles_per_frame_ = static_cast<uint32_t>(19705 * multiplier);
}

float C64SystemWrapper::get_speed_multiplier() const {
    return speed_multiplier_;
}

// Hardware traits interface
const HardwareTraits& C64SystemWrapper::get_hardware_traits() const {
    return hardware_traits_;
}

const SystemTiming& C64SystemWrapper::get_current_timing() const {
    return hardware_traits_.timing;
}

const DisplayTraits& C64SystemWrapper::get_display_traits() const {
    return hardware_traits_.display;
}

const AudioTraits& C64SystemWrapper::get_audio_traits() const {
    return hardware_traits_.audio;
}

// Configuration interface
const SystemConfiguration& C64SystemWrapper::get_configuration() const {
    return system_config_;
}

bool C64SystemWrapper::set_configuration(const SystemConfiguration& config) {
    system_config_ = config;
    return true;
}

bool C64SystemWrapper::apply_configuration() {
    // TODO: Apply configuration changes to C64 system
    // This would involve updating memory, region, peripherals, etc.
    return true;
}

void C64SystemWrapper::render_configuration_ui() {
    // TODO: Render C64-specific configuration UI
    // This will be implemented when we update the GUI
}

// Register C64 system with the registry
REGISTER_SYSTEM(c64_descriptor, []() {
    return std::make_unique<C64SystemWrapper>();
})