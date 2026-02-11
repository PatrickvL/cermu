#include "c16_system.h"
#include "c16_keyboard_matrix.h"
#include "../../chip/input/emu_key_sdl_map.h"
#include "../../core/storage/rom_loader.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

// ============================================================================
// Hardware Traits Definition
// ============================================================================

HardwareTraits C16System::create_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits - C16 uses MOS7360 (TED)
    traits.display.native_width = 320;
    traits.display.native_height = 200;
    traits.display.visible_width = 320;
    traits.display.visible_height = 200;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 128;      // 128 colors (luminance variations)
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = true;
    
    // C16/Plus/4 palette (simplified - first 16 base colors)
    const uint32_t c16_colors[16] = {
        0x000000, 0xFFFFFF, 0x8E3C97, 0x72DB87,
        0x4F44D8, 0x3DAC29, 0xC94B48, 0x5DD9E8,
        0x8A4A00, 0xAC7E3C, 0xDB8B8A, 0x94B6E0,
        0x868686, 0xC9E29E, 0x5CC9B5, 0xBDBDBD
    };
    
    for (int i = 0; i < 16; i++) {
        uint32_t c = c16_colors[i];
        traits.display.default_palette.push_back(
            PaletteColor((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255)
        );
    }
    
    // Audio traits - TED has 2 channel sound
    traits.audio.format = AudioFormat::MONO_16BIT;
    traits.audio.sample_rate_hz = 22050;
    traits.audio.channels = 1;
    traits.audio.chip_name = "TED 7360";
    
    // Timing - PAL version
    traits.timing.cpu_frequency_hz = 886724;    // ~0.89 MHz (PAL)
    traits.timing.video_frequency_hz = 886724;  // Same as CPU
    traits.timing.audio_sample_rate_hz = 22050;
    traits.timing.target_fps = 50;              // PAL
    traits.timing.cycles_per_frame = 17734;     // 886724 / 50
    traits.timing.region = VideoRegion::PAL;
    
    // Memory options
    traits.memory_options.push_back({
        "16KB (C16)",
        16384,
        32768,  // 32KB ROM
        false
    });
    traits.memory_options.push_back({
        "64KB (Plus/4)",
        65536,
        32768,
        true
    });
    
    // Region options
    traits.region_options.push_back({
        "PAL",
        VideoRegion::PAL,
        traits.timing,
        true
    });
    
    SystemTiming ntsc_timing = traits.timing;
    ntsc_timing.cpu_frequency_hz = 894886;      // ~0.89 MHz (NTSC)
    ntsc_timing.video_frequency_hz = 894886;
    ntsc_timing.target_fps = 60;
    ntsc_timing.cycles_per_frame = 14914;       // 894886 / 60
    ntsc_timing.region = VideoRegion::NTSC;
    
    traits.region_options.push_back({
        "NTSC",
        VideoRegion::NTSC,
        ntsc_timing,
        false
    });
    
    return traits;
}

// File detection callback
float C16System::can_load_file_static(const char* filepath, const uint8_t* data, size_t size) {
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
            // PRG files with C16/Plus4 load address (0x1001)
            if (size >= 2) {
                uint16_t load_addr = data[0] | (data[1] << 8);
                if (load_addr == 0x1001) {
                    return 0.6f;  // Moderate confidence (could be VIC-20 too)
                }
                return 0.4f;  // Lower confidence for generic PRG
            }
        }
        if (strcmp(ext, ".tap") == 0 || strcmp(ext, ".TAP") == 0) {
            return 0.5f;
        }
        if (strcmp(ext, ".d64") == 0 || strcmp(ext, ".D64") == 0) {
            return 0.4f;
        }
    }
    return 0.0f;
}

static const char* c16_extensions[] = {".prg", ".tap", ".d64", nullptr};

const SystemDescriptor C16System::c16_descriptor = {
    "Commodore 16 / Plus/4",
    "C16",
    "Commodore 16 and Plus/4 (1984) - 16KB/64KB RAM, TED graphics",
    c16_extensions,
    C16System::create_hardware_traits(),
    C16System::can_load_file_static
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
C16System::C16System()
    : EmulatedSystem()
    , mos7501_(nullptr)
    , ted_(nullptr)
    , keyboard_(nullptr)
    , cycles_per_frame_(17734)
    , initialized_(false)
{
    hardware_traits_ = create_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
    
    // Initialize memory arrays
    memset(ram_simple_, 0, sizeof(ram_simple_));
    memset(basic_rom_, 0, sizeof(basic_rom_));
    memset(kernal_rom_, 0, sizeof(kernal_rom_));
}

C16System::~C16System() {
    shutdown();
}

// ============================================================================
// System Identification
// ============================================================================

const SystemDescriptor& C16System::get_descriptor() const {
    return c16_descriptor;
}

// ============================================================================
// Configuration Management
// ============================================================================

bool C16System::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

bool C16System::apply_configuration() {
    // Apply region settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.region_options.size())) {
        const RegionOption& region = hardware_traits_.region_options[config_.region_option_index];
        cycles_per_frame_ = region.timing.cycles_per_frame;
    }
    
    return true;
}

// ============================================================================
// System Lifecycle
// ============================================================================

bool C16System::initialize() {
    if (initialized_) {
        return true;
    }
    
    printf("C16: Initializing system\n");
    
    // Load ROMs using common ROM loader
    bool roms_loaded = load_roms();
    if (!roms_loaded) {
        printf("C16: Warning - ROMs not loaded, system may not function correctly\n");
    }
    
    // TODO: Initialize MOS7501 CPU when implemented
    mos7501_ = nullptr;
    
    // TODO: Initialize TED 7360 when implemented
    ted_ = nullptr;
    
    // Create keyboard matrix (8×8, scanned by TED)
    keyboard_ = commodore_keyboard_create(&c16_keyboard_config);
    if (keyboard_) {
        // Create the layered keyboard mapper for character-based input
        keyboard_mapper_.reset(create_c16_keyboard_mapper(keyboard_));
    } else {
        printf("C16: Warning - keyboard matrix creation failed\n");
    }
    // NOTE: TED keyboard scanning callbacks will be connected once the TED
    // chip is implemented. The keyboard contact arrays are updated immediately
    // by key_down/key_up and will be ready for TED readback.
    
    initialized_ = true;
    return true;
}

void C16System::shutdown() {
    printf("C16: Shutting down system\n");
    
    // TODO: Destroy MOS7501 when implemented
    mos7501_ = nullptr;
    
    // TODO: Destroy TED when implemented
    ted_ = nullptr;
    
    // Destroy keyboard
    if (keyboard_) {
        commodore_keyboard_destroy(keyboard_);
        keyboard_ = nullptr;
    }
    
    initialized_ = false;
}

void C16System::reset() {
    printf("C16: Resetting system\n");
    
    // TODO: Reset MOS7501 CPU when implemented
    // TODO: Reset TED when implemented
    
    // Reset keyboard matrix
    if (keyboard_) {
        commodore_keyboard_reset(keyboard_);
    }
    
    total_cycles_ = 0;
}

// ============================================================================
// Execution
// ============================================================================

void C16System::tick() {
    // TODO: Execute one CPU cycle when MOS7501 is implemented
    // if (mos7501_) {
    //     mos7501_tick(mos7501_);
    // }
    
    // TODO: Tick TED when implemented
    // if (ted_) {
    //     ted_tick(ted_);
    // }
    
    // TODO: Tick CIA when implemented
    // if (cia_) {
    //     mos6526_tick(cia_);
    // }
    
    total_cycles_++;
}

void C16System::run_frame() {
    uint32_t adjusted_cycles = static_cast<uint32_t>(cycles_per_frame_ * speed_multiplier_);
    for (uint32_t i = 0; i < adjusted_cycles; i++) {
        tick();
    }
}

// ============================================================================
// File Loading
// ============================================================================

bool C16System::load_file(const char* filepath) {
    if (!initialized_) {
        if (!initialize()) {
            return false;
        }
    }
    
    printf("C16: Loading file: %s\n", filepath);
    
    // Determine file type
    const char* ext = strrchr(filepath, '.');
    if (!ext) {
        printf("C16: Unknown file type (no extension)\n");
        return false;
    }
    
    if (strcmp(ext, ".prg") == 0 || strcmp(ext, ".PRG") == 0) {
        // TODO: Implement PRG loading
        printf("C16: PRG file loading not yet implemented\n");
        return false;
    }
    
    printf("C16: Unsupported file type: %s\n", ext);
    return false;
}

// ============================================================================
// Display
// ============================================================================

uint32_t* C16System::get_framebuffer() {
    return rgba_framebuffer_;
}

void C16System::get_display_dimensions(int* width, int* height) const {
    *width = 320;
    *height = 200;
}

void C16System::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
    
    // TODO: Set TED framebuffer when TED 7360 chip is implemented
}

// ============================================================================
// Input
// ============================================================================

void C16System::handle_keyboard_event(int key, bool pressed) {
    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down((SDL_Keycode)key, SDL_SCANCODE_UNKNOWN, 0, false);
        } else {
            keyboard_mapper_->process_key_up((SDL_Keycode)key, SDL_SCANCODE_UNKNOWN, 0);
        }
    } else if (keyboard_) {
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key((SDL_Keycode)key);
        if (ek != EMUKEY_NONE) {
            if (pressed) {
                commodore_keyboard_key_down(keyboard_, ek, false);
            } else {
                commodore_keyboard_key_up(keyboard_, ek, false);
            }
        }
    }
}

void C16System::handle_keyboard_event_ex(int key, int scancode, uint16_t mod, bool pressed, bool repeat) {
    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down((SDL_Keycode)key, (SDL_Scancode)scancode, mod, repeat);
        } else {
            keyboard_mapper_->process_key_up((SDL_Keycode)key, (SDL_Scancode)scancode, mod);
        }
    } else if (!repeat) {
        handle_keyboard_event(key, pressed);
    }
}

void C16System::handle_text_input(const char* text) {
    if (keyboard_mapper_) {
        keyboard_mapper_->process_text_input(text);
    }
}

void C16System::release_all_keys() {
    if (keyboard_mapper_) {
        keyboard_mapper_->release_all();
    }
}

// ============================================================================
// GUI Integration
// ============================================================================

void C16System::render_system_menu_items() {
#ifdef IMGUI_VERSION
    if (ImGui::MenuItem("Reset C16")) {
        reset();
    }
#endif
}

void C16System::render_configuration_ui() {
#ifdef IMGUI_VERSION
    ImGui::Text("C16/Plus4 Configuration");
    ImGui::Separator();
    
    // Memory configuration
    ImGui::Text("System Model:");
    for (size_t i = 0; i < hardware_traits_.memory_options.size(); i++) {
        bool selected = (config_.memory_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(hardware_traits_.memory_options[i].name, selected)) {
            SystemConfiguration new_config = config_;
            new_config.memory_option_index = static_cast<int>(i);
            set_configuration(new_config);
            apply_configuration();
        }
    }
    
    ImGui::Separator();
    
    // Region configuration
    ImGui::Text("Video Region:");
    for (size_t i = 0; i < hardware_traits_.region_options.size(); i++) {
        bool selected = (config_.region_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(hardware_traits_.region_options[i].name, selected)) {
            SystemConfiguration new_config = config_;
            new_config.region_option_index = static_cast<int>(i);
            set_configuration(new_config);
            apply_configuration();
        }
    }
#endif
}

// ============================================================================
// State
// ============================================================================

uint32_t C16System::get_target_fps() const {
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.region_options.size())) {
        return hardware_traits_.region_options[config_.region_option_index].timing.target_fps;
    }
    return 50;  // Default PAL
}

// ============================================================================
// Emulation Control
// ============================================================================

void C16System::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}
// ============================================================================
// Private Helper Methods
// ============================================================================

bool C16System::load_roms() {
    // Try to load C16/Plus4 ROMs from standard locations
    const char* rom_root = "data/c16/roms";  // Default ROM path
    
    // Load KERNAL ROM (16KB at $C000-$FFFF)
    const char* kernal_files[] = {
        "kernal.318006-01.bin",
        "kernal.rom",
        "318006-01.bin",
        nullptr
    };
    
    bool kernal_ok = rom_loader_load_from_root(
        rom_root, kernal_files,
        sizeof(kernal_rom_), kernal_rom_, sizeof(kernal_rom_)
    );
    
    if (!kernal_ok) {
        printf("C16: Failed to load KERNAL ROM\n");
    }
    
    // Load BASIC ROM (16KB at $8000-$BFFF)
    const char* basic_files[] = {
        "basic.318006-02.bin",
        "basic.rom",
        "318006-02.bin",
        nullptr
    };
    
    bool basic_ok = rom_loader_load_from_root(
        rom_root, basic_files,
        sizeof(basic_rom_), basic_rom_, sizeof(basic_rom_)
    );
    
    if (!basic_ok) {
        printf("C16: Failed to load BASIC ROM\n");
    }
    
    return (kernal_ok && basic_ok);
}

uint8_t C16System::cpu_read(uint32_t addr) {
    uint16_t addr16 = addr & 0xFFFF;
    
    // RAM (0x0000-size based on configuration)
    size_t ram_size = 16384;  // Default C16
    if (config_.memory_option_index >= 0 &&
        config_.memory_option_index < static_cast<int>(hardware_traits_.memory_options.size())) {
        ram_size = hardware_traits_.memory_options[config_.memory_option_index].ram_size;
    }
    
    if (addr16 < ram_size) {
        return ram_simple_[addr16];
    }
    
    // BASIC ROM (0x8000-0xBFFF = 16KB)
    if (addr16 >= 0x8000 && addr16 < 0xC000) {
        return basic_rom_[addr16 - 0x8000];
    }
    
    // KERNAL ROM (0xC000-0xFFFF = 16KB)
    if (addr16 >= 0xC000) {
        return kernal_rom_[addr16 - 0xC000];
    }
    
    return 0xFF;  // Unmapped memory
}

void C16System::cpu_write(uint32_t addr, uint8_t data) {
    uint16_t addr16 = addr & 0xFFFF;
    
    // RAM (0x0000-size based on configuration)
    size_t ram_size = 16384;  // Default C16
    if (config_.memory_option_index >= 0 &&
        config_.memory_option_index < static_cast<int>(hardware_traits_.memory_options.size())) {
        ram_size = hardware_traits_.memory_options[config_.memory_option_index].ram_size;
    }
    
    if (addr16 < ram_size) {
        ram_simple_[addr16] = data;
    }
    
    // ROM areas are read-only, writes are ignored
}

uint8_t C16System::cpu_read_callback(void* user_data, uint32_t addr, uint8_t bus_state) {
    C16System* sys = static_cast<C16System*>(user_data);
    (void)bus_state;
    return sys->cpu_read(addr);
}

void C16System::cpu_write_callback(void* user_data, uint32_t addr, uint8_t data) {
    C16System* sys = static_cast<C16System*>(user_data);
    sys->cpu_write(addr, data);
}

// ============================================================================
// System Registration
// ============================================================================

REGISTER_SYSTEM(C16System::c16_descriptor, []() {
    return std::make_unique<C16System>();
})