#include "chip8_system.h"
#include <fstream>
#include <cstring>
#include <cstdio>

// SDL is only needed for keyboard mapping in GUI builds
#ifdef IMGUI_VERSION
#include <SDL.h>
#include "imgui.h"
#endif

// CHIP-8 font set (0-F, 5 bytes each)
static const uint8_t chip8_font[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// ============================================================================
// Hardware Traits Definition
// ============================================================================

static HardwareTraits create_chip8_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits
    traits.display.native_width = 64;
    traits.display.native_height = 32;
    traits.display.visible_width = 64;
    traits.display.visible_height = 32;
    traits.display.format = FramebufferFormat::MONOCHROME_1;  // 1-bit format
    traits.display.palette_size = 2;  // 2 colors (off/on)
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = false;
    
    // Original CHIP-8 display palette (green phosphor monitor)
    traits.display.default_palette.push_back(
        PaletteColor(0, 0, 0, 255)        // Color 0: Black (off)
    );
    traits.display.default_palette.push_back(
        PaletteColor(0, 255, 0, 255)      // Color 1: Green (on)
    );
    
    // Audio traits
    traits.audio.format = AudioFormat::MONO_8BIT;
    traits.audio.sample_rate_hz = 4000;
    traits.audio.channels = 1;
    traits.audio.chip_name = "Simple Beeper";
    
    // Timing
    traits.timing.cpu_frequency_hz = 600;  // ~600 Hz instruction rate
    traits.timing.video_frequency_hz = 60;  // 60 Hz refresh
    traits.timing.audio_sample_rate_hz = 4000;
    traits.timing.target_fps = 60;
    traits.timing.cycles_per_frame = 10;  // ~10 instructions per frame
    traits.timing.region = VideoRegion::NTSC;  // No real region for CHIP-8
    
    // Memory options
    traits.memory_options.push_back({
        "4KB Standard",
        4096,
        0,
        true
    });
    
    // Region options (speed variants)
    traits.region_options.push_back({
        "Standard (600 Hz)",
        VideoRegion::NTSC,
        traits.timing,
        true
    });
    
    SystemTiming fast_timing = traits.timing;
    fast_timing.cpu_frequency_hz = 1200;
    fast_timing.cycles_per_frame = 20;
    traits.region_options.push_back({
        "Fast (1200 Hz)",
        VideoRegion::CUSTOM,
        fast_timing,
        false
    });
    
    return traits;
}

// System descriptor and file detection
static float chip8_can_load_file(const char* filepath, const uint8_t* data, size_t size) {
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        if (strcmp(ext, ".ch8") == 0 || strcmp(ext, ".c8") == 0) {
            return 0.9f;
        }
    }
    
    if (size >= 10 && size <= 3584) {
        return 0.6f;
    }
    
    return 0.0f;
}

static const char* chip8_extensions[] = {".ch8", ".c8", nullptr};

static SystemDescriptor chip8_descriptor = {
    "CHIP-8 Interpreter",
    "CHIP8",
    "Simple interpreted system for games and demos (1970s)",
    chip8_extensions,
    create_chip8_hardware_traits(),
    chip8_can_load_file
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

Chip8System::Chip8System()
    : rgba_framebuffer_(nullptr)
    , rgba_width_(0)
    , rgba_height_(0)
    , total_cycles_(0)
    , speed_multiplier_(1.0f)
    , cycles_per_frame_(10)
    , display_dirty_(false)
    , shift_quirk_(false)
    , load_store_quirk_(false)
{
    hardware_traits_ = create_chip8_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
    reset();
}

// ============================================================================
// System Identification
// ============================================================================

const SystemDescriptor& Chip8System::get_descriptor() const {
    return chip8_descriptor;
}

// ============================================================================
// Configuration Management
// ============================================================================

const SystemConfiguration& Chip8System::get_configuration() const {
    return config_;
}

bool Chip8System::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    
    // Apply palette selection
    auto palette_it = config.custom_settings.find("display_palette");
    if (palette_it != config.custom_settings.end()) {
        const std::string& palette_name = palette_it->second;
        
        if (palette_name == "green") {
            current_palette_[0] = PaletteColor(0, 0, 0, 255);
            current_palette_[1] = PaletteColor(0, 255, 0, 255);
        }
        else if (palette_name == "amber") {
            current_palette_[0] = PaletteColor(0, 0, 0, 255);
            current_palette_[1] = PaletteColor(255, 176, 0, 255);
        }
        else if (palette_name == "white") {
            current_palette_[0] = PaletteColor(0, 0, 0, 255);
            current_palette_[1] = PaletteColor(255, 255, 255, 255);
        }
        else if (palette_name == "c64") {
            current_palette_[0] = PaletteColor(0x40, 0x31, 0x8D, 255);
            current_palette_[1] = PaletteColor(0x7B, 0x70, 0xFC, 255);
        }
    }
    
    display_dirty_ = true;
    return true;
}

bool Chip8System::apply_configuration() {
    // Apply speed settings
    if (config_.region_option_index >= 0 &&
        config_.region_option_index < static_cast<int>(hardware_traits_.region_options.size())) {
        const RegionOption& region = hardware_traits_.region_options[config_.region_option_index];
        cycles_per_frame_ = region.timing.cycles_per_frame;
    }
    
    return true;
}

// ============================================================================
// Hardware Trait Queries
// ============================================================================

const HardwareTraits& Chip8System::get_hardware_traits() const {
    return hardware_traits_;
}

const SystemTiming& Chip8System::get_current_timing() const {
    int idx = config_.region_option_index;
    if (idx >= 0 && idx < static_cast<int>(hardware_traits_.region_options.size())) {
        return hardware_traits_.region_options[idx].timing;
    }
    return hardware_traits_.timing;
}

const DisplayTraits& Chip8System::get_display_traits() const {
    return hardware_traits_.display;
}

const AudioTraits& Chip8System::get_audio_traits() const {
    return hardware_traits_.audio;
}

// ============================================================================
// System Lifecycle
// ============================================================================

bool Chip8System::initialize() {
    reset();
    return true;
}

void Chip8System::shutdown() {
    // Nothing to clean up
}

void Chip8System::reset() {
    memset(memory_, 0, sizeof(memory_));
    memset(V_, 0, sizeof(V_));
    memset(stack_, 0, sizeof(stack_));
    memset(native_display_, 0, sizeof(native_display_));
    memset(keys_, 0, sizeof(keys_));
    
    memcpy(memory_, chip8_font, sizeof(chip8_font));
    
    I_ = 0;
    PC_ = 0x200;
    SP_ = 0;
    delay_timer_ = 0;
    sound_timer_ = 0;
    
    total_cycles_ = 0;
    display_dirty_ = true;
}

// ============================================================================
// Execution
// ============================================================================

void Chip8System::tick() {
    uint16_t opcode = (memory_[PC_] << 8) | memory_[PC_ + 1];
    execute_instruction(opcode);
    total_cycles_++;
}

void Chip8System::run_frame() {
    for (uint32_t i = 0; i < cycles_per_frame_; i++) {
        tick();
    }
    
    update_timers();
}

// ============================================================================
// File Loading
// ============================================================================

bool Chip8System::load_file(const char* filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        printf("CHIP-8: Failed to open file: %s\n", filepath);
        return false;
    }
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (size > 4096 - 512) {
        printf("CHIP-8: File too large: %zu bytes (max 3584)\n", size);
        return false;
    }
    
    // Reset FIRST, then load ROM
    reset();
    
    // Load ROM into memory starting at 0x200
    file.read(reinterpret_cast<char*>(memory_ + 0x200), size);
    printf("CHIP-8: Loaded %zu bytes from %s\n", size, filepath);
    
    // PC is already set to 0x200 by reset()
    
    return true;
}

// ============================================================================
// Display
// ============================================================================

uint32_t* Chip8System::get_framebuffer() {
    if (display_dirty_ && rgba_framebuffer_) {
        // Convert native 1-bit to RGBA8888 using generic renderer
        FramebufferRenderer::convert_monochrome_1bit(
            native_display_,
            64, 32,
            current_palette_[0],
            current_palette_[1],
            rgba_framebuffer_,
            rgba_width_,
            rgba_height_
        );
        display_dirty_ = false;
    }
    
    return rgba_framebuffer_;
}

void Chip8System::get_display_dimensions(int* width, int* height) const {
    *width = 64;
    *height = 32;
}

void Chip8System::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
    display_dirty_ = true;
}

// ============================================================================
// Input
// ============================================================================

void Chip8System::handle_keyboard_event(int key, bool pressed) {
    int chip8_key = map_sdl_key_to_chip8(key);
    if (chip8_key >= 0 && chip8_key < 16) {
        keys_[chip8_key] = pressed ? 1 : 0;
    }
}

void Chip8System::handle_controller_event(int controller, int button, bool pressed) {
    // CHIP-8 doesn't use controllers
    (void)controller;
    (void)button;
    (void)pressed;
}

// ============================================================================
// GUI Integration
// ============================================================================

void Chip8System::render_system_menu_items() {
#ifdef IMGUI_VERSION
    if (ImGui::MenuItem("Reset CHIP-8")) {
        reset();
    }
#endif
}

void Chip8System::render_debug_windows(void* gui_state) {
    (void)gui_state;
    // TODO: Add debug windows
}

void Chip8System::render_configuration_ui() {
#ifdef IMGUI_VERSION
    ImGui::Text("CHIP-8 Display Configuration");
    ImGui::Separator();
    
    // Palette selection
    static const char* palette_names[] = {
        "Green Phosphor (Original)",
        "Amber Monitor",
        "White on Black",
        "C64 Colors"
    };
    static const char* palette_ids[] = {
        "green", "amber", "white", "c64"
    };
    
    std::string current = config_.custom_settings.count("display_palette") > 0
                        ? config_.custom_settings.at("display_palette")
                        : "green";
    
    int selected = 0;
    for (int i = 0; i < 4; i++) {
        if (current == palette_ids[i]) {
            selected = i;
            break;
        }
    }
    
    if (ImGui::Combo("Display Palette", &selected, palette_names, 4)) {
        SystemConfiguration new_config = config_;
        new_config.custom_settings["display_palette"] = palette_ids[selected];
        set_configuration(new_config);
    }
    
    // Show current colors
    ImGui::Separator();
    ImGui::Text("Current Palette:");
    ImVec4 color0(current_palette_[0].r / 255.0f,
                  current_palette_[0].g / 255.0f,
                  current_palette_[0].b / 255.0f, 1.0f);
    ImVec4 color1(current_palette_[1].r / 255.0f,
                  current_palette_[1].g / 255.0f,
                  current_palette_[1].b / 255.0f, 1.0f);
    
    ImGui::ColorButton("Off##color0", color0, 0, ImVec2(40, 40));
    ImGui::SameLine();
    ImGui::ColorButton("On##color1", color1, 0, ImVec2(40, 40));
    
    ImGui::Separator();
    
    // Speed configuration
    ImGui::Text("Execution Speed:");
    for (size_t i = 0; i < hardware_traits_.region_options.size(); i++) {
        bool selected_region = (config_.region_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(hardware_traits_.region_options[i].name, selected_region)) {
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

uint64_t Chip8System::get_total_cycles() const {
    return total_cycles_;
}

uint32_t Chip8System::get_target_fps() const {
    return 60;
}

// ============================================================================
// Emulation Control
// ============================================================================

void Chip8System::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
    cycles_per_frame_ = static_cast<uint32_t>(10 * multiplier);
}

float Chip8System::get_speed_multiplier() const {
    return speed_multiplier_;
}

// ============================================================================
// Private Helper Functions
// ============================================================================

void Chip8System::execute_instruction(uint16_t opcode) {
    uint16_t nnn = opcode & 0x0FFF;
    uint8_t n = opcode & 0x000F;
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t kk = opcode & 0x00FF;
    
    PC_ += 2;
    
    switch (opcode & 0xF000) {
        case 0x0000:
            if (opcode == 0x00E0) {
                // CLS
                memset(native_display_, 0, sizeof(native_display_));
                display_dirty_ = true;
            } else if (opcode == 0x00EE) {
                // RET
                SP_--;
                PC_ = stack_[SP_];
            }
            break;
            
        case 0x1000: PC_ = nnn; break;  // JP
        case 0x2000:  // CALL
            stack_[SP_] = PC_;
            SP_++;
            PC_ = nnn;
            break;
        case 0x3000: if (V_[x] == kk) PC_ += 2; break;  // SE Vx, byte
        case 0x4000: if (V_[x] != kk) PC_ += 2; break;  // SNE Vx, byte
        case 0x5000: if (V_[x] == V_[y]) PC_ += 2; break;  // SE Vx, Vy
        case 0x6000: V_[x] = kk; break;  // LD Vx, byte
        case 0x7000: V_[x] += kk; break;  // ADD Vx, byte
            
        case 0x8000:
            switch (n) {
                case 0x0: V_[x] = V_[y]; break;
                case 0x1: V_[x] |= V_[y]; break;
                case 0x2: V_[x] &= V_[y]; break;
                case 0x3: V_[x] ^= V_[y]; break;
                case 0x4: {
                    uint16_t sum = V_[x] + V_[y];
                    V_[0xF] = (sum > 255) ? 1 : 0;
                    V_[x] = sum & 0xFF;
                    break;
                }
                case 0x5:
                    V_[0xF] = (V_[x] > V_[y]) ? 1 : 0;
                    V_[x] -= V_[y];
                    break;
                case 0x6:
                    if (shift_quirk_) {
                        V_[0xF] = V_[y] & 0x1;
                        V_[x] = V_[y] >> 1;
                    } else {
                        V_[0xF] = V_[x] & 0x1;
                        V_[x] >>= 1;
                    }
                    break;
                case 0x7:
                    V_[0xF] = (V_[y] > V_[x]) ? 1 : 0;
                    V_[x] = V_[y] - V_[x];
                    break;
                case 0xE:
                    if (shift_quirk_) {
                        V_[0xF] = (V_[y] & 0x80) >> 7;
                        V_[x] = V_[y] << 1;
                    } else {
                        V_[0xF] = (V_[x] & 0x80) >> 7;
                        V_[x] <<= 1;
                    }
                    break;
            }
            break;
            
        case 0x9000: if (V_[x] != V_[y]) PC_ += 2; break;  // SNE Vx, Vy
        case 0xA000: I_ = nnn; break;  // LD I, addr
        case 0xB000: PC_ = nnn + V_[0]; break;  // JP V0, addr
        case 0xC000: V_[x] = (rand() & 0xFF) & kk; break;  // RND Vx, byte
            
        case 0xD000: {
            // DRW Vx, Vy, n - Draw sprite using native 1-bit buffer
            uint8_t xpos = V_[x] % 64;
            uint8_t ypos = V_[y] % 32;
            V_[0xF] = 0;
            
            for (int row = 0; row < n; row++) {
                uint8_t sprite_byte = memory_[I_ + row];
                int py = (ypos + row) % 32;
                
                for (int col = 0; col < 8; col++) {
                    int px = (xpos + col) % 64;
                    
                    // Calculate bit position in native buffer
                    int byte_index = py * (64 / 8) + (px / 8);
                    int bit_index = 7 - (px % 8);  // MSB first
                    
                    // Extract sprite bit
                    bool sprite_bit = (sprite_byte & (0x80 >> col)) != 0;
                    
                    if (sprite_bit) {
                        // Check for collision
                        uint8_t old_value = (native_display_[byte_index] >> bit_index) & 1;
                        if (old_value) {
                            V_[0xF] = 1;
                        }
                        
                        // XOR the bit
                        native_display_[byte_index] ^= (1 << bit_index);
                    }
                }
            }
            display_dirty_ = true;
            break;
        }
            
        case 0xE000:
            if (kk == 0x9E) {
                if (keys_[V_[x] & 0xF]) PC_ += 2;
            } else if (kk == 0xA1) {
                if (!keys_[V_[x] & 0xF]) PC_ += 2;
            }
            break;
            
        case 0xF000:
            switch (kk) {
                case 0x07: V_[x] = delay_timer_; break;
                case 0x0A: {
                    bool key_pressed = false;
                    for (int i = 0; i < 16; i++) {
                        if (keys_[i]) {
                            V_[x] = i;
                            key_pressed = true;
                            break;
                        }
                    }
                    if (!key_pressed) PC_ -= 2;
                    break;
                }
                case 0x15: delay_timer_ = V_[x]; break;
                case 0x18: sound_timer_ = V_[x]; break;
                case 0x1E: I_ += V_[x]; break;
                case 0x29: I_ = (V_[x] & 0xF) * 5; break;
                case 0x33:
                    memory_[I_] = V_[x] / 100;
                    memory_[I_ + 1] = (V_[x] / 10) % 10;
                    memory_[I_ + 2] = V_[x] % 10;
                    break;
                case 0x55:
                    for (int i = 0; i <= x; i++) {
                        memory_[I_ + i] = V_[i];
                    }
                    if (load_store_quirk_) I_ += x + 1;
                    break;
                case 0x65:
                    for (int i = 0; i <= x; i++) {
                        V_[i] = memory_[I_ + i];
                    }
                    if (load_store_quirk_) I_ += x + 1;
                    break;
            }
            break;
    }
}

void Chip8System::update_timers() {
    if (delay_timer_ > 0) delay_timer_--;
    if (sound_timer_ > 0) sound_timer_--;
}

int Chip8System::map_sdl_key_to_chip8(int sdl_key) {
#ifdef IMGUI_VERSION
    switch (sdl_key) {
        case SDLK_1: return 0x1;
        case SDLK_2: return 0x2;
        case SDLK_3: return 0x3;
        case SDLK_4: return 0xC;
        case SDLK_q: return 0x4;
        case SDLK_w: return 0x5;
        case SDLK_e: return 0x6;
        case SDLK_r: return 0xD;
        case SDLK_a: return 0x7;
        case SDLK_s: return 0x8;
        case SDLK_d: return 0x9;
        case SDLK_f: return 0xE;
        case SDLK_z: return 0xA;
        case SDLK_x: return 0x0;
        case SDLK_c: return 0xB;
        case SDLK_v: return 0xF;
        default: return -1;
    }
#else
    (void)sdl_key;
    return -1;
#endif
}

// Register CHIP-8 system with the registry
REGISTER_SYSTEM(chip8_descriptor, []() {
    return std::make_unique<Chip8System>();
})