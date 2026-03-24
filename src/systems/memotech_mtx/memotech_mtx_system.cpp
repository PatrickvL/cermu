/*
 * memotech_mtx_system.cpp — Memotech MTX500/MTX512 system implementation
 *
 * Tick loop:
 *   Z80 one T-state per tick(), VDP dot clock, AY at CPU/16, CTC at CPU rate.
 *
 * I/O map:
 *   $00:     Keyboard sense (read)
 *   $01:     VDP data
 *   $02:     VDP control/status
 *   $03:     PSG data read/write
 *   $05:     Keyboard drive (row select)
 *   $06:     PSG address latch
 *   $08-$0B: CTC channels 0-3
 */

#include "systems/memotech_mtx/memotech_mtx_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<MTXVariant V>
static HardwareTraits create_mtx_hardware_traits() {
    HardwareTraits traits = {};

    traits.display.native_width    = mtx_constants::DISPLAY_WIDTH;
    traits.display.native_height   = mtx_constants::DISPLAY_HEIGHT;
    traits.display.visible_width   = mtx_constants::DISPLAY_WIDTH;
    traits.display.visible_height  = mtx_constants::DISPLAY_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 16;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = mtx_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "AY-3-8910";

    traits.timing.cpu_frequency_hz   = mtx_constants::CPU_FREQ_HZ;
    traits.timing.target_fps         = 50;
    traits.timing.cycles_per_frame   = mtx_constants::TSTATES_PER_FRAME;
    traits.timing.standard           = VideoStandard::PAL;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor mtx500_descriptor = {
    "Memotech MTX500", "MTX500",
    "Memotech MTX500 — Z80A @ 4MHz, TMS9918A, AY-3-8910, 32KB RAM (1983)",
    "memotech_mtx", {"MTX500", "MTX 500"},
    nullptr,
    create_mtx_hardware_traits<MTXVariant::MTX500>(),
    nullptr
};

static SystemDescriptor mtx512_descriptor = {
    "Memotech MTX512", "MTX512",
    "Memotech MTX512 — Z80A @ 4MHz, TMS9918A, AY-3-8910, 64KB RAM (1983)",
    "memotech_mtx", {"MTX512", "MTX 512"},
    nullptr,
    create_mtx_hardware_traits<MTXVariant::MTX512>(),
    nullptr
};

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<MTXVariant V>
MemotechMTXSystem<V>::MemotechMTXSystem()
    : System()
    , pins_(MTX_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_mtx_hardware_traits<V>();
}

template<MTXVariant V>
MemotechMTXSystem<V>::~MemotechMTXSystem() = default;

template<MTXVariant V>
const SystemDescriptor& MemotechMTXSystem<V>::get_descriptor() const {
    if constexpr (V == MTXVariant::MTX500) return mtx500_descriptor;
    else return mtx512_descriptor;
}

template<MTXVariant V>
bool MemotechMTXSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<MTXVariant V>
bool MemotechMTXSystem<V>::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

template<MTXVariant V>
bool MemotechMTXSystem<V>::initialize() {
    printf("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    
    
    
    board_.create_chips(&pins_);
    board_.apply(bus_);

    configure_bus_memory_map();

    
    pins_ = board_.cpu().init();
    board_.sound().init();
    board_.io().init();

    board_.sound().set_clock_frequency(mtx_constants::CPU_FREQ_HZ / 16);
    board_.sound().set_audio_sample_rate(audio_sample_rate_);
    audio_sample_period_ = mtx_constants::CPU_FREQ_HZ / audio_sample_rate_;

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    if (!load_roms()) {
        printf("%s: Warning — ROMs not loaded\n", Traits::name);
    }

    register_bus_chips(board_);

    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.video().set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(mtx_constants::DEFAULT_SAMPLE_RATE,
                           mtx_constants::DEFAULT_SAMPLE_RATE);

    system_ready_ = true;
    printf("%s: System initialized (RAM: %dKB)\n", Traits::name,
           Traits::ram_size / 1024);
    return true;
}

template<MTXVariant V>
void MemotechMTXSystem<V>::shutdown() {
    
    system_ready_ = false;
}

template<MTXVariant V>
void MemotechMTXSystem<V>::reset() {
    
    board_.reset_chips();
    pins_ = board_.cpu().reset(pins_);
    frame_tstate_counter_ = 0;
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
}

// ============================================================================
// EXECUTION
// ============================================================================

template<MTXVariant V>
void MemotechMTXSystem<V>::tick() {
    

    // VDP tick
    bus_state_t vdp_bus = 0;
    vdp_bus = board_.video().tick(vdp_bus);

    if (BUS_GET_BIT(vdp_bus, BUS_IRQ_BIT) == 0)
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    else
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);

    // CTC tick (system clock rate)
    board_.io().tick();
    if (board_.io().interrupt_pending()) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    }

    // CPU tick
    pins_ = board_.cpu().tick(pins_);

    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // PSG tick (CPU/16)
    if ((frame_tstate_counter_ & 0x0F) == 0) {
        board_.sound().tick();
    }

    // Audio sample generation
    audio_sample_counter_++;
    if (audio_sample_counter_ >= audio_sample_period_) {
        audio_sample_counter_ = 0;
        float sample = board_.sound().get_sample();
        audio_ring_buf_.write(&sample, 1);
        if (audio_port_) audio_port_->drive_sample(sample);
    }

    frame_tstate_counter_++;
    total_cycles_++;
}

template<MTXVariant V>
void MemotechMTXSystem<V>::run_frame() {
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

template<MTXVariant V>
void MemotechMTXSystem<V>::configure_bus_memory_map() {
    board_.apply(bus_);
}

// ============================================================================
// I/O DISPATCH
// ============================================================================

template<MTXVariant V>
bus_state_t MemotechMTXSystem<V>::io_tick(bus_state_t pins) {
    uint8_t port = BUS_GET_ADDR(pins) & 0xFF;
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    // Keyboard sense ($00)
    if (is_read && port == mtx_constants::KBD_SENSE_PORT) {
        uint8_t data = 0xFF;
        for (int row = 0; row < mtx_constants::KEYBOARD_ROWS; ++row) {
            if (!(keyboard_drive_ & (1 << row)))
                data &= keyboard_matrix_[row];
        }
        BUS_SET_DATA(pins, data);
        return pins;
    }

    // Keyboard drive ($05)
    if (!is_read && port == mtx_constants::KBD_DRIVE_PORT) {
        keyboard_drive_ = BUS_GET_DATA(pins);
        return pins;
    }

    // VDP data ($01)
    if (port == mtx_constants::VDP_DATA_PORT) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, 0);  // port 0 = data
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = TMS9918A::port_read(&board_.video(), vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            TMS9918A::port_write(&board_.video(), vdp_bus);
        }
        return pins;
    }

    // VDP control ($02)
    if (port == mtx_constants::VDP_CTRL_PORT) {
        bus_state_t vdp_bus = 0;
        BUS_SET_ADDR(vdp_bus, 1);  // port 1 = control/status
        BUS_SET_DATA(vdp_bus, BUS_GET_DATA(pins));
        if (is_read) {
            vdp_bus = TMS9918A::port_read(&board_.video(), vdp_bus);
            BUS_SET_DATA(pins, BUS_GET_DATA(vdp_bus));
        } else {
            TMS9918A::port_write(&board_.video(), vdp_bus);
        }
        return pins;
    }

    // PSG data ($03)
    if (port == mtx_constants::PSG_DATA_PORT) {
        if (is_read)
            BUS_SET_DATA(pins, board_.sound().read_register());
        else
            board_.sound().write_register(BUS_GET_DATA(pins));
        return pins;
    }

    // PSG address latch ($06)
    if (!is_read && port == mtx_constants::PSG_ADDR_PORT) {
        board_.sound().latch_address(BUS_GET_DATA(pins));
        return pins;
    }

    // CTC channels ($08-$0B)
    if (port >= mtx_constants::CTC_BASE_PORT && port <= (mtx_constants::CTC_BASE_PORT + 3)) {
        int channel = port - mtx_constants::CTC_BASE_PORT;
        if (is_read)
            BUS_SET_DATA(pins, board_.io().read(channel));
        else
            board_.io().write(channel, BUS_GET_DATA(pins));
        return pins;
    }

    return pins;
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<MTXVariant V>
bool MemotechMTXSystem<V>::load_file(const char* filepath) {
    if (!filepath) return false;
    printf("%s: File loading not yet implemented: %s\n", Traits::name, filepath);
    return false;
}

// ============================================================================
// DISPLAY
// ============================================================================


// ============================================================================
// AUDIO
// ============================================================================

template<MTXVariant V>
uint32_t MemotechMTXSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    return static_cast<uint32_t>(
        audio_ring_buf_.read(buffer, static_cast<size_t>(max_samples)));
}

template<MTXVariant V>
void MemotechMTXSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
    audio_sample_period_ = mtx_constants::CPU_FREQ_HZ / audio_sample_rate_;
    board_.sound().set_audio_sample_rate(sample_rate_hz);
}

// ============================================================================
// INPUT
// ============================================================================

template<MTXVariant V>
void MemotechMTXSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // MTX keyboard matrix: 8 rows × 8 columns, active-low
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
        { SDLK_b, 3, 4 }, { SDLK_n, 3, 5 }, { SDLK_m, 3, 6 }, { SDLK_COMMA, 3, 7 },
        // Row 4
        { SDLK_9, 4, 0 }, { SDLK_0, 4, 1 }, { SDLK_MINUS, 4, 2 },
        { SDLK_o, 4, 3 }, { SDLK_p, 4, 4 }, { SDLK_l, 4, 5 },
        // Row 5
        { SDLK_RETURN, 5, 0 }, { SDLK_SPACE, 5, 1 }, { SDLK_BACKSPACE, 5, 2 },
        // Row 6
        { SDLK_LSHIFT, 6, 0 }, { SDLK_RSHIFT, 6, 0 }, { SDLK_LCTRL, 6, 1 },
        // Row 7
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

template<MTXVariant V>
bool MemotechMTXSystem<V>::load_roms() {
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

template class MemotechMTXSystem<MTXVariant::MTX500>;
template class MemotechMTXSystem<MTXVariant::MTX512>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(mtx500_descriptor, [] {
    return std::make_unique<MemotechMTXSystem<MTXVariant::MTX500>>();
});

REGISTER_SYSTEM(mtx512_descriptor, [] {
    return std::make_unique<MemotechMTXSystem<MTXVariant::MTX512>>();
});
