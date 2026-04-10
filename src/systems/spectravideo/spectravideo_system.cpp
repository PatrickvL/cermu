/*
 * spectravideo_system.cpp — Spectravideo SVI-318 / SVI-328 system implementation
 *
 * Tick loop:
 *   Each call to tick() advances the Z80 by one T-state.
 *   The VDP is clocked once per CPU T-state (approximate).
 *   The AY-3-8910 is clocked at CPU_FREQ / 16 internally.
 *
 * I/O map:
 *   $80:  VDP data read/write
 *   $81:  VDP status read / control write
 *   $88:  PSG address latch (write)
 *   $8C:  PSG data write
 *   $90:  PSG data read
 *   $96:  PPI Port A — keyboard row select
 *   $97:  PPI Port B — keyboard column data
 *   $98:  PPI Port C — cassette / printer / ROM banking
 *   $99:  PPI control word
 */

#include "core/cermu.hpp"
#include "systems/spectravideo/spectravideo_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/formats/format_load_helpers.hpp"
#include "core/vfs/vfs.hpp"
#include "utils/keyboard_matrix.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<SVIVariant V>
static HardwareTraits create_svi_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = svi_constants::DISPLAY_WIDTH;
    traits.display.native_height   = svi_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = svi_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = svi_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;
    traits.display.has_overscan    = false;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = svi_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "AY-3-8910";

    traits.timing.cpu_frequency_hz   = svi_constants::CPU_FREQ_HZ;
    traits.timing.video_frequency_hz = svi_constants::CPU_FREQ_HZ;
    traits.timing.audio_sample_rate_hz = svi_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps         = 60;
    traits.timing.cycles_per_frame   = svi_constants::TSTATES_PER_FRAME_NTSC;
    traits.timing.standard           = VideoStandard::NTSC;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor svi318_descriptor = {
    "SVI-318", "SVI318",
    "Spectravideo SVI-318 — Z80A, TMS9918A, AY-3-8910, 16KB RAM (1983)",
    "spectravideo", {"SVI318", "SVI-318", "Spectravideo318"},
    nullptr,
    create_svi_hardware_traits<SVIVariant::SVI318>(),
    nullptr,
    "Spectravideo", 1983, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

static SystemDescriptor svi328_descriptor = {
    "SVI-328", "SVI328",
    "Spectravideo SVI-328 — Z80A, TMS9918A, AY-3-8910, 64KB RAM (1983)",
    "spectravideo", {"SVI328", "SVI-328", "Spectravideo328"},
    nullptr,
    create_svi_hardware_traits<SVIVariant::SVI328>(),
    nullptr,
    "Spectravideo", 1983, z80::ZilogZ80ATraits.display_name, SystemType::Home
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<SVIVariant V>
SpectravideoSystem<V>::SpectravideoSystem()
    : System()
    , pins_(SVI_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_svi_hardware_traits<V>();
}

template<SVIVariant V>
SpectravideoSystem<V>::~SpectravideoSystem() = default;

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<SVIVariant V>
const SystemDescriptor& SpectravideoSystem<V>::get_descriptor() const {
    if constexpr (V == SVIVariant::SVI318) return svi318_descriptor;
    else return svi328_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<SVIVariant V>
bool SpectravideoSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<SVIVariant V>
bool SpectravideoSystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<SVIVariant V>
bool SpectravideoSystem<V>::initialize() {
    using Traits = SVIVariantTraits<V>;
    log_info("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // Bind value-typed Chips members, then factory-create remaining (RAM/ROM)
    bind_all(board_, board_.components_, BT::kManifest);

    // Set port manifest for default peripheral attachment.
    port_manifest_       = BT::kManifest.port_slots;
    port_manifest_count_ = BT::kManifest.port_count;
    board_.apply(bus_);

    // Configure memory map
    configure_bus_memory_map();

    // Init chips
    pins_ = board_.z80.init();
    board_.psg.init();
    board_.ppi.init();

    // PSG clock & audio
    board_.psg.set_clock_frequency(svi_constants::CPU_FREQ_HZ / 16);
    board_.psg.set_audio_sample_rate(audio_sample_rate_);
    audio_sample_period_ = svi_constants::CPU_FREQ_HZ / audio_sample_rate_;

    // Keyboard init — all keys released (active-low)
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    // PPI Port B read callback — returns keyboard column data
    board_.ppi.set_port_b_read_callback(
        [](void* ctx, uint8_t /*port_a*/) -> uint8_t {
            auto* sys = static_cast<SpectravideoSystem*>(ctx);
            uint8_t row = sys->board_.ppi.get_port_a_output() & 0x0F;
            if (row < svi_constants::KEYBOARD_ROWS)
                return sys->keyboard_matrix_[row];
            return 0xFF;
        }, this);

    // Load ROMs
    if (!load_roms()) {
        log_info("%s: Warning — ROMs not loaded, system may not function\n", Traits::name);
    }

    // Register chips for Hardware menu
    register_bus_chips(board_);

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.vdp.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    // Audio port
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(svi_constants::DEFAULT_SAMPLE_RATE,
                           svi_constants::DEFAULT_SAMPLE_RATE);

    log_info("%s: System initialized (RAM: %dKB)\n", Traits::name,
           Traits::ram_size / 1024);
    system_ready_ = true;
    return true;
}

template<SVIVariant V>
void SpectravideoSystem<V>::shutdown() {
    system_ready_ = false;
}

template<SVIVariant V>
void SpectravideoSystem<V>::reset() {
    board_.reset_chips();
    pins_ = board_.z80.reset(pins_);
    frame_tstate_counter_ = 0;
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
    board_.ppi.init();
    board_.vdp.reset();
    board_.psg.init();
}

// ============================================================================
// EXECUTION
// ============================================================================

template<SVIVariant V>
void SpectravideoSystem<V>::tick() {
    // VDP tick
    bus_state_t vdp_bus = 0;
    vdp_bus = board_.vdp.tick(vdp_bus);

    // Forward VDP interrupt to Z80
    if (BUS_GET_BIT(vdp_bus, BUS_IRQ_BIT) == 0) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    } else {
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);
    }

    // CPU tick
    pins_ = board_.z80.tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // PSG tick — AY runs at CPU/16
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

template<SVIVariant V>
void SpectravideoSystem<V>::run_frame() {
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

template<SVIVariant V>
void SpectravideoSystem<V>::configure_bus_memory_map() {
    // Default layout: ROM at $0000-$7FFF, RAM at $8000+
    board_.apply(bus_);
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

template<SVIVariant V>
bus_state_t SpectravideoSystem<V>::io_tick(bus_state_t pins) {
    uint8_t port = BUS_GET_ADDR(pins) & 0xFF;
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    // VDP ports $80-$81
    if (port == svi_constants::VDP_DATA_PORT || port == svi_constants::VDP_CTRL_PORT) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, port - svi_constants::VDP_DATA_PORT);  // 0=data, 1=ctrl
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (port == svi_constants::VDP_DATA_PORT) {
            if (is_read) {
                vdp_bus = TMS9918A::port_read(&board_.vdp, vdp_bus);
                BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
            } else {
                TMS9918A::port_write(&board_.vdp, vdp_bus);
            }
        } else {
            if (is_read) {
                vdp_bus = TMS9918A::port_read(&board_.vdp, vdp_bus);
                BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
            } else {
                TMS9918A::port_write(&board_.vdp, vdp_bus);
            }
        }
        return pins;
    }

    // PSG ports $88 (addr), $8C (write), $90 (read)
    if (port == svi_constants::PSG_ADDR_PORT && !is_read) {
        board_.psg.latch_address(BUS_GET_DATA(pins));
        return pins;
    }
    if (port == svi_constants::PSG_DATA_WRITE_PORT && !is_read) {
        board_.psg.write_register(BUS_GET_DATA(pins));
        return pins;
    }
    if (port == svi_constants::PSG_DATA_READ_PORT && is_read) {
        BUS_SET_DATA(pins, board_.psg.read_register());
        return pins;
    }

    // PPI ports $96-$99
    if (port >= svi_constants::PPI_PORT_A && port <= svi_constants::PPI_CONTROL) {
        if (is_read) {
            BUS_SET_DATA(pins, board_.ppi.read(port - svi_constants::PPI_PORT_A));
        } else {
            board_.ppi.write(port - svi_constants::PPI_PORT_A, BUS_GET_DATA(pins));
        }
        return pins;
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<SVIVariant V>
bool SpectravideoSystem<V>::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;

    // SVI cartridges are raw ROM images that replace BASIC ROM at $0000-$7FFF.
    // On real hardware, inserting a cartridge physically replaces the
    // BASIC ROM on the bus at $0000-$7FFF.
    uint8_t* rom = board_.bios.data();
    if (!rom) return false;

    if (!load_raw_rom_mirrored(filepath, rom, 0x8000, 0x8000,
                               SVIVariantTraits<V>::name, program_title_))
        return false;

    reset();
    return true;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

template<SVIVariant V>
uint32_t SpectravideoSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return static_cast<uint32_t>(
        audio_ring_buf_.read(buffer, static_cast<size_t>(max_samples)));
}

template<SVIVariant V>
void SpectravideoSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    audio_sample_period_ = svi_constants::CPU_FREQ_HZ / audio_sample_rate_;
    board_.psg.set_audio_sample_rate(sample_rate_hz);
}

// ============================================================================
// INPUT
// ============================================================================

template<SVIVariant V>
void SpectravideoSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // SVI keyboard matrix: 11 rows × 8 columns, active-low
    // Row selected by PPI Port A bits 0-3
    static constexpr KeyMatrixMapping mappings[] = {
        // Row 0: 0-7
        { SDLK_0, 0, 0 }, { SDLK_1, 0, 1 }, { SDLK_2, 0, 2 }, { SDLK_3, 0, 3 },
        { SDLK_4, 0, 4 }, { SDLK_5, 0, 5 }, { SDLK_6, 0, 6 }, { SDLK_7, 0, 7 },
        // Row 1: 8-9, -, =, etc
        { SDLK_8, 1, 0 }, { SDLK_9, 1, 1 }, { SDLK_MINUS, 1, 2 }, { SDLK_EQUALS, 1, 3 },
        { SDLK_BACKSLASH, 1, 4 }, { SDLK_LEFTBRACKET, 1, 5 }, { SDLK_RIGHTBRACKET, 1, 6 },
        { SDLK_SEMICOLON, 1, 7 },
        // Row 2: A-H
        { SDLK_a, 2, 0 }, { SDLK_b, 2, 1 }, { SDLK_c, 2, 2 }, { SDLK_d, 2, 3 },
        { SDLK_e, 2, 4 }, { SDLK_f, 2, 5 }, { SDLK_g, 2, 6 }, { SDLK_h, 2, 7 },
        // Row 3: I-P
        { SDLK_i, 3, 0 }, { SDLK_j, 3, 1 }, { SDLK_k, 3, 2 }, { SDLK_l, 3, 3 },
        { SDLK_m, 3, 4 }, { SDLK_n, 3, 5 }, { SDLK_o, 3, 6 }, { SDLK_p, 3, 7 },
        // Row 4: Q-X
        { SDLK_q, 4, 0 }, { SDLK_r, 4, 1 }, { SDLK_s, 4, 2 }, { SDLK_t, 4, 3 },
        { SDLK_u, 4, 4 }, { SDLK_v, 4, 5 }, { SDLK_w, 4, 6 }, { SDLK_x, 4, 7 },
        // Row 5: Y, Z, etc
        { SDLK_y, 5, 0 }, { SDLK_z, 5, 1 },
        // Row 6: SHIFT, CTRL, GRAPH, CAPS
        { SDLK_LSHIFT, 6, 0 }, { SDLK_RSHIFT, 6, 0 },
        { SDLK_LCTRL, 6, 1 }, { SDLK_RCTRL, 6, 1 },
        { SDLK_LALT, 6, 2 },   // GRAPH
        { SDLK_CAPSLOCK, 6, 3 },
        { SDLK_F1, 6, 5 }, { SDLK_F2, 6, 6 }, { SDLK_F3, 6, 7 },
        // Row 7: F4-F5, ESC, TAB, STOP, BS, SELECT, ENTER
        { SDLK_F4, 7, 0 }, { SDLK_F5, 7, 1 },
        { SDLK_ESCAPE, 7, 2 }, { SDLK_TAB, 7, 3 },
        { SDLK_END, 7, 4 },    // STOP
        { SDLK_BACKSPACE, 7, 5 },
        { SDLK_HOME, 7, 6 },   // SELECT
        { SDLK_RETURN, 7, 7 },
        // Row 8: SPACE, arrows
        { SDLK_SPACE, 8, 0 },
        { SDLK_UP, 8, 5 }, { SDLK_DOWN, 8, 6 },
        { SDLK_LEFT, 8, 7 },
        // Row 9
        { SDLK_RIGHT, 9, 0 },
        { SDLK_DELETE, 9, 1 }, { SDLK_INSERT, 9, 2 },
        // Row 10: period, comma, slash, quote, backquote
        { SDLK_PERIOD, 10, 0 }, { SDLK_COMMA, 10, 1 },
        { SDLK_SLASH, 10, 2 }, { SDLK_QUOTE, 10, 3 },
        { SDLK_BACKQUOTE, 10, 4 },
    };

    keyboard_matrix_apply(mappings, keyboard_matrix_, key, pressed);
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<SVIVariant V>
bool SpectravideoSystem<V>::load_roms() {
    using Traits = SVIVariantTraits<V>;
    char rom_root[1024];
    if (!system_config_discover_rom_root(Traits::data_folder, rom_root, sizeof(rom_root))) {
        log_info("%s: Could not find ROM root folder\n", Traits::name);
        return false;
    }
    return board_.load_roms(rom_root, Traits::name);
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

template class SpectravideoSystem<SVIVariant::SVI318>;
template class SpectravideoSystem<SVIVariant::SVI328>;

REGISTER_SYSTEM(svi318_descriptor, [] {
    return std::make_unique<SpectravideoSystem<SVIVariant::SVI318>>();
});

REGISTER_SYSTEM(svi328_descriptor, [] {
    return std::make_unique<SpectravideoSystem<SVIVariant::SVI328>>();
});
