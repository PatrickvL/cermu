/*
 * mc6809_vdg_system.cpp — Unified MC6809E + MC6847 VDG system implementation
 *
 * Single implementation for all hardware-identical variants:
 *   CoCo 1, CoCo 2, Dragon 32, Dragon 64
 *
 * Tick loop (identical across all variants):
 *   MC6847 VDG dot clock → FS drives PIA0 CA1 (VSYNC IRQ)
 *   MC6809E one bus cycle per tick()
 *   PIA 1 Port B bits → VDG mode pins (AG, GM0–GM2, CSS)
 *   SAM for address decode / DRAM refresh
 *
 * I/O map (memory-mapped only — no I/O ports):
 *   $FF00–$FF03 : PIA 0 (keyboard, joystick comparator, HSYNC)
 *   $FF20–$FF23 : PIA 1 (VDG mode, DAC, cassette, RS-232)
 *   $FFC0–$FFDF : SAM (VDG mode, display offset, memory config)
 */

#include "core/cermu.hpp"
#include "systems/mc6809_vdg/mc6809_vdg_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "core/formats/format_load_helpers.hpp"
#include "core/vfs/vfs.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

template<MC6809VDGVariant V>
MC6809VDGSystem<V>::MC6809VDGSystem()
    : System()
    , pins_(MC6809_VDG_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_mc6809_vdg_hardware_traits<V>();
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
}

template<MC6809VDGVariant V>
MC6809VDGSystem<V>::~MC6809VDGSystem() = default;

template<MC6809VDGVariant V>
const SystemDescriptor& MC6809VDGSystem<V>::get_descriptor() const {
    using VT = MC6809VDGVariantTraits<V>;
    static SystemDescriptor desc = {
        VT::log_prefix,
        VT::log_prefix,
        VT::log_prefix,
        VT::rom_folder,
        {},
        nullptr,
        create_mc6809_vdg_hardware_traits<V>(),
        nullptr,
        "", 0, "MC6809E", SystemType::Home
    };
    return desc;
}

template<MC6809VDGVariant V>
bool MC6809VDGSystem<V>::set_configuration(const SystemConfiguration& config) {
    config_ = config;
    return true;
}

template<MC6809VDGVariant V>
bool MC6809VDGSystem<V>::apply_configuration() { return true; }

// ============================================================================
// LIFECYCLE
// ============================================================================

template<MC6809VDGVariant V>
bool MC6809VDGSystem<V>::initialize() {
    log_info("%s: Initializing system\n", Traits::log_prefix);

    register_board(&board_);
    bind_all(board_, board_.components_, kMC6809VDGManifest<V>);

    port_manifest_       = kMC6809VDGManifest<V>.port_slots;
    port_manifest_count_ = kMC6809VDGManifest<V>.port_count;
    board_.apply(bus_);

    configure_bus_memory_map();
    wire_pia_callbacks();

    pins_ = board_.cpu.init();
    board_.vdg.init();
    board_.pia0.init();
    board_.pia1.init();
    board_.sam.reset();

    if (!load_roms()) {
        log_info("%s: Warning — ROM(s) not loaded\n", Traits::log_prefix);
    }

    register_bus_chips(board_);

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();
    board_.vdg.set_video_out(&video_port_->output());
    video_port_->bind_frame_output(&last_frame_data_);

    // Audio output
    audio_port_ = std::make_unique<AudioPort>();
    audio_port_->configure(mc6809_vdg::DEFAULT_SAMPLE_RATE,
                           mc6809_vdg::DEFAULT_SAMPLE_RATE);

    system_ready_ = true;
    log_info("%s: System initialized\n", Traits::log_prefix);
    return true;
}

template<MC6809VDGVariant V>
void MC6809VDGSystem<V>::shutdown() {
    system_ready_ = false;
}

template<MC6809VDGVariant V>
void MC6809VDGSystem<V>::reset() {
    board_.reset_chips();
    pins_ = board_.cpu.reset(pins_);
    frame_cycle_counter_ = 0;
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
}

// ============================================================================
// EXECUTION — identical tick loop across all variants
// ============================================================================

template<MC6809VDGVariant V>
void MC6809VDGSystem<V>::tick() {
    // VDG tick — renders display, generates FS/HS
    board_.vdg.tick();

    // Copy VDG IRQ (FS — frame sync) → PIA0 CA1 for VSYNC interrupt
    bool fs = board_.vdg.check_fs();
    if (fs) board_.pia0.set_ca1(true);

    // CPU tick
    pins_ = board_.cpu.tick(pins_);

    // Memory / I/O dispatch
    pins_ = bus_.tick(pins_);

    // PIA 1 Port B drives VDG mode pins:
    //   bit 7 = A/G, bits 6-4 = GM2-GM0, bit 3 = CSS
    // (Identical between CoCo and Dragon)
    uint8_t pia1b = board_.pia1.port_b_data;
    board_.vdg.set_ag((pia1b & 0x80) != 0);
    board_.vdg.set_gm((pia1b >> 4) & 0x07);
    board_.vdg.set_css((pia1b & 0x08) != 0);

    frame_cycle_counter_++;
    total_cycles_++;
}

template<MC6809VDGVariant V>
void MC6809VDGSystem<V>::run_frame() {
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

template<MC6809VDGVariant V>
void MC6809VDGSystem<V>::configure_bus_memory_map() {
    board_.apply(bus_);
}

// ============================================================================
// PIA CALLBACKS — keyboard matrix scanning (identical logic)
// ============================================================================

template<MC6809VDGVariant V>
void MC6809VDGSystem<V>::wire_pia_callbacks() {
    // PIA 0 — keyboard matrix scanning
    board_.pia0.user_data = this;
    board_.pia0.on_port_a_read = +[](void* ud) -> uint8_t {
        auto* sys = static_cast<MC6809VDGSystem<V>*>(ud);
        // Port A reads the keyboard column state based on Port B row select
        uint8_t result = 0xFF;
        uint8_t rows = ~sys->board_.pia0.port_b_data;  // Active-low to active-high
        for (int r = 0; r < 8; r++) {
            if (rows & (1 << r))
                result &= sys->keyboard_matrix_[r];
        }
        return result;
    };

    // PIA 1 — VDG mode / DAC / sound
    board_.pia1.user_data = this;
    board_.pia1.on_port_b_write = +[](void* ud, uint8_t data) {
        // Port B bits control VDG: AG, GM0-GM2, CSS
        // This is handled in tick() via individual VDG mode pin setters
        (void)ud; (void)data;
    };
}

// ============================================================================
// FILE LOADING
// ============================================================================

template<MC6809VDGVariant V>
bool MC6809VDGSystem<V>::load_file(const char* filepath) {
    if (!filepath || !system_ready_) return false;

    uint8_t* cart = board_.cart.data();
    if (!cart) return false;

    // Cartridges are raw ROM images, up to 16 KB.
    // Load into $C000–$FEFF range, mirrored if smaller.
    if (!load_raw_rom_mirrored(filepath, cart, 0x4000, 0xC000,
                               get_descriptor().name, program_title_))
        return false;

    reset();
    return true;
}

// ============================================================================
// AUDIO — 1-bit DAC (PIA 1 Port A bit 2) — silent until properly wired
// ============================================================================

template<MC6809VDGVariant V>
uint32_t MC6809VDGSystem<V>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) return audio_port_->read_samples(buffer, max_samples);
    std::memset(buffer, 0, max_samples * sizeof(float));
    return max_samples;
}

template<MC6809VDGVariant V>
void MC6809VDGSystem<V>::set_audio_sample_rate(int sample_rate_hz) {
    audio_sample_rate_ = static_cast<uint32_t>(sample_rate_hz);
}

// ============================================================================
// INPUT — keyboard matrix (variant-specific layout)
//
// Both CoCo and Dragon use an 8×7 active-low keyboard matrix scanned via
// PIA 0 (Port B selects row, Port A reads column).  The physical key→matrix
// mapping differs between the two keyboard designs.
// ============================================================================

template<MC6809VDGVariant V>
void MC6809VDGSystem<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    struct KeyMapping { SDL_Keycode sdl_key; int row; int col; };

    if constexpr (is_coco_variant<V>) {
        // CoCo keyboard: 51 keys
        static constexpr KeyMapping mappings[] = {
            // Row 0
            { SDLK_AT,       0, 0 }, { SDLK_a,        0, 1 }, { SDLK_b,        0, 2 },
            { SDLK_c,        0, 3 }, { SDLK_d,        0, 4 }, { SDLK_e,        0, 5 },
            { SDLK_f,        0, 6 },
            // Row 1
            { SDLK_g,        1, 0 }, { SDLK_h,        1, 1 }, { SDLK_i,        1, 2 },
            { SDLK_j,        1, 3 }, { SDLK_k,        1, 4 }, { SDLK_l,        1, 5 },
            { SDLK_m,        1, 6 },
            // Row 2
            { SDLK_n,        2, 0 }, { SDLK_o,        2, 1 }, { SDLK_p,        2, 2 },
            { SDLK_q,        2, 3 }, { SDLK_r,        2, 4 }, { SDLK_s,        2, 5 },
            { SDLK_t,        2, 6 },
            // Row 3
            { SDLK_u,        3, 0 }, { SDLK_v,        3, 1 }, { SDLK_w,        3, 2 },
            { SDLK_x,        3, 3 }, { SDLK_y,        3, 4 }, { SDLK_z,        3, 5 },
            // Row 4
            { SDLK_UP,       4, 3 }, { SDLK_DOWN,     4, 4 }, { SDLK_LEFT,     4, 5 },
            { SDLK_RIGHT,    4, 6 },
            // Row 5
            { SDLK_SPACE,    5, 0 }, { SDLK_0,        5, 1 }, { SDLK_1,        5, 2 },
            { SDLK_2,        5, 3 }, { SDLK_3,        5, 4 }, { SDLK_4,        5, 5 },
            { SDLK_5,        5, 6 },
            // Row 6
            { SDLK_6,        6, 0 }, { SDLK_7,        6, 1 }, { SDLK_8,        6, 2 },
            { SDLK_9,        6, 3 }, { SDLK_COLON,    6, 4 }, { SDLK_SEMICOLON,6, 5 },
            // Row 7
            { SDLK_RETURN,   7, 0 }, { SDLK_BACKSPACE,7, 1 },
            { SDLK_LSHIFT,   7, 3 }, { SDLK_RSHIFT,   7, 3 },
        };

        for (const auto& m : mappings) {
            if (m.sdl_key == key) {
                if (pressed)
                    keyboard_matrix_[m.row] &= ~(1 << m.col);
                else
                    keyboard_matrix_[m.row] |= (1 << m.col);
            }
        }
    } else {
        // Dragon keyboard: 53 keys (different scan code layout)
        static constexpr KeyMapping mappings[] = {
            // Row 0
            { SDLK_0,        0, 0 }, { SDLK_1,        0, 1 }, { SDLK_2,        0, 2 },
            { SDLK_3,        0, 3 }, { SDLK_4,        0, 4 }, { SDLK_5,        0, 5 },
            { SDLK_6,        0, 6 },
            // Row 1
            { SDLK_7,        1, 0 }, { SDLK_8,        1, 1 }, { SDLK_9,        1, 2 },
            { SDLK_COLON,    1, 3 }, { SDLK_SEMICOLON,1, 4 }, { SDLK_COMMA,    1, 5 },
            { SDLK_MINUS,    1, 6 },
            // Row 2
            { SDLK_PERIOD,   2, 0 }, { SDLK_SLASH,    2, 1 }, { SDLK_AT,       2, 2 },
            { SDLK_a,        2, 3 }, { SDLK_b,        2, 4 }, { SDLK_c,        2, 5 },
            { SDLK_d,        2, 6 },
            // Row 3
            { SDLK_e,        3, 0 }, { SDLK_f,        3, 1 }, { SDLK_g,        3, 2 },
            { SDLK_h,        3, 3 }, { SDLK_i,        3, 4 }, { SDLK_j,        3, 5 },
            { SDLK_k,        3, 6 },
            // Row 4
            { SDLK_l,        4, 0 }, { SDLK_m,        4, 1 }, { SDLK_n,        4, 2 },
            { SDLK_o,        4, 3 }, { SDLK_p,        4, 4 }, { SDLK_q,        4, 5 },
            { SDLK_r,        4, 6 },
            // Row 5
            { SDLK_s,        5, 0 }, { SDLK_t,        5, 1 }, { SDLK_u,        5, 2 },
            { SDLK_v,        5, 3 }, { SDLK_w,        5, 4 }, { SDLK_x,        5, 5 },
            { SDLK_y,        5, 6 },
            // Row 6
            { SDLK_z,        6, 0 }, { SDLK_UP,       6, 3 }, { SDLK_DOWN,     6, 4 },
            { SDLK_LEFT,     6, 5 }, { SDLK_RIGHT,    6, 6 },
            // Row 7
            { SDLK_SPACE,    7, 0 }, { SDLK_RETURN,   7, 1 }, { SDLK_ESCAPE,   7, 2 },
            { SDLK_LSHIFT,   7, 6 }, { SDLK_RSHIFT,   7, 6 },
        };

        for (const auto& m : mappings) {
            if (m.sdl_key == key) {
                if (pressed)
                    keyboard_matrix_[m.row] &= ~(1 << m.col);
                else
                    keyboard_matrix_[m.row] |= (1 << m.col);
            }
        }
    }
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<MC6809VDGVariant V>
bool MC6809VDGSystem<V>::load_roms() {
    char rom_root[1024];
    if (!system_config_discover_rom_root(Traits::rom_folder, rom_root, sizeof(rom_root))) {
        log_info("%s: Could not find ROM root folder\n", Traits::log_prefix);
        return false;
    }
    return board_.load_roms(rom_root, Traits::log_prefix);
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATION
// ============================================================================

template class MC6809VDGSystem<MC6809VDGVariant::COCO1>;
template class MC6809VDGSystem<MC6809VDGVariant::COCO2>;
template class MC6809VDGSystem<MC6809VDGVariant::DRAGON32>;
template class MC6809VDGSystem<MC6809VDGVariant::DRAGON64>;
