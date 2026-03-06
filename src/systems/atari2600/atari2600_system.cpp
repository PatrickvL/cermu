/*
 * atari2600_system.cpp — Atari 2600 (VCS) system implementation
 *
 * Tick loop:
 *   Each CPU cycle = 3 TIA color clocks.
 *   1. TIA tick (3 color clocks) — generates video + audio
 *   2. If TIA has WSYNC pending, skip CPU tick (CPU halted)
 *   3. CPU PHI2 → memory service → CPU PHI1
 *   4. RIOT timer tick
 *   5. Connector device state → RIOT Port A / TIA fire inputs
 *
 * Address decoding (13-bit, $0000-$1FFF):
 *   A12=0, A7=0          → TIA ($0000-$007F, mirrored)
 *   A12=0, A7=1, A9=0    → RIOT RAM ($0080-$00FF, 128 bytes)
 *   A12=0, A7=1, A9=1    → RIOT I/O ($0280-$02FF)
 *   A12=1                 → Cart ROM ($1000-$1FFF)
 *
 * Bank switching (F8 scheme for 8KB carts):
 *   Access $1FF8 → select bank 0
 *   Access $1FF9 → select bank 1
 */

#include "atari2600_system.h"
#include "../../core/system_registry.h"
#include "../../core/connector.h"
#include "../../core/vfs/vfs.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

#ifdef CERMU_HAS_GUI
#include "imgui.h"
#endif

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

static HardwareTraits create_atari2600_hardware_traits() {
    HardwareTraits traits = {};

    // Display
    traits.display.native_width    = atari2600_constants::DISPLAY_WIDTH;
    traits.display.native_height   = atari2600_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = atari2600_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = atari2600_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 128;
    traits.display.pixel_aspect_ratio = 2.0f;   // TIA pixels are roughly 2:1 aspect
    traits.display.has_overscan    = false;

    // Build palette from TIA NTSC palette
    for (int i = 0; i < 128; ++i) {
        uint32_t c = tia_t::ntsc_palette[i];
        // NTSC palette in TIA is ARGB; extract components
        uint8_t r = (c >> 16) & 0xFF;
        uint8_t g = (c >> 8)  & 0xFF;
        uint8_t b =  c        & 0xFF;
        traits.display.default_palette.push_back(PaletteColor(r, g, b, 255));
    }

    // Audio — TIA generates mono float audio (mixed from two internal channels)
    traits.audio.format             = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz     = atari2600_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels           = 1;  // Mono mix of two TIA channels
    traits.audio.chip_name          = "TIA";

    // Timing
    traits.timing.cpu_frequency_hz   = atari2600_constants::CPU_FREQ_NTSC;
    traits.timing.video_frequency_hz = atari2600_constants::TIA_FREQ_NTSC;
    traits.timing.audio_sample_rate_hz = atari2600_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps         = 60;
    traits.timing.cycles_per_frame   = atari2600_constants::CYCLES_PER_FRAME_NTSC;
    traits.timing.standard           = VideoStandard::NTSC;

    return traits;
}

// ============================================================================
// FILE PROBE
// ============================================================================

static SystemProbeResult atari2600_probe_file(
    const format_descriptor_t* /*matched_format*/,
    const char* filepath, const uint8_t* data, size_t size) {

    SystemProbeResult result = { 0.0f, {} };
    const char* ext = filepath ? strrchr(filepath, '.') : nullptr;
    if (!ext) return result;

    // Match common Atari 2600 file extensions
    if (strcmp(ext, ".a26") == 0 || strcmp(ext, ".A26") == 0) {
        result.confidence = 0.9f;   // .a26 is Atari 2600 specific
    } else if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".BIN") == 0) {
        // .bin is generic — check size for typical cart sizes
        if (size == 2048 || size == 4096 || size == 8192 ||
            size == 16384 || size == 32768) {
            result.confidence = 0.3f;  // Could be 2600, but ambiguous
        }
    }

    (void)data;
    return result;
}

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor atari2600_descriptor = {
    "Atari 2600",                       // name
    "A2600",                            // short_name
    "Atari 2600 (VCS) — 6507 CPU, TIA video/audio, 128 bytes RAM",  // description
    "atari2600",                        // data_folder
    {"Atari VCS", "VCS", "2600"},       // aliases
    nullptr,                            // supported_formats
    create_atari2600_hardware_traits(), // hardware_traits
    atari2600_probe_file                // probe
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

Atari2600System::Atari2600System()
    : EmulatedSystem()
    , pins_(ATARI2600_BUS_DEFAULT_STATE)
    , cycles_per_frame_(atari2600_constants::CYCLES_PER_FRAME_NTSC)
{
    hardware_traits_ = create_atari2600_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;

    tia_.init();
    riot_.init();
}

Atari2600System::~Atari2600System() {
    if (cpu_) {
        delete cpu_;
        cpu_ = nullptr;
    }
}

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

const SystemDescriptor& Atari2600System::get_descriptor() const {
    return atari2600_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

bool Atari2600System::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

bool Atari2600System::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool Atari2600System::initialize() {
    printf("Atari2600: Initializing system\n");

    // Create CPU
    cpu_ = new MOS6507();
    if (!cpu_) {
        printf("Atari2600: Failed to create MOS6507 CPU\n");
        return false;
    }
    cpu_->init();
    cpu_->reset(0);

    // Initialize TIA and RIOT
    tia_.init();
    tia_.set_audio_sample_rate(atari2600_constants::DEFAULT_SAMPLE_RATE);
    riot_.init();

    // Console switches default: color mode, both difficulty A, not pressed
    console_switches_ = 0xFF;  // All bits high = not pressed (active-low)

    // Setup connector ports for joysticks
    setup_connector_ports();

    // Register chips for debug/hardware menu
    register_chip(static_cast<ChipBase*>(cpu_),
        "MOS 6507 CPU", "6507", "CPU", 0x0000);
    register_chip(&tia_,
        "TIA (Television Interface Adapter)", "TIA", "Video/Audio", 0x0000);
    register_chip(&riot_,
        "PIA 6532 RIOT", "6532", "I/O", 0x0080);

    printf("Atari2600: System initialized\n");
    return true;
}

void Atari2600System::shutdown() {
    printf("Atari2600: Shutting down\n");
}

void Atari2600System::reset() {
    printf("Atari2600: Reset\n");

    tia_.reset();
    riot_.reset();

    if (cpu_) {
        cpu_->reset(0);
    }
    pins_ = ATARI2600_BUS_DEFAULT_STATE;
    total_cycles_ = 0;

    console_switches_ = 0xFF;
    joystick_state_ = 0xFF;
    frame_complete_ = false;
    in_vsync_ = false;

    // Re-setup bank switching for cart
    bank_select_ = 0;
}

// ============================================================================
// EXECUTION
// ============================================================================

void Atari2600System::tick() {
    // TIA tick: 3 color clocks per CPU cycle
    tia_.tick_cpu_cycle();

    // If WSYNC is pending, the CPU is halted — skip the CPU tick
    if (!tia_.is_cpu_halted()) {
        tick_cpu();
    }

    // RIOT timer tick (once per CPU cycle)
    riot_.tick();

    // Read joystick inputs from connector ports
    update_joystick_state();

    total_cycles_++;

    // Frame boundary detection:
    // When TIA VSYNC transitions from active to inactive, a new frame starts.
    // We detect this by watching the TIA's vsync flag.
    if (in_vsync_ && !tia_.vsync_active) {
        // VSYNC just ended — frame is complete
        frame_complete_ = true;
        tia_.scanline = 0;  // Reset scanline counter for new frame
    }
    in_vsync_ = tia_.vsync_active;
}

void Atari2600System::run_frame() {
    if (!system_ready_) return;

    frame_complete_ = false;

    // Run until TIA completes a frame (VSYNC cycle)
    // Safety limit: don't run more than 2× normal frame cycles
    uint32_t safety_limit = cycles_per_frame_ * 2;
    uint32_t cycles_run = 0;

    while (!frame_complete_ && cycles_run < safety_limit) {
        tick();
        cycles_run++;
    }

    // If we hit the safety limit, force frame completion
    if (!frame_complete_) {
        tia_.scanline = 0;
    }

    // Copy framebuffer
    get_framebuffer();

    // Tick all attached peripheral devices
    tick_peripherals();
}

// ============================================================================
// CPU TICK
// ============================================================================

void Atari2600System::tick_cpu() {
    if (cpu_) {
        pins_ = cpu_->tick<MOS6507::Phase::PHI2>(pins_);
        pins_ = mem_tick(pins_);
        pins_ = cpu_->tick<MOS6507::Phase::PHI1>(pins_);
        // MOS6507 has no IRQ pin, and NMI is unused — still call sample_nmi_pin
        // for completeness (the 6507 traits disable it internally)
        cpu_->sample_nmi_pin(pins_);
    }
}

// ============================================================================
// MEMORY ACCESS
// ============================================================================

bus_state_t Atari2600System::mem_tick(bus_state_t s) {
    // 6507 has 13-bit address bus
    uint16_t addr = BUS_GET_ADDR(s) & 0x1FFF;

    if (BUS_GET_BIT(s, BUS_RW_BIT)) {
        // ---- READ CYCLE ----
        uint8_t data = 0x00;

        if (addr & 0x1000) {
            // A12=1: Cartridge ROM
            data = cart_read(addr);
        } else if (addr & 0x0080) {
            if (addr & 0x0200) {
                // A12=0, A7=1, A9=1: RIOT I/O registers
                data = riot_.read_io(addr);
            } else {
                // A12=0, A7=1, A9=0: RIOT RAM (128 bytes)
                data = riot_.read_ram(addr & 0x7F);
            }
        } else {
            // A12=0, A7=0: TIA read registers
            data = tia_.read(addr);
        }

        BUS_SET_DATA(s, data);
    } else {
        // ---- WRITE CYCLE ----
        uint8_t data = BUS_GET_DATA(s);

        if (addr & 0x1000) {
            // A12=1: Cartridge write (bank switching hotspots)
            cart_write(addr, data);
        } else if (addr & 0x0080) {
            if (addr & 0x0200) {
                // RIOT I/O registers
                riot_.write_io(addr, data);
            } else {
                // RIOT RAM
                riot_.write_ram(addr & 0x7F, data);
            }
        } else {
            // TIA write registers
            tia_.write(addr, data);
        }
    }

    return s;
}

// ============================================================================
// CARTRIDGE ACCESS
// ============================================================================

uint8_t Atari2600System::cart_read(uint16_t addr) {
    uint16_t offset = addr & 0x0FFF;  // Offset within 4KB window

    // Check for bank switching hotspots (F8 scheme: 8KB, 2 banks)
    if (bank_count_ > 1) {
        // F8 bank switching: $1FF8 = bank 0, $1FF9 = bank 1
        if (offset == 0x0FF8) {
            bank_select_ = 0;
        } else if (offset == 0x0FF9 && bank_count_ >= 2) {
            bank_select_ = 1;
        }
        // F6 bank switching: $1FF6-$1FF9 for 16KB (4 banks)
        else if (bank_count_ >= 4) {
            if (offset >= 0x0FF6 && offset <= 0x0FF9) {
                bank_select_ = static_cast<uint8_t>(offset - 0x0FF6);
            }
        }
    }

    // Calculate ROM offset with bank selection
    uint32_t rom_offset;
    if (cart_size_ <= 2048) {
        // 2KB cart: mirror into 4KB window
        rom_offset = offset & 0x07FF;
    } else if (cart_size_ <= 4096) {
        // 4KB cart: direct mapping
        rom_offset = offset;
    } else {
        // Banked cart: offset into selected bank
        rom_offset = (static_cast<uint32_t>(bank_select_) * 0x1000) + offset;
    }

    if (rom_offset < cart_rom_.size()) {
        return cart_rom_[rom_offset];
    }
    return 0xFF;
}

void Atari2600System::cart_write(uint16_t addr, uint8_t /*data*/) {
    uint16_t offset = addr & 0x0FFF;

    // Bank switching hotspots (same as read side — reads also trigger)
    if (bank_count_ > 1) {
        if (offset == 0x0FF8) {
            bank_select_ = 0;
        } else if (offset == 0x0FF9 && bank_count_ >= 2) {
            bank_select_ = 1;
        } else if (bank_count_ >= 4) {
            if (offset >= 0x0FF6 && offset <= 0x0FF9) {
                bank_select_ = static_cast<uint8_t>(offset - 0x0FF6);
            }
        }
    }
}

// ============================================================================
// FILE LOADING
// ============================================================================

bool Atari2600System::load_file(const char* filepath) {
    if (!cpu_) {
        if (!initialize()) {
            return false;
        }
    }

    printf("Atari2600: Loading file: %s\n", filepath);

    // Read the ROM file (VFS-aware — handles archive paths like
    // "roms.7z!/Atari 2600/G/Galaxian.a26" transparently).
    size_t file_size = 0;
    uint8_t* file_data = vfs_read_file(filepath, &file_size);
    if (!file_data) {
        printf("Atari2600: Failed to open file: %s\n", filepath);
        return false;
    }

    if (file_size == 0 || file_size > 65536) {
        printf("Atari2600: Invalid file size: %zu bytes\n", file_size);
        free(file_data);
        return false;
    }

    cart_rom_.assign(file_data, file_data + file_size);
    free(file_data);

    cart_size_ = static_cast<uint32_t>(file_size);

    // Determine bank switching scheme based on size
    if (cart_size_ <= 4096) {
        bank_count_ = 1;
    } else if (cart_size_ <= 8192) {
        bank_count_ = 2;     // F8 bank switching
    } else if (cart_size_ <= 16384) {
        bank_count_ = 4;     // F6 bank switching
    } else {
        bank_count_ = static_cast<uint8_t>(cart_size_ / 4096);
    }
    bank_select_ = bank_count_ - 1;  // Start in last bank (where reset vector is)

    printf("Atari2600: Loaded %u bytes, %d bank(s)\n", cart_size_, bank_count_);

    // Set program title from filename
    const char* name = strrchr(filepath, '/');
    if (!name) name = strrchr(filepath, '\\');
    program_title_ = name ? (name + 1) : filepath;

    // Reset system to start executing
    system_ready_ = true;
    reset();

    return true;
}

// ============================================================================
// DISPLAY
// ============================================================================

uint32_t* Atari2600System::get_framebuffer() {
    // TIA renders directly into the framebuffer
    return rgba_framebuffer_;
}

void Atari2600System::get_display_dimensions(int* width, int* height) const {
    *width  = atari2600_constants::DISPLAY_WIDTH;
    *height = atari2600_constants::DISPLAY_HEIGHT;
}

void Atari2600System::set_framebuffer(uint32_t* buffer, int width, int height) {
    rgba_framebuffer_ = buffer;
    rgba_width_ = width;
    rgba_height_ = height;

    // Pass framebuffer to TIA for direct rendering
    tia_.set_framebuffer(buffer, width, height);
}

// ============================================================================
// AUDIO
// ============================================================================

uint32_t Atari2600System::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    return tia_.audio_read(buffer, max_samples);
}

void Atari2600System::set_audio_sample_rate(int sample_rate_hz) {
    tia_.set_audio_sample_rate(sample_rate_hz);
}

// ============================================================================
// INPUT
// ============================================================================

void Atari2600System::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // Console switches (active-low)
    if (key == SDLK_F1) {
        // Reset switch
        if (pressed)
            console_switches_ &= ~atari2600_constants::SWCHB_RESET;
        else
            console_switches_ |= atari2600_constants::SWCHB_RESET;
    } else if (key == SDLK_F2) {
        // Select switch
        if (pressed)
            console_switches_ &= ~atari2600_constants::SWCHB_SELECT;
        else
            console_switches_ |= atari2600_constants::SWCHB_SELECT;
    } else if (key == SDLK_F3) {
        // Color/B&W toggle (press to toggle)
        if (pressed)
            console_switches_ ^= atari2600_constants::SWCHB_BW;
    } else if (key == SDLK_F5) {
        // Player 0 difficulty toggle
        if (pressed)
            console_switches_ ^= atari2600_constants::SWCHB_P0_DIFF;
    } else if (key == SDLK_F6) {
        // Player 1 difficulty toggle
        if (pressed)
            console_switches_ ^= atari2600_constants::SWCHB_P1_DIFF;
    }

    // All joystick input flows through the peripheral device system
    // (ControlPortInputDevice → ConnectorPort signals), not through keyboard events.
}

// ============================================================================
// JOYSTICK STATE UPDATE
// ============================================================================

void Atari2600System::update_joystick_state() {
    // Read signals from connector ports and map to RIOT Port A and TIA inputs.
    //
    // RIOT Port A (SWCHA) layout:
    //   Bit 7: P0 Right     Bit 3: P1 Right
    //   Bit 6: P0 Left      Bit 2: P1 Left
    //   Bit 5: P0 Down      Bit 1: P1 Down
    //   Bit 4: P0 Up        Bit 0: P1 Up
    //
    // TIA INPT4/INPT5: fire buttons (active-low)
    //
    // Connector signals use ControlPortBit (active-low):
    //   JOY_UP=0, JOY_DOWN=1, JOY_LEFT=2, JOY_RIGHT=3, JOY_FIRE=6

    joystick_state_ = 0xFF;  // All bits high = all directions released

    // Player 0 (connector port 0)
    if (connector_ports_.size() > 0) {
        uint32_t sig0 = connector_ports_[0]->read_signals();
        // Active-low: bit is 0 when pressed
        if (!(sig0 & (1u << ConnectorSignals::JOY_UP)))    joystick_state_ &= ~0x10;
        if (!(sig0 & (1u << ConnectorSignals::JOY_DOWN)))  joystick_state_ &= ~0x20;
        if (!(sig0 & (1u << ConnectorSignals::JOY_LEFT)))  joystick_state_ &= ~0x40;
        if (!(sig0 & (1u << ConnectorSignals::JOY_RIGHT))) joystick_state_ &= ~0x80;
        // Fire button → TIA INPT4 (active-low: 0=pressed, 1=not pressed)
        tia_.inpt4 = (sig0 & (1u << ConnectorSignals::JOY_FIRE)) != 0;
    }

    // Player 1 (connector port 1)
    if (connector_ports_.size() > 1) {
        uint32_t sig1 = connector_ports_[1]->read_signals();
        if (!(sig1 & (1u << ConnectorSignals::JOY_UP)))    joystick_state_ &= ~0x01;
        if (!(sig1 & (1u << ConnectorSignals::JOY_DOWN)))  joystick_state_ &= ~0x02;
        if (!(sig1 & (1u << ConnectorSignals::JOY_LEFT)))  joystick_state_ &= ~0x04;
        if (!(sig1 & (1u << ConnectorSignals::JOY_RIGHT))) joystick_state_ &= ~0x08;
        tia_.inpt5 = (sig1 & (1u << ConnectorSignals::JOY_FIRE)) != 0;
    }

    // Write joystick state to RIOT Port A and console switches to Port B
    riot_.port_a_input = joystick_state_;
    riot_.port_b_input = console_switches_;
}

// ============================================================================
// CONNECTOR PORTS
// ============================================================================

void Atari2600System::setup_connector_ports() {
    connector_ports_.clear();

    // The Atari 2600 uses the same DB-9 joystick connector as Commodore systems.
    // We use CONTROL_PORT_DB9 ConnectorType since JoystickDevice already
    // registers as compatible with this connector type.
    static const ConnectorDefinition atari_joy_1_def = {
        ConnectorType::CONTROL_PORT_DB9,
        "Left Controller",
        ConnectorSignals::CONTROL_PORT_SIGNALS,
        ConnectorSignals::CONTROL_PORT_SIGNAL_COUNT,
        false, false
    };

    static const ConnectorDefinition atari_joy_2_def = {
        ConnectorType::CONTROL_PORT_DB9,
        "Right Controller",
        ConnectorSignals::CONTROL_PORT_SIGNALS,
        ConnectorSignals::CONTROL_PORT_SIGNAL_COUNT,
        false, false
    };

    add_connector_port(atari_joy_1_def, 1);  // Player 1
    add_connector_port(atari_joy_2_def, 2);  // Player 2

    printf("Atari2600: Created %zu connector ports\n", connector_ports_.size());

    // Attach default peripherals (joysticks) and auto-bind host inputs
    attach_default_peripherals();
}

std::vector<EmulatedSystem::DefaultPeripheral>
Atari2600System::get_default_peripherals() const {
    return {
        { 0, "joystick" },   // Left Controller
        { 1, "joystick" },   // Right Controller
    };
}

// ============================================================================
// GUI
// ============================================================================

void Atari2600System::render_system_menu_items() {
#ifdef CERMU_HAS_GUI
    if (ImGui::MenuItem("Reset Atari 2600")) {
        reset();
    }
    ImGui::Separator();
    // Console switch controls
    bool color_mode = (console_switches_ & atari2600_constants::SWCHB_BW) != 0;
    if (ImGui::MenuItem("Color Mode", nullptr, color_mode)) {
        console_switches_ ^= atari2600_constants::SWCHB_BW;
    }
    bool p0_diff_a = (console_switches_ & atari2600_constants::SWCHB_P0_DIFF) != 0;
    if (ImGui::MenuItem("P0 Difficulty A", nullptr, p0_diff_a)) {
        console_switches_ ^= atari2600_constants::SWCHB_P0_DIFF;
    }
    bool p1_diff_a = (console_switches_ & atari2600_constants::SWCHB_P1_DIFF) != 0;
    if (ImGui::MenuItem("P1 Difficulty A", nullptr, p1_diff_a)) {
        console_switches_ ^= atari2600_constants::SWCHB_P1_DIFF;
    }
#endif
}

void Atari2600System::render_configuration_ui() {
#ifdef CERMU_HAS_GUI
    ImGui::Text("Atari 2600 Configuration");
    ImGui::Separator();
    ImGui::Text("Cartridge: %s",
        system_ready_ ? program_title_.c_str() : "No cartridge loaded");
    if (system_ready_) {
        ImGui::Text("ROM Size: %u bytes (%d bank%s)",
            cart_size_, bank_count_, bank_count_ > 1 ? "s" : "");
    }
    ImGui::Separator();
    ImGui::Text("Console Switches:");
    ImGui::Text("  F1 = Reset, F2 = Select");
    ImGui::Text("  F3 = Color/B&W toggle");
    ImGui::Text("  F5 = P0 Difficulty, F6 = P1 Difficulty");
#endif
}

// ============================================================================
// EMULATION CONTROL
// ============================================================================

void Atari2600System::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(atari2600_descriptor, []() {
    return std::make_unique<Atari2600System>();
})
