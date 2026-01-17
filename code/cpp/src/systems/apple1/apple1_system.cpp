#include "apple1_system.h"
#include <cstring>
#include <cstdio>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

// Include chip headers
#include "../../chip/cpu/fam65xx/mos6502.h"

// Include ROM loader
#include "../../core/storage/rom_loader.h"
#include "../../core/system_registry.h"

// ============================================================================
// Hardware Traits Definition
// ============================================================================

static HardwareTraits create_apple1_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits - Apple 1 used terminal display (40x24 text)
    traits.display.native_width = 320;      // 40 columns * 8 pixels
    traits.display.native_height = 192;     // 24 rows * 8 pixels
    traits.display.visible_width = 320;
    traits.display.visible_height = 192;
    traits.display.format = FramebufferFormat::RGBA8888;
    traits.display.palette_size = 2;        // Monochrome (green on black typically)
    traits.display.pixel_aspect_ratio = 1.0f;
    traits.display.has_overscan = false;
    
    // Apple 1 monochrome palette (green phosphor CRT)
    traits.display.default_palette.push_back(
        PaletteColor(0, 0, 0, 255)          // Black background
    );
    traits.display.default_palette.push_back(
        PaletteColor(51, 255, 51, 255)      // Green phosphor text
    );
    
    // Audio traits - No audio hardware
    traits.audio.format = AudioFormat::NONE;
    traits.audio.sample_rate_hz = 0;
    traits.audio.channels = 0;
    traits.audio.chip_name = "None";
    
    // Timing
    traits.timing.cpu_frequency_hz = 1000000;   // 1 MHz
    traits.timing.video_frequency_hz = 1000000; // Same as CPU
    traits.timing.audio_sample_rate_hz = 0;
    traits.timing.target_fps = 60;              // Video refresh
    traits.timing.cycles_per_frame = 16667;     // 1000000 / 60
    traits.timing.region = VideoRegion::NTSC;
    
    // Memory options
    traits.memory_options.push_back({
        "4KB RAM",
        4096,
        0,
        false
    });
    traits.memory_options.push_back({
        "8KB RAM",
        8192,
        0,
        true  // Default
    });
    traits.memory_options.push_back({
        "64KB RAM",
        65536,
        0,
        false
    });
    
    return traits;
}

// File detection callback
static float apple1_can_load_file(const char* filepath, const uint8_t* data, size_t size) {
    const char* ext = strrchr(filepath, '.');
    if (ext) {
        // Apple 1 typically used simple binary files or text files
        if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
            return 0.4f;  // Low confidence - generic binary
        }
        if (strcmp(ext, ".hex") == 0 || strcmp(ext, ".HEX") == 0) {
            return 0.5f;  // Intel HEX format
        }
        if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".TXT") == 0) {
            return 0.3f;  // Text/source files
        }
    }
    (void)data;
    (void)size;
    return 0.0f;
}

static const char* apple1_extensions[] = {".bin", ".hex", ".txt", nullptr};

static SystemDescriptor apple1_descriptor = {
    "Apple 1",
    "APPLE1",
    "Apple 1 (1976) - Woz's first computer, 8KB RAM, terminal display",
    apple1_extensions,
    create_apple1_hardware_traits(),
    apple1_can_load_file
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
Apple1System::Apple1System()
    : EmulatedSystem()
    , cpu_(nullptr)
    , terminal_(nullptr)
    , cycles_per_frame_(16667)
    , ram_size_(8192)  // Default 8KB
    , has_basic_(false)
    , cursor_col_(0)
    , cursor_row_(0)
{
    hardware_traits_ = create_apple1_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
    
    // Initialize PIA
    pia6820_init(&pia_);
    pia_.user_data = this;
    pia_.on_port_b_write = pia_display_write;
}

Apple1System::~Apple1System() {
    // Destroy CPU
    if (cpu_) {
        mos6502_destroy(cpu_);
        cpu_ = nullptr;
    }
    
    // Destroy terminal
    if (terminal_) {
        delete terminal_;
        terminal_ = nullptr;
    }
}

// ============================================================================
// System Identification
// ============================================================================

const SystemDescriptor& Apple1System::get_descriptor() const {
    return apple1_descriptor;
}

// ============================================================================
// Configuration Management
// ============================================================================

bool Apple1System::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

bool Apple1System::apply_configuration() {
    // Apply memory configuration
    if (config_.memory_option_index >= 0 &&
        config_.memory_option_index < static_cast<int>(hardware_traits_.memory_options.size())) {
        const MemoryOption& mem = hardware_traits_.memory_options[config_.memory_option_index];
        ram_size_ = mem.ram_size;
    }
    
    return true;
}

// ============================================================================
// System Lifecycle
// ============================================================================
bool Apple1System::initialize() {
    printf("Apple1: Initializing system\n");
    
    // Initialize memory arrays
    memset(ram_simple_, 0, sizeof(ram_simple_));
    memset(monitor_rom_, 0, sizeof(monitor_rom_));
    memset(basic_rom_, 0, sizeof(basic_rom_));
    
    // Create terminal (40 cols x 24 rows, 8x8 characters)
    terminal_ = new TextTerminal(40, 24, 8, 8);
    terminal_->clear(0xFF000000);  // Black background
    terminal_->set_foreground_color(0xFF33FF33);  // Green phosphor
    terminal_->set_background_color(0xFF000000);  // Black
    
    // Load ROMs using common ROM loader
    bool roms_loaded = load_roms();
    if (!roms_loaded) {
        printf("Apple1: Warning - ROMs not loaded, system may not function correctly\n");
    }
    
    // Create CPU (MOS6502) with memory callbacks
    cpu_ = mos6502_create();
    if (!cpu_) {
        printf("Apple1: Failed to create MOS6502 CPU\n");
        return false;
    }
    
    // Create enhanced descriptor with memory callbacks
    fam65xx_chip_descriptor_t* cpu_desc = mos6502_create_descriptor(
        cpu_read,
        cpu_write,
        this  // user_data points to this Apple1System instance
    );
    
    if (cpu_desc) {
        mos6502_init_enhanced(cpu_, cpu_desc);
        mos6502_destroy_descriptor(cpu_desc);
    }
    
    // Reset CPU to initialize state
    mos6502_reset(cpu_, 0);
    
    // Reset PIA
    pia6820_init(&pia_);
    pia_.user_data = this;
    pia_.on_port_b_write = pia_display_write;
    
    printf("Apple1: System initialized (RAM: %dKB)\n", ram_size_ / 1024);
    return true;
}

void Apple1System::shutdown() {
    printf("Apple1: Shutting down system\n");
}

void Apple1System::reset() {
    printf("Apple1: Resetting system\n");
    if (cpu_) {
        mos6502_reset(cpu_, 0);
    }
    total_cycles_ = 0;
}

// ============================================================================
// Execution
// ============================================================================

void Apple1System::tick() {
    tick_cpu();
    total_cycles_++;
}

void Apple1System::run_frame() {
    uint32_t adjusted_cycles = static_cast<uint32_t>(cycles_per_frame_ * speed_multiplier_);
    for (uint32_t i = 0; i < adjusted_cycles; i++) {
        tick();
    }
}

// ============================================================================
// File Loading
// ============================================================================

bool Apple1System::load_file(const char* filepath) {
    printf("Apple1: Loading file: %s\n", filepath);
    
    // Determine file type
    const char* ext = strrchr(filepath, '.');
    if (!ext) {
        printf("Apple1: Unknown file type (no extension)\n");
        return false;
    }
    
    if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
        // TODO: Implement binary file loading
        printf("Apple1: Binary file loading not yet implemented\n");
        return false;
    }
    
    printf("Apple1: Unsupported file type: %s\n", ext);
    return false;
}

// ============================================================================
// Display
// ============================================================================

uint32_t* Apple1System::get_framebuffer() {
    // Render terminal to framebuffer
    if (terminal_ && rgba_framebuffer_) {
        terminal_->render(rgba_framebuffer_, rgba_width_, rgba_height_);
    }
    return rgba_framebuffer_;
}

void Apple1System::get_display_dimensions(int* width, int* height) const {
    *width = 320;   // 40 columns * 8 pixels
    *height = 192;  // 24 rows * 8 pixels
}

void Apple1System::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;
}

// ============================================================================
// Input
// ============================================================================

void Apple1System::handle_keyboard_event(int key, bool pressed) {
    if (!pressed) return;  // Only handle key press, not release
    
    // Convert to uppercase (Apple 1 was uppercase only)
    if (key >= 'a' && key <= 'z') {
        key = key - 'a' + 'A';
    }
    
    // Apple 1 uses 7-bit ASCII
    if (key >= 0x20 && key < 0x7F) {
        pia6820_set_keyboard_data(&pia_, key & 0x7F);
    } else if (key == '\r' || key == '\n') {
        pia6820_set_keyboard_data(&pia_, 0x0D);  // Carriage return
    } else if (key == '\b' || key == 127) {
        pia6820_set_keyboard_data(&pia_, 0x08);  // Backspace
    } else if (key == 27) {
        pia6820_set_keyboard_data(&pia_, 0x1B);  // Escape
    }
}

// ============================================================================
// GUI Integration
// ============================================================================

void Apple1System::render_system_menu_items() {
#ifdef IMGUI_VERSION
    if (ImGui::MenuItem("Reset Apple 1")) {
        reset();
    }
#endif
}

void Apple1System::render_configuration_ui() {
#ifdef IMGUI_VERSION
    ImGui::Text("Apple 1 Configuration");
    ImGui::Separator();
    
    // Memory configuration
    ImGui::Text("RAM Size:");
    for (size_t i = 0; i < hardware_traits_.memory_options.size(); i++) {
        bool selected = (config_.memory_option_index == static_cast<int>(i));
        if (ImGui::RadioButton(hardware_traits_.memory_options[i].name, selected)) {
            SystemConfiguration new_config = config_;
            new_config.memory_option_index = static_cast<int>(i);
            set_configuration(new_config);
            apply_configuration();
        }
    }
#endif
}

// ============================================================================
// State
// ============================================================================

uint32_t Apple1System::get_target_fps() const {
    return 60;  // Fixed 60 FPS
}

// ============================================================================
// Emulation Control
// ============================================================================

void Apple1System::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

// Memory access callbacks for CPU
uint8_t Apple1System::cpu_read(void* user_data, uint32_t addr, uint8_t bus_state) {
    Apple1System* sys = static_cast<Apple1System*>(user_data);
    (void)bus_state;
    
    uint16_t addr16 = addr & 0xFFFF;
    
    // PIA 6820 registers (0xD010-0xD013)
    if (addr16 >= 0xD010 && addr16 <= 0xD013) {
        return pia6820_read(&sys->pia_, addr16);
    }
    
    // RAM (0x0000 to ram_size)
    if (addr16 < sys->ram_size_) {
        return sys->ram_simple_[addr16];
    }
    
    // Monitor ROM (0xFF00-0xFFFF = 256 bytes)
    if (addr16 >= 0xFF00) {
        return sys->monitor_rom_[addr16 - 0xFF00];
    }
    
    // Optional BASIC ROM locations (varies by configuration)
    // TODO: Add BASIC ROM mapping if has_basic_ is true
    
    return 0xFF;  // Unmapped memory
}

void Apple1System::cpu_write(void* user_data, uint32_t addr, uint8_t data) {
    Apple1System* sys = static_cast<Apple1System*>(user_data);
    
    uint16_t addr16 = addr & 0xFFFF;
    
    // PIA 6820 registers (0xD010-0xD013)
    if (addr16 >= 0xD010 && addr16 <= 0xD013) {
        pia6820_write(&sys->pia_, addr16, data);
        return;
    }
    
    // RAM (0x0000 to ram_size)
    if (addr16 < sys->ram_size_) {
        sys->ram_simple_[addr16] = data;
        return;
    }
    
    // ROM areas are read-only, writes are ignored
}

void Apple1System::tick_cpu() {
    if (cpu_) {
        // Tick the CPU (this handles one cycle of execution)
        mos6502_tick(cpu_, 0);
    }
}

// PIA display write callback
void Apple1System::pia_display_write(void* user_data, uint8_t data) {
    Apple1System* sys = static_cast<Apple1System*>(user_data);
    sys->display_char(data & 0x7F);  // 7-bit ASCII
}

// Display character on terminal
void Apple1System::display_char(uint8_t ch) {
    if (!terminal_) return;
    
    if (ch == 0x0D) {
        // Carriage return - move to next line
        cursor_col_ = 0;
        cursor_row_++;
        if (cursor_row_ >= 24) {
            terminal_->scroll_up(1);
            cursor_row_ = 23;
        }
    } else if (ch == 0x08) {
        // Backspace
        if (cursor_col_ > 0) {
            cursor_col_--;
            terminal_->put_char(cursor_col_, cursor_row_, ' ');
        }
    } else if (ch == 0x1B) {
        // Escape - clear screen
        terminal_->clear();
        cursor_col_ = 0;
        cursor_row_ = 0;
    } else if (ch >= 0x20 && ch < 0x7F) {
        // Printable character
        terminal_->put_char(cursor_col_, cursor_row_, ch);
        cursor_col_++;
        if (cursor_col_ >= 40) {
            cursor_col_ = 0;
            cursor_row_++;
            if (cursor_row_ >= 24) {
                terminal_->scroll_up(1);
                cursor_row_ = 23;
            }
        }
    }
    
    // Update cursor position
    terminal_->set_cursor(cursor_col_, cursor_row_);
}

bool Apple1System::load_roms() {
    // Try to load Apple 1 ROMs from standard locations
    const char* rom_root = "data/apple1/roms";  // Default ROM path
    
    // Load Woz Monitor ROM (256 bytes at $FF00-$FFFF)
    const char* monitor_files[] = {
        "apple1.rom",
        "monitor.rom",
        "wozmon.rom",
        nullptr
    };
    
    bool monitor_ok = rom_loader_load_from_root(
        rom_root, monitor_files,
        sizeof(monitor_rom_), monitor_rom_, sizeof(monitor_rom_)
    );
    
    if (!monitor_ok) {
        printf("Apple1: Failed to load Monitor ROM\n");
    }
    
    // Optional: Load Apple 1 BASIC ROM (4KB)
    const char* basic_files[] = {
        "apple1basic.rom",
        "basic.rom",
        nullptr
    };
    
    bool basic_ok = rom_loader_load_from_root(
        rom_root, basic_files,
        sizeof(basic_rom_), basic_rom_, sizeof(basic_rom_)
    );
    
    if (basic_ok) {
        printf("Apple1: BASIC ROM loaded\n");
        has_basic_ = true;
    } else {
        printf("Apple1: BASIC ROM not found (optional)\n");
        has_basic_ = false;
    }
    
    return monitor_ok;  // Only monitor ROM is required
}

// ============================================================================
// System Registration
// ============================================================================

REGISTER_SYSTEM(apple1_descriptor, []() {
    return std::make_unique<Apple1System>();
})