/*
 * msx_system.cpp — MSX1 / MSX2 / MSX2+ system implementation
 *
 * Tick loop:
 *   Each call to tick() advances the Z80 by one T-state.
 *   The VDP is clocked per dot (342 dots/line).
 *   The AY-3-8910 is clocked at CPU_FREQ / 16 internally.
 *
 * I/O map:
 *   $98:  VDP data read/write
 *   $99:  VDP status read / control write
 *   $9A:  V9938+ palette write
 *   $9B:  V9938+ indirect register access
 *   $A0:  PSG address latch (write)
 *   $A1:  PSG data write
 *   $A2:  PSG data read
 *   $A8:  PPI Port A — primary slot select
 *   $A9:  PPI Port B — keyboard column data
 *   $AA:  PPI Port C — keyboard row select, cassette, caps LED
 *   $AB:  PPI control word
 */

#include "systems/msx/msx_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<MSXVariant V>
static HardwareTraits create_msx_hardware_traits() {
    using Traits = MSXVariantTraits<V>;
    HardwareTraits traits = {};

    traits.display.native_width    = Traits::display_w;
    traits.display.native_height   = Traits::display_h;
    traits.display.visible_width   = Traits::display_w;
    traits.display.visible_height  = Traits::display_h;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;
    traits.display.has_overscan    = false;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = msx_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "AY-3-8910";

    traits.timing.cpu_frequency_hz   = msx_constants::CPU_FREQ_HZ;
    traits.timing.video_frequency_hz = msx_constants::CPU_FREQ_HZ;
    traits.timing.audio_sample_rate_hz = msx_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps         = 60;
    traits.timing.cycles_per_frame   = msx_constants::TSTATES_PER_FRAME_NTSC;
    traits.timing.standard           = VideoStandard::NTSC;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor msx1_descriptor = {
    "MSX1", "MSX1",
    "MSX1 — Z80A, TMS9918A, AY-3-8910, i8255 PPI (1983)",
    "msx", {"MSX", "MSX1"},
    nullptr,
    create_msx_hardware_traits<MSXVariant::MSX1>(),
    nullptr
};

static SystemDescriptor msx2_descriptor = {
    "MSX2", "MSX2",
    "MSX2 — Z80A, V9938, AY-3-8910, i8255 PPI (1985)",
    "msx", {"MSX2"},
    nullptr,
    create_msx_hardware_traits<MSXVariant::MSX2>(),
    nullptr
};

static SystemDescriptor msx2p_descriptor = {
    "MSX2+", "MSX2+",
    "MSX2+ — Z80A, V9958, AY-3-8910, i8255 PPI (1988)",
    "msx", {"MSX2+", "MSX2Plus"},
    nullptr,
    create_msx_hardware_traits<MSXVariant::MSX2P>(),
    nullptr
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<MSXVariant V>
MSXSystem<V>::MSXSystem()
    : System()
    , pins_(MSX_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_msx_hardware_traits<V>();
}

template<MSXVariant V>
MSXSystem<V>::~MSXSystem() = default;

// ============================================================================
// SYSTEM IDENTIFICATION
// ============================================================================

template<MSXVariant V>
const SystemDescriptor& MSXSystem<V>::get_descriptor() const {
    if constexpr (V == MSXVariant::MSX1) return msx1_descriptor;
    else if constexpr (V == MSXVariant::MSX2) return msx2_descriptor;
    else return msx2p_descriptor;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

template<MSXVariant V>
bool MSXSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<MSXVariant V>
bool MSXSystem<V>::apply_configuration() {
    return true;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

template<MSXVariant V>
bool MSXSystem<V>::initialize() {
    printf("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // Pre-bind stack-member chips, then factory-create remaining
    board_.bind_chip(board_.template find_index<typename Traits::VDP>(), &vdp_);
    board_.bind_chip(board_.template find_index<AY_3_8910>(), &psg_);
    board_.bind_chip(board_.template find_index<i8255_t>(), &ppi_);
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // Configure memory map
    configure_bus_memory_map();

    // Init chips
    cpu_ = board_.template cpu<ZilogZ80A>();
    pins_ = board_.cpu_chip()->init();
    psg_.init();
    ppi_.init();

    // AY clock: PSG runs at CPU_FREQ / 16 internally, but we tick it
    // at CPU rate and let the chip handle internal division
    psg_.set_clock_frequency(msx_constants::CPU_FREQ_HZ / 16);
    psg_.set_audio_sample_rate(audio_sample_rate_);

    // Audio setup
    audio_sample_period_ = msx_constants::CPU_FREQ_HZ / audio_sample_rate_;

    // Keyboard init — all keys released (active-low)
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    // PPI Port B read callback — returns keyboard column data
    ppi_.set_port_b_read_callback(
        [](void* ctx, uint8_t /*port_a*/) -> uint8_t {
            auto* sys = static_cast<MSXSystem*>(ctx);
            uint8_t row = sys->ppi_.get_port_c_output() & 0x0F;
            if (row < msx_constants::KEYBOARD_ROWS)
                return sys->keyboard_matrix_[row];
            return 0xFF;
        }, this);

    // Load ROMs
    if (!load_roms()) {
        printf("%s: Warning — ROMs not loaded, system may not function\n", Traits::name);
    }

    // Register chips for Hardware menu
    register_bus_chips(board_);

    // Display setup
    display_.init(Traits::display_w, Traits::display_h);
    display_.set_palette(vdp_.system_palette(), vdp_.palette_size());
    vdp_.set_display(&display_);
    register_display(&display_);

    // Video stream output
    video_port_ = std::make_unique<CompositeVideoPort>();
    vdp_.set_stream(&video_port_->stream());
    video_port_->bind_frame_output(&last_frame_data_);

    // Audio port
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(msx_constants::DEFAULT_SAMPLE_RATE,
                           msx_constants::DEFAULT_SAMPLE_RATE);

    printf("%s: System initialized (RAM: %dKB)\n", Traits::name,
           Traits::ram_size / 1024);
    system_ready_ = true;
    return true;
}

template<MSXVariant V>
void MSXSystem<V>::shutdown() {
    cpu_ = nullptr;
    system_ready_ = false;
}

template<MSXVariant V>
void MSXSystem<V>::reset() {
    if (!cpu_) return;
    board_.reset_chips();
    pins_ = board_.cpu_chip()->reset(pins_);
    slot_select_ = 0;
    frame_tstate_counter_ = 0;
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
    ppi_.init();
    vdp_.reset();
    psg_.init();
}

// ============================================================================
// EXECUTION
// ============================================================================

template<MSXVariant V>
void MSXSystem<V>::tick() {
    if (!cpu_) return;

    // VDP tick — dot clock is ~3× CPU clock, but for simplicity
    // we tick the VDP once per CPU T-state (approximate)
    bus_state_t vdp_bus = 0;
    vdp_bus = vdp_.tick(vdp_bus);

    // Check VDP interrupt
    if (BUS_GET_BIT(vdp_bus, BUS_IRQ_BIT) == 0) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);  // Assert INT (active-low)
    } else {
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);
    }

    // CPU tick
    pins_ = cpu_->tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // PSG tick — AY runs at CPU/16, but we tick at CPU rate
    // and let generate_sample handle downsampling
    if ((frame_tstate_counter_ & 0x0F) == 0) {
        psg_.tick();
    }

    // Audio sample generation
    audio_sample_counter_++;
    if (audio_sample_counter_ >= audio_sample_period_) {
        audio_sample_counter_ = 0;
        float sample = psg_.get_sample();
        audio_ring_buf_.write(&sample, 1);
        if (audio_port_) audio_port_->drive_sample(sample);
    }

    frame_tstate_counter_++;
    total_cycles_++;
}

template<MSXVariant V>
void MSXSystem<V>::run_frame() {
    if (!video_port_) return;
    auto& stream = video_port_->stream();
    while (!stream.frame_ended()) {
        tick();
    }
    video_port_->swap_frame();
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<MSXVariant V>
void MSXSystem<V>::configure_bus_memory_map() {
    // Default MSX slot layout:
    //   Pages 0-1 ($0000-$7FFF): ROM (slot 0)
    //   Pages 2-3 ($8000-$FFFF): RAM (slot 3)
    board_.apply(bus_);
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

template<MSXVariant V>
bus_state_t MSXSystem<V>::io_tick(bus_state_t pins) {
    uint8_t port = BUS_GET_ADDR(pins) & 0xFF;
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    // VDP ports $98-$9B
    if (port >= msx_constants::VDP_DATA_PORT && port <= msx_constants::VDP_INDIRECT_PORT) {
        // Set up a bus word with port offset in address for VDP static dispatch
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, port - msx_constants::VDP_DATA_PORT);
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = decltype(vdp_)::port_read(&vdp_, vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            decltype(vdp_)::port_write(&vdp_, vdp_bus);
        }
        return pins;
    }

    // PSG ports $A0-$A2
    if (port == msx_constants::PSG_ADDR_PORT && !is_read) {
        psg_.latch_address(BUS_GET_DATA(pins));
        return pins;
    }
    if (port == msx_constants::PSG_DATA_WRITE_PORT && !is_read) {
        psg_.write_register(BUS_GET_DATA(pins));
        return pins;
    }
    if (port == msx_constants::PSG_DATA_READ_PORT && is_read) {
        BUS_SET_DATA(pins, psg_.read_register());
        return pins;
    }

    // PPI ports $A8-$AB
    if (port >= msx_constants::PPI_PORT_A && port <= msx_constants::PPI_CONTROL) {
        if (is_read) {
            BUS_SET_DATA(pins, ppi_.read(port - msx_constants::PPI_PORT_A));
        } else {
            ppi_.write(port - msx_constants::PPI_PORT_A, BUS_GET_DATA(pins));
            // Slot selection changed — update memory map
            if (port == msx_constants::PPI_PORT_A) {
                slot_select_ = ppi_.get_port_a_output();
                // TODO: remap memory pages based on slot_select_
            }
        }
        return pins;
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<MSXVariant V>
bool MSXSystem<V>::load_file(const char* filepath) {
    if (!filepath || !cpu_) return false;
    // TODO: support ROM cartridge (.rom) and disk image formats
    printf("%s: File loading not yet implemented: %s\n", Traits::name, filepath);
    return false;
}

// ============================================================================
// DISPLAY
// ============================================================================

template<MSXVariant V>
void MSXSystem<V>::get_display_dimensions(int* width, int* height) const {
    *width  = Traits::display_w;
    *height = Traits::display_h;
}

// ============================================================================
// AUDIO
// ============================================================================

template<MSXVariant V>
uint32_t MSXSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return static_cast<uint32_t>(
        audio_ring_buf_.read(buffer, static_cast<size_t>(max_samples)));
}

template<MSXVariant V>
void MSXSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    audio_sample_period_ = msx_constants::CPU_FREQ_HZ / audio_sample_rate_;
    psg_.set_audio_sample_rate(sample_rate_hz);
}

// ============================================================================
// INPUT
// ============================================================================

template<MSXVariant V>
void MSXSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // MSX keyboard matrix: 11 rows × 8 columns, active-low
    // Row selected by PPI Port C bits 0-3
    struct KeyMapping { SDL_Keycode sdl_key; int row; int bit; };
    static constexpr KeyMapping mappings[] = {
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
        // Row 6: SHIFT, CTRL, GRAPH, CAPS, CODE, F1-F3
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
        // Row 8: SPACE, cursor keys, etc
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
// GUI
// ============================================================================

template<MSXVariant V>
void MSXSystem<V>::render_system_menu_items() {}

template<MSXVariant V>
void MSXSystem<V>::render_configuration_ui() {}

template<MSXVariant V>
void MSXSystem<V>::set_speed_multiplier(float multiplier) {
    speed_multiplier_ = multiplier;
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<MSXVariant V>
bool MSXSystem<V>::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root(Traits::data_folder, rom_root, sizeof(rom_root))) {
        printf("%s: Could not find ROM root folder\n", Traits::name);
        return false;
    }
    return board_.load_roms(rom_root, Traits::name);
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================

template class MSXSystem<MSXVariant::MSX1>;
template class MSXSystem<MSXVariant::MSX2>;
template class MSXSystem<MSXVariant::MSX2P>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(msx1_descriptor, [] {
    return std::make_unique<MSXSystem<MSXVariant::MSX1>>();
});

REGISTER_SYSTEM(msx2_descriptor, [] {
    return std::make_unique<MSXSystem<MSXVariant::MSX2>>();
});

REGISTER_SYSTEM(msx2p_descriptor, [] {
    return std::make_unique<MSXSystem<MSXVariant::MSX2P>>();
});
