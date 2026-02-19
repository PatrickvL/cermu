#include "c16_system.h"
#include "c16_keyboard_matrix.h"
#include "../../chip/input/emu_key_sdl_map.h"
#include "../../core/storage/rom_loader.h"
#include "../../core/formats/format_registry.h"
#include "../../core/formats/prg_format.h"
#include "../../core/formats/d64_format.h"
#include "../../core/formats/t64_format.h"
#include "../../core/formats/tap_format.h"
#include "../../core/formats/crt_format.h"
#include "../../core/formats/lnx_format.h"
#include "../../core/formats/commodore_load_helpers.h"
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
            // Check TAP header to see if this is specifically a C16/Plus4 tape
            int platform = commodore_tap_identify_platform(filepath);
            if (platform == 2) return 0.95f;  // C16 TAP
            if (platform == 0) return 0.2f;   // C64 TAP (low for C16)
            return 0.4f;
        }
        if (strcmp(ext, ".d64") == 0 || strcmp(ext, ".D64") == 0) {
            return 0.4f;
        }
        if (strcmp(ext, ".t64") == 0 || strcmp(ext, ".T64") == 0) {
            return 0.4f;  // T64 archives are usually C64, but can contain C16
        }
    }
    return 0.0f;
}

/** Formats the C16/Plus4 can load — used by SystemDescriptor and file dialogs. */
static const format_descriptor_t* const c16_formats[] = {
    &PRG_FORMAT_DESCRIPTOR, &TAP_FORMAT_DESCRIPTOR, &D64_FORMAT_DESCRIPTOR,
    &T64_FORMAT_DESCRIPTOR, &LNX_FORMAT_DESCRIPTOR, &BIN_FORMAT_DESCRIPTOR,
    nullptr
};

const SystemDescriptor C16System::c16_descriptor = {
    "Commodore 16 / Plus/4",
    "C16",
    "Commodore 16 and Plus/4 (1984) - 16KB/64KB RAM, TED graphics",
    c16_formats,
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
    
    setup_connector_ports();
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

    // Tick all attached peripheral devices
    tick_peripherals();
}

// ============================================================================
// File Loading
// ============================================================================

// ============================================================================
// File Loading
// ============================================================================

// ============================================================================
// Commodore Load Helper Callbacks -- C16-specific
// ============================================================================

static uint8_t c16_mem_read(void* ctx, uint16_t addr) {
    return static_cast<uint8_t*>(ctx)[addr];
}

static void c16_mem_write_byte(void* ctx, uint16_t addr, uint8_t val) {
    static_cast<uint8_t*>(ctx)[addr] = val;
}

static void c16_mem_write_block(void* ctx, uint16_t addr,
                                const uint8_t* data, size_t len) {
    memcpy(&static_cast<uint8_t*>(ctx)[addr], data, len);
}

bool C16System::load_file(const char* filepath) {
    if (!initialized_) {
        printf("C16: System not initialized, initializing now...\n");
        if (!initialize()) {
            printf("C16: Failed to initialize system for file loading\n");
            return false;
        }
    }
    
    printf("C16: Loading file: %s\n", filepath);

    format_load_result_t result = {};
    if (!format_load_file(filepath, &result)) {
        printf("C16: Failed to load file: %s\n", result.error_msg);
        format_load_result_free(&result);
        return false;
    }

    commodore_load_context_t ctx = {};
    ctx.system_name     = "C16";
    ctx.write_byte      = c16_mem_write_byte;
    ctx.write_block     = c16_mem_write_block;
    ctx.mem_read        = c16_mem_read;
    ctx.mem_ctx         = ram_simple_;
    ctx.basic_params    = &COMMODORE_BASIC_C16;
    ctx.basic_start_addrs[0] = 0x1001;
    ctx.default_raw_addr = 0x4000;
    ctx.set_pc          = nullptr;  // MOS7501 CPU not yet implemented

    bool success = commodore_apply_load_result(&ctx, &result, filepath);

    format_load_result_free(&result);
    return success;
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

void C16System::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    if (keyboard_mapper_) {
        if (pressed) {
            keyboard_mapper_->process_key_down(key, SDL_SCANCODE_UNKNOWN, 0, false);
        } else {
            keyboard_mapper_->process_key_up(key, SDL_SCANCODE_UNKNOWN, 0);
        }
    } else if (keyboard_) {
        emu_key_t ek = EmuKeySDLMap::instance().sdl_keycode_to_emu_key(key);
        if (ek != EMUKEY_NONE) {
            if (pressed) {
                commodore_keyboard_key_down(keyboard_, ek, false);
            } else {
                commodore_keyboard_key_up(keyboard_, ek, false);
            }
        }
    }
}

void C16System::handle_keyboard_event_ex(SDL_Keycode key, SDL_Scancode scancode, uint16_t mod, bool pressed, bool repeat) {
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
// CONNECTOR PORT SETUP — C16/Plus4
// ============================================================================
// C16/Plus4 has: 2× Joystick ports (mini-DIN, electrically DB-9 compatible,
// directly read by TED — no paddles), IEC Serial Bus, Cassette Port,
// User Port (Plus/4 only), and Expansion Port (cartridge slot).
//
// Joystick port signals are a subset of the standard DB-9 control port:
// UP, DOWN, LEFT, RIGHT, FIRE — no analog paddle lines (no SID POT inputs).

static const ConnectorDefinition c16_joy_port_1_def = {
    ConnectorType::CONTROL_PORT_DB9,
    "Joystick Port 1",
    ConnectorSignals::CONTROL_PORT_SIGNALS,
    ConnectorSignals::CONTROL_PORT_SIGNAL_COUNT
};

static const ConnectorDefinition c16_joy_port_2_def = {
    ConnectorType::CONTROL_PORT_DB9,
    "Joystick Port 2",
    ConnectorSignals::CONTROL_PORT_SIGNALS,
    ConnectorSignals::CONTROL_PORT_SIGNAL_COUNT
};

static const ConnectorDefinition c16_iec_serial_def = {
    ConnectorType::IEC_SERIAL,
    "IEC Serial Bus",
    ConnectorSignals::IEC_SERIAL_SIGNALS,
    ConnectorSignals::IEC_SERIAL_SIGNAL_COUNT
};

static const ConnectorDefinition c16_cassette_def = {
    ConnectorType::CASSETTE_PORT,
    "Cassette Port",
    ConnectorSignals::CASSETTE_PORT_SIGNALS,
    ConnectorSignals::CASSETTE_PORT_SIGNAL_COUNT
};

static const ConnectorDefinition c16_user_port_def = {
    ConnectorType::USER_PORT,
    "User Port",
    ConnectorSignals::USER_PORT_SIGNALS,
    ConnectorSignals::USER_PORT_SIGNAL_COUNT
};

static const SignalLine c16_expansion_signals[] = {
    { "/RESET", SignalDirection::OUTPUT, 0 },
    { "/IRQ",   SignalDirection::INPUT,  1 },
};
static const ConnectorDefinition c16_expansion_def = {
    ConnectorType::EXPANSION_PORT,
    "Expansion Port",
    c16_expansion_signals,
    2
};

void C16System::setup_connector_ports() {
    connector_ports_.clear();

    // Port 0 — Joystick Port 1 (directly scanned by TED $FF08)
    add_connector_port(c16_joy_port_1_def, 1);

    // Port 1 — Joystick Port 2 (directly scanned by TED $FF08)
    add_connector_port(c16_joy_port_2_def, 2);

    // Port 2 — IEC Serial Bus (disk drive, printer)
    add_connector_port(c16_iec_serial_def, 0);

    // Port 3 — Cassette Port (datasette, mini-DIN connector)
    add_connector_port(c16_cassette_def, 0);

    // Port 4 — User Port (Plus/4 only; directly connected to 6529 port chip)
    add_connector_port(c16_user_port_def, 0);

    // Port 5 — Expansion Port (cartridge slot)
    add_connector_port(c16_expansion_def, 0);

    printf("C16: Created %zu connector ports\n", connector_ports_.size());
}

// ============================================================================
// System Registration
// ============================================================================

REGISTER_SYSTEM(C16System::c16_descriptor, []() {
    return std::make_unique<C16System>();
})