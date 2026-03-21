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
 * Bank switching is delegated to A2600Mapper subclasses, auto-detected
 * by ROM size and content analysis in a2600_mapper_factory.
 */

#include "systems/atari2600/atari2600_system.hpp"
#include "systems/atari2600/mappers/a2600_mapper_factory.hpp"
#include "core/system_registry.hpp"
#include "core/port.hpp"
#include "core/vfs/vfs.hpp"
#include <cstring>
#include <cstdio>
#include <algorithm>

#ifdef CERMU_HAS_GUI
#include <imgui.h>
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
            size == 12288 || size == 16384 || size == 32768 ||
            size == 65536 || size == 131072 || size == 262144 ||
            size == 524288) {
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
    : System()
    , pins_(ATARI2600_BUS_DEFAULT_STATE)
    , cycles_per_frame_(atari2600_constants::CYCLES_PER_FRAME_NTSC)
{
    hardware_traits_ = create_atari2600_hardware_traits();
    current_palette_ = hardware_traits_.display.default_palette;
}

Atari2600System::~Atari2600System() = default;

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
    register_board(&board_);

    // ── Factory-create ALL chips from the manifest ──────────────────────
    board_.bind_chipset();
    board_.create_chips(&pins_);

    // ── Configure MemoryBus page tables (mirrors + cart pages) ──────────
    configure_bus_memory_map();

    // ── Initialize chips ───────────────────────────────────────────
    board_.cpu().init();
    board_.cpu().reset();
    board_.vdp().init();
    board_.vdp().set_audio_sample_rate(atari2600_constants::DEFAULT_SAMPLE_RATE);
    board_.chips().riot.init();

    // Console switches default: color mode, both difficulty A, not pressed
    console_switches_ = 0xFF;  // All bits high = not pressed (active-low)

    // Setup connector ports for joysticks
    setup_ports();

    // Register all manifest-created chips for the Hardware menu
    register_bus_chips(board_);

    // GPU indexed palette rendering — 128-color TIA NTSC palette
    display_.init(atari2600_constants::DISPLAY_WIDTH,
                  atari2600_constants::DISPLAY_HEIGHT);
    display_.set_palette(board_.vdp().palette_rgba_, 128);
    board_.vdp().set_display(&display_);
    register_display(&display_);

    // Video stream output — composite video from TIA
    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.vdp().set_stream(&video_port_->stream());
    video_port_->bind_display(&display_, board_.vdp().palette_rgba_);
    video_port_->bind_frame_output(&last_frame_data_);

    // Audio port — TIA does its own decimation, uses drive_sample()
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(atari2600_constants::DEFAULT_SAMPLE_RATE,
                           atari2600_constants::DEFAULT_SAMPLE_RATE);
    board_.vdp().set_audio_port(audio_port_.get());

    printf("Atari2600: System initialized\n");
    return true;
}

void Atari2600System::shutdown() {
    printf("Atari2600: Shutting down\n");
    System::shutdown();
}

void Atari2600System::reset() {
    printf("Atari2600: Reset\n");

    // Reset all manifest chips (TIA, RIOT; CartChip is no-op)
    board_.reset_chips();
    board_.cpu().reset();
    pins_ = ATARI2600_BUS_DEFAULT_STATE;
    total_cycles_ = 0;

    console_switches_ = 0xFF;
    joystick_state_ = 0xFF;
    frame_complete_ = false;
    in_vsync_ = false;

    // Reset mapper to initial bank state
    if (mapper_)
        mapper_->reset();
}

// ============================================================================
// EXECUTION
// ============================================================================

void Atari2600System::tick() {
    // TIA tick: 3 color clocks per CPU cycle
    board_.vdp().tick_cpu_cycle();

    // If WSYNC is pending, the CPU is halted — skip the CPU tick
    if (!board_.vdp().is_cpu_halted()) {
        tick_cpu();
    }

    // RIOT timer tick (once per CPU cycle)
    board_.chips().riot.tick();

    // Read joystick inputs from connector ports
    update_joystick_state();

    total_cycles_++;

    // Frame boundary detection:
    // When TIA VSYNC transitions from active to inactive, reset the TIA
    // scanline counter for the new frame.  Frame sync itself is now
    // stream-driven (TIA drives FrameEnd on VSYNC rising edge).
    bool vsync_active = (board_.vdp().regs_[TIA_VSYNC] & 0x02) != 0;
    if (in_vsync_ && !vsync_active) {
        board_.vdp().scanline = 0;  // Reset scanline counter for new frame
    }
    in_vsync_ = vsync_active;
}

void Atari2600System::run_frame() {
    if (!system_ready_ || !video_port_) return;

    // Stream-driven: TIA drives FrameEnd on VSYNC rising edge.
    // Safety limit protects against games that never trigger VSYNC.
    auto& stream = video_port_->stream();
    uint32_t safety_limit = cycles_per_frame_ * 2;
    uint32_t cycles_run = 0;

    while (!stream.frame_ended() && cycles_run < safety_limit) {
        tick();
        cycles_run++;
    }

    // If we hit the safety limit, force scanline reset
    if (!stream.frame_ended()) {
        board_.vdp().scanline = 0;
    }

    video_port_->swap_frame();

    // Tick all attached peripheral devices
    tick_peripherals();
}

// ============================================================================
// CPU TICK
// ============================================================================

void Atari2600System::tick_cpu() {
    {
        pins_ = board_.cpu().tick<MOS6507::Phase::PHI2>(pins_);

        // Memory dispatch through MemoryBus (TIA, RIOT, Cart all via MMIO)
        pins_ = bus_.tick(pins_);

        // Bus snooping for mappers that monitor all accesses
        // (e.g. 3F watches TIA writes, FE watches stack at $01FE)
        if (mapper_snoop_) {
            uint16_t addr = BUS_GET_ADDR(pins_);
            mapper_->bus_snoop(addr, BUS_GET_DATA(pins_),
                               !BUS_GET_BIT(pins_, BUS_RW_BIT));
        }

        pins_ = board_.cpu().tick<MOS6507::Phase::PHI1>(pins_);
        // MOS6507 has no IRQ pin, and NMI is unused — still call sample_nmi_pin
        // for completeness (the 6507 traits disable it internally)
        board_.cpu().sample_nmi_pin(pins_);
    }
}

// ============================================================================
// BUS MEMORY MAP CONFIGURATION
// ============================================================================

void Atari2600System::configure_bus_memory_map() {
    // apply() auto-wires everything declaratively:
    //   - Pages 0-15: MaskedSubTable with TIA (A7=0) + RIOT (A7=1) regions,
    //     mirrored via bank_size=4096 on the TIA/RIOT slots.
    //   - Pages 16-31: full-page MMIO for the cartridge mapper,
    //     mirrored via bank_size=4096 on the Cart slot.
    board_.apply(bus_);
}

// ============================================================================
// FILE LOADING
// ============================================================================

bool Atari2600System::load_file(const char* filepath) {
    // If a cartridge was already loaded, perform a full cold-boot reset so
    // that no stale CPU register values, framebuffer pixels, or audio
    // samples leak from the previous program into the new one.
    bool cold_boot = system_ready_;

    if (!system_ready_) {
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

    if (file_size == 0 || file_size > 524288) {
        printf("Atari2600: Invalid file size: %zu bytes\n", file_size);
        free(file_data);
        return false;
    }

    cart_rom_.assign(file_data, file_data + file_size);
    free(file_data);

    cart_size_ = static_cast<uint32_t>(file_size);

    // Auto-detect banking scheme and create mapper
    mapper_ = a2600_mapper_factory::create(cart_rom_.data(), cart_size_);
    mapper_snoop_ = mapper_->needs_bus_snoop();

    // Wire the mapper into the cart MMIO adapter for MemoryBus dispatch
    board_.chips().cart.set_mapper(mapper_.get());

    printf("Atari2600: Loaded %u bytes, mapper=%s, %d bank(s)\n",
           cart_size_, mapper_->name(), mapper_->bank_count());

    // Set program title from filename
    const char* name = strrchr(filepath, '/');
    if (!name) name = strrchr(filepath, '\\');
    program_title_ = name ? (name + 1) : filepath;

    // Reset system to start executing
    system_ready_ = true;
    reset();

    // Cold boot: zero CPU registers and clear framebuffer/audio so nothing
    // from the previous program bleeds through.
    if (cold_boot) {
        board_.cpu().set(REG_A,   0);
        board_.cpu().set(REG_X,   0);
        board_.cpu().set(REG_Y,   0);
        board_.cpu().set(REG_SPL, 0xFD);  // Power-on stack pointer
        board_.cpu().set(REG_P,   0x24);  // I flag set, unused bit 5 set

        if (rgba_framebuffer_) {
            memset(rgba_framebuffer_,
                   0,
                   static_cast<size_t>(rgba_width_) * rgba_height_ * sizeof(uint32_t));
        }

        // Flush stale audio from the ring buffer
        board_.vdp().audio_buffer_.reset();
    }

    return true;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

uint32_t Atari2600System::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return board_.vdp().audio_read(buffer, max_samples);
}

void Atari2600System::set_audio_sample_rate(int sample_rate_hz) {
    board_.vdp().set_audio_sample_rate(sample_rate_hz);
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
    // (ControlPortInputDevice → Port signals), not through keyboard events.
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
    if (get_ports().size() > 0) {
        uint32_t sig0 = get_port(0)->read_signals();
        // Active-low: bit is 0 when pressed
        if (!(sig0 & (1u << PortSignals::JOY_UP)))    joystick_state_ &= ~0x10;
        if (!(sig0 & (1u << PortSignals::JOY_DOWN)))  joystick_state_ &= ~0x20;
        if (!(sig0 & (1u << PortSignals::JOY_LEFT)))  joystick_state_ &= ~0x40;
        if (!(sig0 & (1u << PortSignals::JOY_RIGHT))) joystick_state_ &= ~0x80;
        // Fire button → TIA INPT4 (active-low: 0=pressed, 1=not pressed)
        board_.vdp().read_regs_[TIA_INPT4] = (sig0 & (1u << PortSignals::JOY_FIRE)) ? 0x80 : 0x00;
    }

    // Player 1 (connector port 1)
    if (get_ports().size() > 1) {
        uint32_t sig1 = get_port(1)->read_signals();
        if (!(sig1 & (1u << PortSignals::JOY_UP)))    joystick_state_ &= ~0x01;
        if (!(sig1 & (1u << PortSignals::JOY_DOWN)))  joystick_state_ &= ~0x02;
        if (!(sig1 & (1u << PortSignals::JOY_LEFT)))  joystick_state_ &= ~0x04;
        if (!(sig1 & (1u << PortSignals::JOY_RIGHT))) joystick_state_ &= ~0x08;
        board_.vdp().read_regs_[TIA_INPT5] = (sig1 & (1u << PortSignals::JOY_FIRE)) ? 0x80 : 0x00;
    }

    // Write joystick state to RIOT Port A and console switches to Port B
    board_.chips().riot.port_a_input = joystick_state_;
    board_.chips().riot.port_b_input = console_switches_;
}

// ============================================================================
// CONNECTOR PORTS
// ============================================================================

void Atari2600System::setup_ports() {

    // The Atari 2600 uses the same DB-9 joystick connector as Commodore systems.
    // We use CONTROL_PORT_DB9 PortType since JoystickDevice already
    // registers as compatible with this connector type.
    static const PortDefinition atari_joy_1_def = {
        PortType::CONTROL_PORT_DB9,
        "Left Controller",
        PortSignals::CONTROL_PORT_SIGNALS,
        PortSignals::CONTROL_PORT_SIGNAL_COUNT,
        false, false
    };

    static const PortDefinition atari_joy_2_def = {
        PortType::CONTROL_PORT_DB9,
        "Right Controller",
        PortSignals::CONTROL_PORT_SIGNALS,
        PortSignals::CONTROL_PORT_SIGNAL_COUNT,
        false, false
    };

    add_port(atari_joy_1_def, 1);  // Player 1
    add_port(atari_joy_2_def, 2);  // Player 2

    printf("Atari2600: Created %zu ports\n", get_ports().size());
}

std::vector<System::DefaultPeripheral>
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
    if (system_ready_ && mapper_) {
        ImGui::Text("ROM Size: %u bytes", cart_size_);
        ImGui::Text("Mapper: %s (%d bank%s, current: %d)",
            mapper_->name(), mapper_->bank_count(),
            mapper_->bank_count() > 1 ? "s" : "",
            mapper_->current_bank());
    }
    ImGui::Separator();
    ImGui::Text("Console Switches:");
    ImGui::Text("  F1 = Reset, F2 = Select");
    ImGui::Text("  F3 = Color/B&W toggle");
    ImGui::Text("  F5 = P0 Difficulty, F6 = P1 Difficulty");
#endif
}

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(atari2600_descriptor, []() {
    return std::make_unique<Atari2600System>();
})
