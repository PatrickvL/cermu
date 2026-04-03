/*
 * tatung_einstein_system.cpp — Tatung Einstein TC-01 system implementation
 *
 * Tick loop:
 *   Z80 one T-state per tick(), VDP dot clock (PAL TMS9929A),
 *   AY at CPU/16, CTC at CPU rate, PIO as needed.
 *
 * I/O map:
 *   $00:     PSG address latch
 *   $01:     PSG data write
 *   $02:     PSG data read
 *   $03:     VDP data
 *   $04:     VDP control/status
 *   $08-$0B: CTC channels
 *   $10-$13: PIO
 *   $23:     ROM bank control
 */

#include "core/cermu.hpp"
#include "systems/tatung_einstein/tatung_einstein_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

static HardwareTraits create_einstein_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = einstein_constants::DISPLAY_WIDTH;
    traits.display.native_height   = einstein_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = einstein_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = einstein_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = einstein_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "AY-3-8910";

    traits.timing.cpu_frequency_hz   = einstein_constants::CPU_FREQ_HZ;
    traits.timing.target_fps         = 50;
    traits.timing.cycles_per_frame   = einstein_constants::TSTATES_PER_FRAME;
    traits.timing.standard           = VideoStandard::PAL;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor einstein_descriptor = {
    "Tatung Einstein", "Einstein",
    "Tatung Einstein TC-01 — Z80A @ 4MHz, TMS9929A, AY-3-8910, CTC, PIO (1984)",
    "tatung_einstein", {"Einstein", "TatungEinstein", "TC-01"},
    nullptr,
    create_einstein_hardware_traits(),
    nullptr,
    "Tatung", 1984, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

TatungEinsteinSystem::TatungEinsteinSystem()
    : System()
    , pins_(EINSTEIN_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_einstein_hardware_traits();
}

TatungEinsteinSystem::~TatungEinsteinSystem() = default;

const SystemDescriptor& TatungEinsteinSystem::get_descriptor() const {
    return einstein_descriptor;
}

bool TatungEinsteinSystem::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

bool TatungEinsteinSystem::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

bool TatungEinsteinSystem::initialize() {
    log_info("Tatung Einstein: Initializing system\n");
    register_board(&board_);

    { size_t slot_idx_ = 0;
      EINSTEIN_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_BIND_SEQUENTIAL, board_) }
    board_.create_chips(&pins_);
    board_.apply(bus_);

    configure_bus_memory_map();

    pins_ = board_.z80.init();
    board_.psg.init();
    board_.ctc.init();
    board_.pio.init();

    board_.psg.set_clock_frequency(einstein_constants::CPU_FREQ_HZ / 16);
    board_.psg.set_audio_sample_rate(audio_sample_rate_);
    audio_sample_period_ = einstein_constants::CPU_FREQ_HZ / audio_sample_rate_;

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    if (!load_roms()) {
        log_info("Tatung Einstein: Warning — ROMs not loaded\n");
    }

    register_bus_chips(board_);

    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.vdp.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(einstein_constants::DEFAULT_SAMPLE_RATE,
                           einstein_constants::DEFAULT_SAMPLE_RATE);

    system_ready_ = true;
    log_info("Tatung Einstein: System initialized (64KB RAM)\n");
    return true;
}

void TatungEinsteinSystem::shutdown() {
    system_ready_ = false;
}

void TatungEinsteinSystem::reset() {
    board_.reset_chips();
    pins_ = board_.z80.reset(pins_);
    frame_tstate_counter_ = 0;
    rom_enabled_ = true;
    configure_bus_memory_map();
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
}

// ============================================================================
// EXECUTION
// ============================================================================

void TatungEinsteinSystem::tick() {

    // VDP tick (PAL TMS9929A)
    bus_state_t vdp_bus = 0;
    vdp_bus = board_.vdp.tick(vdp_bus);

    if (BUS_GET_BIT(vdp_bus, BUS_IRQ_BIT) == 0)
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    else
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);

    // CTC tick
    board_.ctc.tick();
    if (board_.ctc.interrupt_pending()) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    }

    // CPU tick
    pins_ = board_.z80.tick(pins_);

    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // PSG tick (CPU/16)
    if ((frame_tstate_counter_ & 0x0F) == 0) {
        board_.psg.tick();
    }

    // Audio sample generation
    audio_sample_counter_++;
    if (audio_sample_counter_ >= audio_sample_period_) {
        audio_sample_counter_ = 0;
        float sample = board_.psg.get_sample();
        audio_ring_buf_.write(&sample, 1);
        if (audio_port_) audio_port_->drive_sample(sample);
    }

    frame_tstate_counter_++;
    total_cycles_++;
}

void TatungEinsteinSystem::run_frame() {
    if (!video_port_) return;
    auto& output = video_port_->output();
    while (!output.frame_ended()) {
        tick();
    }
    video_port_->swap_frame();
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

void TatungEinsteinSystem::configure_bus_memory_map() {
    board_.apply(bus_);
    // ROM overlay is managed by the board's chip manifest:
    // ROM at slot 1, RAM at slot 0. ROM overlays reads at $0000-$1FFF.
    // When rom_enabled_ is false, we disable the ROM overlay.
    if (!rom_enabled_) {
        // TODO: Disable ROM read overlay so RAM is visible at $0000
    }
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

bus_state_t TatungEinsteinSystem::io_tick(bus_state_t pins) {
    uint8_t port = BUS_GET_ADDR(pins) & 0xFF;
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    // PSG address latch ($00)
    if (!is_read && port == einstein_constants::PSG_ADDR_PORT) {
        board_.psg.latch_address(BUS_GET_DATA(pins));
        return pins;
    }

    // PSG data write ($01)
    if (!is_read && port == einstein_constants::PSG_DATA_WRITE_PORT) {
        board_.psg.write_register(BUS_GET_DATA(pins));
        return pins;
    }

    // PSG data read ($02)
    if (is_read && port == einstein_constants::PSG_DATA_READ_PORT) {
        BUS_SET_DATA(pins, board_.psg.read_register());
        return pins;
    }

    // VDP data ($03)
    if (port == einstein_constants::VDP_DATA_PORT) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, 0);  // port 0 = data
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = TMS9929A::port_read(&board_.vdp, vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            TMS9929A::port_write(&board_.vdp, vdp_bus);
        }
        return pins;
    }

    // VDP control ($04)
    if (port == einstein_constants::VDP_CTRL_PORT) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, 1);  // port 1 = control/status
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = TMS9929A::port_read(&board_.vdp, vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            TMS9929A::port_write(&board_.vdp, vdp_bus);
        }
        return pins;
    }

    // CTC channels ($08-$0B)
    if (port >= einstein_constants::CTC_BASE_PORT &&
        port <= (einstein_constants::CTC_BASE_PORT + 3)) {
        int channel = port - einstein_constants::CTC_BASE_PORT;
        if (is_read)
            BUS_SET_DATA(pins, board_.ctc.read(channel));
        else
            board_.ctc.write(channel, BUS_GET_DATA(pins));
        return pins;
    }

    // PIO ($10-$13)
    if (port >= einstein_constants::PIO_BASE_PORT &&
        port <= (einstein_constants::PIO_BASE_PORT + 3)) {
        pins = board_.pio.io_tick(pins);
        return pins;
    }

    // ROM bank control ($23) — write disables ROM overlay
    if (!is_read && port == einstein_constants::ROM_BANK_PORT) {
        rom_enabled_ = false;
        configure_bus_memory_map();
        return pins;
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

bool TatungEinsteinSystem::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;
    log_info("Tatung Einstein: File loading not yet implemented: %s\n", filepath);
    return false;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

uint32_t TatungEinsteinSystem::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return static_cast<uint32_t>(
        audio_ring_buf_.read(buffer, static_cast<size_t>(max_samples)));
}

void TatungEinsteinSystem::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    audio_sample_period_ = einstein_constants::CPU_FREQ_HZ / audio_sample_rate_;
    board_.psg.set_audio_sample_rate(sample_rate_hz);
}

// ============================================================================
// INPUT
// ============================================================================

void TatungEinsteinSystem::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // Einstein keyboard matrix via AY-3-8910 I/O ports
    struct KeyMapping { SDL_Keycode sdl_key; int row; int bit; };
    static constexpr KeyMapping mappings[] = {
        // Row 0
        { SDLK_1, 0, 0 }, { SDLK_2, 0, 1 }, { SDLK_3, 0, 2 }, { SDLK_4, 0, 3 },
        { SDLK_5, 0, 4 }, { SDLK_6, 0, 5 }, { SDLK_7, 0, 6 }, { SDLK_8, 0, 7 },
        // Row 1
        { SDLK_q, 1, 0 }, { SDLK_w, 1, 1 }, { SDLK_e, 1, 2 }, { SDLK_r, 1, 3 },
        { SDLK_t, 1, 4 }, { SDLK_y, 1, 5 }, { SDLK_u, 1, 6 }, { SDLK_i, 1, 7 },
        // Row 2
        { SDLK_a, 2, 0 }, { SDLK_s, 2, 1 }, { SDLK_d, 2, 2 }, { SDLK_f, 2, 3 },
        { SDLK_g, 2, 4 }, { SDLK_h, 2, 5 }, { SDLK_j, 2, 6 }, { SDLK_k, 2, 7 },
        // Row 3
        { SDLK_z, 3, 0 }, { SDLK_x, 3, 1 }, { SDLK_c, 3, 2 }, { SDLK_v, 3, 3 },
        { SDLK_b, 3, 4 }, { SDLK_n, 3, 5 }, { SDLK_m, 3, 6 },
        // Row 4
        { SDLK_9, 4, 0 }, { SDLK_0, 4, 1 }, { SDLK_MINUS, 4, 2 }, { SDLK_EQUALS, 4, 3 },
        { SDLK_o, 4, 4 }, { SDLK_p, 4, 5 }, { SDLK_l, 4, 6 },
        // Row 5
        { SDLK_RETURN, 5, 0 }, { SDLK_SPACE, 5, 1 }, { SDLK_BACKSPACE, 5, 2 },
        { SDLK_ESCAPE, 5, 3 },
        // Row 6 — modifiers
        { SDLK_LSHIFT, 6, 0 }, { SDLK_RSHIFT, 6, 0 }, { SDLK_LCTRL, 6, 1 },
        // Row 7 — cursor keys
        { SDLK_UP, 7, 0 }, { SDLK_DOWN, 7, 1 }, { SDLK_LEFT, 7, 2 }, { SDLK_RIGHT, 7, 3 },
    };

    for (const auto& m : mappings) {
        if (m.sdl_key == key) {
            if (pressed)
                keyboard_matrix_[m.row] &= ~(1 << m.bit);
            else
                keyboard_matrix_[m.row] |= (1 << m.bit);
        }
    }
}

// ============================================================================
// ROM LOADING
// ============================================================================

bool TatungEinsteinSystem::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root("tatung_einstein", rom_root, sizeof(rom_root))) {
        log_info("Tatung Einstein: Could not find ROM root folder\n");
        return false;
    }
    return board_.load_roms(rom_root, "Tatung Einstein");
}

// ============================================================================
// PORT MANIFEST
// ============================================================================
//                                      tag       type              name                  num  int  bus  default_device
#define EINSTEIN_FOR_EACH_PORT(V, ctx) \
    V(ctx, EXPANSION,  EXPANSION_PORT,   "Expansion Port",       0, false, false, nullptr)         \
    V(ctx, VIDEO,      VIDEO_COMPOSITE,  "Video Out",            0, false, false, "crt_tv")        \
    V(ctx, AUDIO,      AUDIO_MONO,       "Audio Out",            0, false, false, nullptr)

CERMU_PORT_MANIFEST(Einstein, EINSTEIN_FOR_EACH_PORT)

void TatungEinsteinSystem::setup_ports() {
    create_ports_from_manifest(kEinsteinPorts);
}

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(einstein_descriptor, [] {
    return std::make_unique<TatungEinsteinSystem>();
});
