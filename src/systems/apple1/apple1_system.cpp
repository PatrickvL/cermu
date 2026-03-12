#include "systems/apple1/apple1_system.h"
#include "systems/apple1/apple1_constants.h"
#include "core/chip.h"
#include <cstring>
#include <cstdio>

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#endif

// CPU type included via apple1_system.h → fam65xx.hpp

// Include ROM loader
#include "core/storage/rom_loader.h"
#include "core/config/path_discovery.h"
#include "core/system_registry.h"

// ============================================================================
// Hardware Traits Definition
// ============================================================================

static HardwareTraits create_apple1_hardware_traits() {
    HardwareTraits traits = {};
    
    // Display traits - Apple 1 used terminal display (40x24 text)
    traits.display.native_width = apple1_constants::DISPLAY_WIDTH;
    traits.display.native_height = apple1_constants::DISPLAY_HEIGHT;
    traits.display.visible_width = apple1_constants::DISPLAY_WIDTH;
    traits.display.visible_height = apple1_constants::DISPLAY_HEIGHT;
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
    traits.timing.cpu_frequency_hz = apple1_constants::CPU_FREQ;
    traits.timing.video_frequency_hz = apple1_constants::CPU_FREQ;
    traits.timing.audio_sample_rate_hz = 0;
    traits.timing.target_fps = 60;
    traits.timing.cycles_per_frame = apple1_constants::CYCLES_PER_FRAME;
    traits.timing.standard = VideoStandard::NTSC;
    
    // Memory options
    traits.memory_options.push_back({
        "4KB RAM",
        4096,
        0,
        false
    });
    traits.memory_options.push_back({
        "8KB RAM",
        apple1_constants::RAM_8K,
        0,
        true  // Default
    });
    traits.memory_options.push_back({
        "64KB RAM",
        apple1_constants::RAM_64K,
        0,
        false
    });
    
    return traits;
}

// File detection callback
static SystemProbeResult apple1_probe_file(
    const format_descriptor_t* /*matched_format*/,
    const char* filepath, const uint8_t* data, size_t size) {

    SystemProbeResult result = { 0.0f, {} };
    const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
    if (ext) {
        // Apple 1 typically used simple binary files or text files
        if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
            result.confidence = 0.4f;  // Low confidence - generic binary
        } else if (strcmp(ext, ".hex") == 0 || strcmp(ext, ".HEX") == 0) {
            result.confidence = 0.5f;  // Intel HEX format
        } else if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".TXT") == 0) {
            result.confidence = 0.3f;  // Text/source files
        }
    }
    (void)data;
    (void)size;
    return result;
}

static SystemDescriptor apple1_descriptor = {
    "Apple 1",
    "APPLE1",
    "Apple 1 (1976) - Woz's first computer, 8KB RAM, terminal display",
    "apple1",
    {"Apple1", "Apple-1", "Apple 1"},
    nullptr,  // supported_formats: Apple 1 does not use format handler system
    create_apple1_hardware_traits(),
    apple1_probe_file
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
Apple1System::Apple1System()
    : System()
    , cpu_(nullptr)
    , terminal_(nullptr)
    , cycles_per_frame_(apple1_constants::CYCLES_PER_FRAME)
    , ram_size_(apple1_constants::RAM_8K)
    , has_basic_(false)
    , cursor_col_(0)
    , cursor_row_(0)
    , pins_(APPLE1_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_apple1_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
    
    // Initialize PIA with callbacks
    pia_.init();
    pia_.user_data = this;
    pia_.on_port_a_read = pia_keyboard_read;  // Port A: keyboard input
    pia_.on_port_b_write = pia_display_write; // Port B: display output
    // DDR left at 0x00 — Woz Monitor configures PIA during boot.
}

Apple1System::~Apple1System() {
    // Destroy CPU
    if (cpu_) {
        delete cpu_;
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
    
    // ── Pre-bind PIA, then factory-create memory chips ────────────────
    bus_mem_.bind_chip(apple1_chips::kPiaSlot, &pia_);
    bus_mem_.create_chips(&pins_);
    monitor_rom_ = bus_mem_.chip_as<ROMChip>(apple1_chips::kMonitorSlot);
    basic_rom_   = bus_mem_.chip_as<ROMChip>(apple1_chips::kBasicSlot);

    // Character ROM — not on the bus (used by terminal renderer only).
    auto char_chip = std::make_unique<ROMChip>(
        ChipInfo{"2513", "Signetics"}, 512, ROMChip::ROM, &pins_,
        "CharROM");
    char_rom_ = char_chip.get();
    
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

    // ── Configure page tables for the current ram_size_ ─────────────────────
    // apply() auto-wires: RAM pages, ROM overlays, PIA MMIO + MaskedSubTable.
    // configure_bus_memory_map() then trims to actual RAM size.
    configure_bus_memory_map();
    
    // Create CPU (MOS6502) — direct C++ instantiation
    cpu_ = new MOS6502();
    if (!cpu_) {
        printf("Apple1: Failed to create MOS6502 CPU\n");
        return false;
    }
    
    // Initialize CPU (descriptor-free — memory I/O is handled via bus_state_t pins)
    cpu_->init();
    
    // Reset CPU to initialize state
    cpu_->reset(0);
    
    // Reset PIA with callbacks
    pia_.init();
    pia_.user_data = this;
    pia_.on_port_a_read = pia_keyboard_read;
    pia_.on_port_b_write = pia_display_write;
    // DDR left at 0x00 after init — the Woz Monitor sets DDRB = $7F
    // via STY $D012 during its boot sequence (control bit 2 = 0 → DDR mode).
    
    setup_connector_ports();

    // Register chips for the Hardware menu (transfers ownership of memory chips)
    register_chip(static_cast<ChipBase*>(cpu_),
        "MOS 6502 CPU", "6502", "CPU", 0x0000);
    register_chip(&pia_,
        "PIA 6820 (Keyboard/Display)", "PIA", "I/O", apple1_constants::PIA_BASE);
    register_chip(std::make_unique<ChipPlaceholder>(
        ChipInfo{"Terminal", "Custom"}, "Text Terminal (40x24)", "Terminal", "Video"));
    register_bus_chips(bus_mem_);
    register_chip(std::move(char_chip));
    
    printf("Apple1: System initialized (RAM: %dKB)\n", ram_size_ / 1024);
    return true;
}

void Apple1System::shutdown() {
    printf("Apple1: Shutting down system\n");
    System::shutdown();
}

void Apple1System::reset() {
    printf("Apple1: Resetting system\n");
    
    // Reset all manifest chips (PIA; RAM/ROM are no-op)
    bus_mem_.reset_chips();
    pia_.user_data = this;
    pia_.on_port_a_read = pia_keyboard_read;
    pia_.on_port_b_write = pia_display_write;
    
    // Clear terminal
    if (terminal_) {
        terminal_->clear(0xFF000000);
        cursor_col_ = 0;
        cursor_row_ = 0;
        terminal_->set_cursor(0, 0);
    }
    
    if (cpu_) {
        cpu_->reset(0);
    }
    pins_ = APPLE1_BUS_DEFAULT_STATE;
    total_cycles_ = 0;
}

// ============================================================================
// Execution
// ============================================================================

void Apple1System::tick() {
    tick_cpu();
    total_cycles_++;
    
    // Feed queued keystrokes into PIA at a realistic rate
    pump_paste_queue();
}

void Apple1System::run_frame() {
    uint32_t adjusted_cycles = static_cast<uint32_t>(cycles_per_frame_ * speed_multiplier_);
    for (uint32_t i = 0; i < adjusted_cycles; i++) {
        tick();
    }

    // Tick all attached peripheral devices
    tick_peripherals();

    // Render terminal to RGBA framebuffer so the emu thread snapshot
    // picks up the latest display state.
    get_framebuffer();
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
    *width = apple1_constants::DISPLAY_WIDTH;
    *height = apple1_constants::DISPLAY_HEIGHT;
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
    
    // Non-printable keys that don't come through SDL_TEXTINPUT
    if (key == '\r' || key == '\n' || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        set_keyboard_data(0x0D);  // Carriage return
    } else if (key == '\b' || key == 127 || key == SDLK_BACKSPACE) {
        set_keyboard_data(0x08);  // Backspace (Apple 1: rubout)
    } else if (key == 27 || key == SDLK_ESCAPE) {
        set_keyboard_data(0x1B);  // Escape
    }
}

void Apple1System::handle_text_input(const char* text) {
    if (!text) return;
    
    // SDL_TEXTINPUT delivers the actual typed character (including shifted
    // symbols like !, @, #, etc.).  Process each character in the string.
    for (const char* p = text; *p; ++p) {
        uint8_t ch = static_cast<uint8_t>(*p);
        
        // Apple 1 uses 7-bit ASCII; ignore anything outside printable range
        if (ch < 0x20 || ch >= 0x7F) continue;
        
        // Convert lowercase to uppercase (Apple 1 was uppercase only)
        if (ch >= 'a' && ch <= 'z') {
            ch = ch - 'a' + 'A';
        }
        
        set_keyboard_data(ch);
    }
}

// ============================================================================
// GUI Integration
// ============================================================================

void Apple1System::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    if (ImGui::MenuItem("Reset Apple 1")) {
        reset();
    }
    ImGui::Separator();
    if (has_basic_ && ImGui::MenuItem("Start BASIC")) {
        queue_text("E000R\r");
    }
    if (has_basic_ && ImGui::MenuItem("Run BASIC Demo")) {
        // Enter BASIC, then type a small test program and RUN it
        queue_text(
            "E000R\r"
            // Wait for BASIC prompt, then type the program
            "10 PRINT \"HELLO APPLE 1!\"\r"
            "20 FOR I = 1 TO 10\r"
            "30 PRINT I, I*I\r"
            "40 NEXT I\r"
            "50 PRINT \"DONE\"\r"
            "RUN\r"
        );
    }
#endif
}

void Apple1System::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
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

// ============================================================================
// Chip Registration — now done inline in initialize()
// ============================================================================

// ============================================================================
// Emulation Control
// ============================================================================

void Apple1System::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

// ============================================================================
// BUS MEMORY MAP CONFIGURATION
// ============================================================================

void Apple1System::configure_bus_memory_map() {
    using ChipId      = PT::ChipId;
    using WriteChipId = PT::WriteChipId;

    const size_t ram_pages = ram_size_ / Bus::kPageSize;  // 16 (4K), 32 (8K), or 256 (64K)

    // apply() establishes the full default map from the manifest:
    //   - All 256 RAM pages (read + write)
    //   - Monitor ROM overlays read page $FF
    //   - BASIC ROM overlays read pages $E0–$EF
    //   - PIA MMIO via MaskedSubTable on page $D0
    bus_mem_.apply(bus_);

    // ── Trim RAM to actual size ─────────────────────────────────────────────
    // Selectively unmap pages beyond actual RAM that aren't ROM-covered or
    // PIA sub-table–routed.  Check each page's current chip id to avoid
    // clobbering ROM overlays or sub-table sentinels.
    if (ram_pages < 256) {
        for (size_t page = ram_pages; page < 256; ++page) {
            auto rd = bus_.viewer(0).read_chip(page);
            auto wr = bus_.viewer(0).write_chip(page);

            // Only unmap if this page still points to its RAM chip id
            if (size_t(rd) < 256 && size_t(rd) == page)
                bus_.set_read_page(0, page, PT::kNoChipSelected);
            if (size_t(wr) < 256 && size_t(wr) == page)
                bus_.set_write_page(0, page, PT::kNoChipSelectedWrite);
        }
    }

    // ── BASIC ROM — unmap if not loaded ─────────────────────────────────────
    if (!has_basic_) {
        for (size_t i = 0; i < 16; ++i) {
            const size_t page = 0xE0 + i;
            // Restore underlying RAM (if present) or leave unmapped
            if (page < ram_pages) {
                bus_.set_read_page(0, page, ChipId(page));
            } else {
                bus_.set_read_page(0, page, PT::kNoChipSelected);
            }
        }
    }

    // ── 64K mode: ROM writes pass through to underlying RAM ─────────────────
    if (ram_size_ == apple1_constants::RAM_64K) {
        bus_.set_write_page(0, 0xFF, WriteChipId(0xFF));
        if (has_basic_) {
            for (size_t i = 0; i < 16; ++i)
                bus_.set_write_page(0, 0xE0 + i, WriteChipId(0xE0 + i));
        }
    }

    // ── PIA page ($D0) — update MaskedSubTable base chip ────────────────────
    // apply() created the sub-table with base = RAM $D0.  If RAM doesn't
    // reach $D0, switch the base to open bus.
    const int pia_sub = bus_mem_.slot(apple1_chips::kPiaSlot).sub_table_idx;
    if (pia_sub >= 0) {
        if (ram_pages > 0xD0) {
            bus_.set_masked_base(0, size_t(pia_sub),
                ChipId(0xD0), WriteChipId(0xD0));
        } else {
            bus_.set_masked_base(0, size_t(pia_sub),
                PT::kNoChipSelected, PT::kNoChipSelectedWrite);
        }
    }
}

void Apple1System::tick_cpu() {
    if (cpu_) {
        pins_ = cpu_->tick<MOS6502::Phase::PHI2>(pins_);
        pins_ = bus_.tick(0, pins_);
        pins_ = cpu_->tick<MOS6502::Phase::PHI1>(pins_);
        cpu_->sample_nmi_pin(pins_);
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
    sys->display_char(data & 0x7F);
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
    pia_.set_port_a_input(0x80 | (key_code & 0x7F));
    
    // Simulate MM5740 keyboard encoder strobe pulse:
    // Ensure CA1 is low first so the rising edge is always detected,
    // even if a previous key press left CA1 high.
    pia_.set_ca1(false);
    pia_.set_ca1(true);
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

// ============================================================================
// Keystroke Injection (paste / auto-type)
// ============================================================================

void Apple1System::queue_text(const char* text) {
    if (!text) return;
    paste_queue_ += text;
}

void Apple1System::pump_paste_queue() {
    if (paste_queue_.empty()) return;
    
    // Wait between keystrokes — the PIA needs time to process each character.
    // ~20000 cycles ≈ 20 ms at 1 MHz gives the Woz Monitor / BASIC enough
    // time to read, echo, and process each keystroke.
    if (paste_delay_cycles_ > 0) {
        paste_delay_cycles_--;
        return;
    }
    
    // Only inject when the PIA shows the previous key has been consumed
    // (bit 7 of port A is clear = strobe consumed by CPU read)
    if (keyboard_ready()) return;
    
    // Pop the next character
    char ch = paste_queue_.front();
    paste_queue_.erase(paste_queue_.begin());
    
    uint8_t key = static_cast<uint8_t>(ch);
    
    // Convert \r and \n to Apple 1 carriage return
    if (key == '\r' || key == '\n') {
        key = 0x0D;
    }
    // Convert lowercase to uppercase (Apple 1 is uppercase only)
    else if (key >= 'a' && key <= 'z') {
        key = key - 'a' + 'A';
    }
    
    set_keyboard_data(key);
    paste_delay_cycles_ = 20000;  // Delay before next character
}

bool Apple1System::load_roms() {
    // Discover ROM root using the same upward-search from executable/CWD
    // that other systems (C64, C16) use — avoids CWD dependency.
    char rom_root[1024];
    if (!system_config_discover_rom_root("apple1", rom_root, sizeof(rom_root))) {
        printf("Apple1: Could not find ROM root folder\n");
        return false;
    }
    
    // Load Woz Monitor ROM (256 bytes at $FF00-$FFFF)
    const char* monitor_files[] = {
        "apple1.rom",
        "monitor.rom",
        "wozmon.rom",
        nullptr
    };
    
    bool monitor_ok = rom_loader_load_from_root(
        rom_root, monitor_files,
        monitor_rom_->size_bytes(), monitor_rom_->data(), monitor_rom_->size_bytes()
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
        char_rom_->size_bytes(), char_rom_->data(), char_rom_->size_bytes()
    );
    
    if (char_ok && terminal_) {
        printf("Apple1: Signetics 2513 character ROM loaded\n");
        // Convert 2513 ROM format to 8x8 font for TextTerminal
        uint8_t font_8x8[256 * 8];
        memset(font_8x8, 0, sizeof(font_8x8));
        convert_2513_to_8x8_font(char_rom_->data(), font_8x8);
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
        basic_rom_->size_bytes(), basic_rom_->data(), basic_rom_->size_bytes()
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

// Convert Signetics 2513 character ROM to 8x8 font for TextTerminal
void Apple1System::convert_2513_to_8x8_font(const uint8_t* char_rom, uint8_t* font_8x8) {
    // The 2513N ROM contains 64 characters, 8 bytes each (512 bytes total).
    // Pixel data is 6 bits wide in bits 5-0 of each byte (bit 5 = leftmost).
    //
    // Character mapping in the ROM:
    //   ROM index  0-31  →  ASCII 0x40-0x5F  (@, A-Z, [, \, ], ^, _)
    //   ROM index 32-63  →  ASCII 0x20-0x3F  (space, !, ", ... 9, :, ;, ... ?)
    //
    // The TextTerminal renderer uses (0x80 >> px) to test pixels from left,
    // so we left-align the 6-bit data by shifting << 2.

    // Clear entire font table first
    memset(font_8x8, 0, 256 * 8);

    for (int ch = 0; ch < 64; ch++) {
        int src_offset = ch * 8;

        // Map ROM index to ASCII code
        int ascii;
        if (ch < 32) {
            ascii = 0x40 + ch;   // @, A-Z, [, \, ], ^, _
        } else {
            ascii = 0x20 + (ch - 32);  // space through ?
        }

        int dst_offset = ascii * 8;

        // Copy all 8 rows, left-aligning the 6-bit pixel data into bits 7-2
        for (int row = 0; row < 8; row++) {
            font_8x8[dst_offset + row] = (char_rom[src_offset + row] & 0x3F) << 2;
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