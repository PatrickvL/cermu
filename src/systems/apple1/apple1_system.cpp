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

static SystemDescriptor apple1_descriptor = {
    "Apple 1",
    "APPLE1",
    "Apple 1 (1976) - Woz's first computer, 8KB RAM, terminal display",
    nullptr,  // supported_formats: Apple 1 does not use format handler system
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
    
    // Initialize PIA with callbacks
    pia6820_init(&pia_);
    pia_.user_data = this;
    pia_.on_port_a_read = pia_keyboard_read;  // Port A: keyboard input
    pia_.on_port_b_write = pia_display_write; // Port B: display output
    
    // Configure PIA direction: Port A = input, Port B = output (Apple 1 convention)
    pia_.port_a_direction = 0x00;  // All inputs (keyboard)
    pia_.port_b_direction = 0xFF;  // All outputs (display)
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
    memset(char_rom_, 0, sizeof(char_rom_));
    
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
    
    // Reset PIA with callbacks
    pia6820_init(&pia_);
    pia_.user_data = this;
    pia_.on_port_a_read = pia_keyboard_read;
    pia_.on_port_b_write = pia_display_write;
    pia_.port_a_direction = 0x00;  // Port A = input (keyboard)
    pia_.port_b_direction = 0xFF;  // Port B = output (display)
    
    setup_connector_ports();
    
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

    // Tick all attached peripheral devices
    tick_peripherals();
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

void Apple1System::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    if (!pressed) return;  // Only handle key press, not release
    
    // Convert to uppercase (Apple 1 was uppercase only)
    if (key >= 'a' && key <= 'z') {
        key = key - 'a' + 'A';
    }
    
    // Apple 1 uses 7-bit ASCII
    if (key >= 0x20 && key < 0x7F) {
        set_keyboard_data(key & 0x7F);
    } else if (key == '\r' || key == '\n') {
        set_keyboard_data(0x0D);  // Carriage return
    } else if (key == '\b' || key == 127) {
        set_keyboard_data(0x08);  // Backspace
    } else if (key == 27) {
        set_keyboard_data(0x1B);  // Escape
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
// Chip Info — Hardware menu enumeration
// ============================================================================

std::vector<ChipInfo> Apple1System::get_chip_info() const {
    std::vector<ChipInfo> chips = {
        { "MOS 6502 CPU",                "6502",       "CPU",     0x0000, false, false },
        { "PIA 6820 (Keyboard/Display)", "PIA",        "I/O",     0xD010, false, false },
        { "Text Terminal (40x24)",       "Terminal",   "Video",   0x0000, false, false },
        { "RAM",                         "RAM",        "Memory",  0x0000, false, false },
        { "Woz Monitor ROM (256B)",      "Monitor",    "Memory",  0xFF00, false, false },
    };
    if (has_basic_) {
        chips.push_back({ "Apple 1 BASIC ROM (4KB)", "BASIC", "Memory", 0xE000, false, false });
    }
    chips.push_back({ "Signetics 2513 Char ROM", "CharROM", "Memory", 0x0000, false, false });
    return chips;
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

// PIA keyboard read callback (Port A)
uint8_t Apple1System::pia_keyboard_read(void* user_data) {
    Apple1System* sys = static_cast<Apple1System*>(user_data);
    
    // Return current keyboard state
    // Note: The Apple 1 keyboard hardware clears bit 7 (strobe) when Port A is read
    // This is NOT PIA behavior - it's the external keyboard circuit's behavior
    uint8_t data = sys->pia_.port_a_data;
    
    // Simulate Apple 1 keyboard circuit: clear strobe on read
    sys->clear_keyboard_strobe();
    
    return data;
}

// PIA display write callback (Port B)
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

// Apple 1 keyboard helpers (system-specific PIA Port A usage)
void Apple1System::set_keyboard_data(uint8_t key_code) {
    // Apple 1 convention: Set bit 7 (strobe) and key code in bits 0-6
    pia6820_set_port_a_input(&pia_, 0x80 | (key_code & 0x7F));
    
    // Trigger CA1 to signal key press (for interrupt-driven input)
    pia6820_set_ca1(&pia_, true);
}

bool Apple1System::keyboard_ready() const {
    // Check if bit 7 is set (keyboard data available)
    return (pia_.port_a_data & 0x80) != 0;
}

void Apple1System::clear_keyboard_strobe() {
    // Apple 1 keyboard hardware behavior: The keyboard circuit clears bit 7 (strobe)
    // when the CPU reads Port A. This is NOT PIA behavior - it's the external
    // keyboard hardware responding to the PIA's read signal.
    pia_.port_a_data &= 0x7F;
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
    
    // Load Signetics 2513 Character ROM (512 bytes)
    const char* char_files[] = {
        "2513.rom",
        "signetics2513.bin",
        "chargen.rom",
        "342-0036-00.c1",
        nullptr
    };
    
    bool char_ok = rom_loader_load_from_root(
        rom_root, char_files,
        sizeof(char_rom_), char_rom_, sizeof(char_rom_)
    );
    
    if (char_ok && terminal_) {
        printf("Apple1: Signetics 2513 character ROM loaded\n");
        // Convert 2513 ROM format to 8x8 font for TextTerminal
        uint8_t font_8x8[256 * 8];
        memset(font_8x8, 0, sizeof(font_8x8));
        convert_2513_to_8x8_font(char_rom_, font_8x8);
        terminal_->set_font(font_8x8);
    } else {
        printf("Apple1: Character ROM not found, using built-in font\n");
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

// Convert Signetics 2513 character ROM (5x7 in 8 bytes) to 8x8 font
void Apple1System::convert_2513_to_8x8_font(const uint8_t* char_rom, uint8_t* font_8x8) {
    // The 2513 ROM contains 64 characters (uppercase ASCII 0x20-0x5F)
    // Each character is 8 bytes, with 5x7 pixel data in the upper bits
    
    for (int ch = 0; ch < 64; ch++) {
        int src_offset = ch * 8;
        int dst_offset = (0x20 + ch) * 8;  // Map to ASCII 0x20-0x5F
        
        // Copy and shift the 5-bit wide characters to left-align in 8-bit bytes
        for (int row = 0; row < 7; row++) {
            // 2513 stores 5-bit data in upper 5 bits, shift left by 1 for better centering
            font_8x8[dst_offset + row] = (char_rom[src_offset + row] >> 1) & 0xF8;
        }
        font_8x8[dst_offset + 7] = 0x00;  // Bottom row blank
    }
    
    // Fill in control characters (0x00-0x1F) with blanks or simple patterns
    for (int ch = 0; ch < 0x20; ch++) {
        for (int row = 0; row < 8; row++) {
            font_8x8[ch * 8 + row] = 0x00;
        }
    }
    
    // Fill in extended ASCII (0x60-0xFF) by duplicating or leaving blank
    for (int ch = 0x60; ch < 256; ch++) {
        for (int row = 0; row < 8; row++) {
            font_8x8[ch * 8 + row] = 0x00;
        }
    }
}

// ============================================================================
// CONNECTOR PORT SETUP — Apple 1
// ============================================================================
// Apple 1 has: 1× Expansion Connector (44-pin edge, exposes full 6502 bus)
// and 1× Cassette Interface (the Apple Cassette Interface / ACI was a
// separately sold card that plugged into the expansion slot; modeled as
// its own port since nearly all Apple 1 setups included it).

static const ConnectorDefinition apple1_expansion_def = {
    ConnectorType::EXPANSION_PORT,
    "Expansion Connector",
    ConnectorSignals::APPLE1_EXPANSION_SIGNALS,
    ConnectorSignals::APPLE1_EXPANSION_SIGNAL_COUNT,
    false, false
};

static const ConnectorDefinition apple1_cassette_def = {
    ConnectorType::CASSETTE_PORT,
    "Cassette Interface (ACI)",
    ConnectorSignals::APPLE1_CASSETTE_SIGNALS,
    ConnectorSignals::APPLE1_CASSETTE_SIGNAL_COUNT,
    false, false
};

void Apple1System::setup_connector_ports() {
    connector_ports_.clear();

    // Port 0 — Expansion Connector (44-pin edge, full 6502 bus)
    add_connector_port(apple1_expansion_def, 0);

    // Port 1 — Cassette Interface (ACI card, audio in/out)
    add_connector_port(apple1_cassette_def, 0);

    printf("Apple1: Created %zu connector ports\n", connector_ports_.size());
}

// ============================================================================
// System Registration
// ============================================================================

REGISTER_SYSTEM(apple1_descriptor, []() {
    return std::make_unique<Apple1System>();
})