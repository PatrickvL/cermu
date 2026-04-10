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
#include "utils/keyboard_matrix.hpp"
#include "utils/guest_key_chars.hpp"
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
    if (fs) {
        board_.pia0.set_ca1(true);
        // Render frame at field sync — the VDG reads from RAM at the
        // SAM-controlled display offset and drives the video output.
        uint16_t offset = board_.sam.display_offset();
        board_.vdg.render_frame(board_.ram.data() + offset);
    }

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
    if constexpr (is_coco_variant<V>) {
        // CoCo keyboard: 51 keys
        static const KeyMatrixEntry entries[] = {
            // Row 0
            { 0, 0, '@', 0 }, { 0, 1, 'a', 0 }, { 0, 2, 'b', 0 },
            { 0, 3, 'c', 0 }, { 0, 4, 'd', 0 }, { 0, 5, 'e', 0 },
            { 0, 6, 'f', 0 },
            // Row 1
            { 1, 0, 'g', 0 }, { 1, 1, 'h', 0 }, { 1, 2, 'i', 0 },
            { 1, 3, 'j', 0 }, { 1, 4, 'k', 0 }, { 1, 5, 'l', 0 },
            { 1, 6, 'm', 0 },
            // Row 2
            { 2, 0, 'n', 0 }, { 2, 1, 'o', 0 }, { 2, 2, 'p', 0 },
            { 2, 3, 'q', 0 }, { 2, 4, 'r', 0 }, { 2, 5, 's', 0 },
            { 2, 6, 't', 0 },
            // Row 3
            { 3, 0, 'u', 0 }, { 3, 1, 'v', 0 }, { 3, 2, 'w', 0 },
            { 3, 3, 'x', 0 }, { 3, 4, 'y', 0 }, { 3, 5, 'z', 0 },
            // Row 4
            { 4, 3, UKEY_CURSOR_UP, 0 }, { 4, 4, UKEY_CURSOR_DOWN, 0 },
            { 4, 5, UKEY_CURSOR_LEFT, 0 }, { 4, 6, UKEY_CURSOR_RIGHT, 0 },
            // Row 5
            { 5, 0, ' ', 0 }, { 5, 1, '0', 0 }, { 5, 2, '1', 0 },
            { 5, 3, '2', 0 }, { 5, 4, '3', 0 }, { 5, 5, '4', 0 },
            { 5, 6, '5', 0 },
            // Row 6
            { 6, 0, '6', 0 }, { 6, 1, '7', 0 }, { 6, 2, '8', 0 },
            { 6, 3, '9', 0 }, { 6, 4, ':', 0 }, { 6, 5, ';', 0 },
            // Row 7
            { 7, 0, '\r', 0 }, { 7, 1, '\b', 0 },
            { 7, 3, UKEY_SHIFT_L, 0 },
        };
        static const HostKeyBinding bindings[] = {
            { SDLK_UP,        UKEY_CURSOR_UP    },
            { SDLK_DOWN,      UKEY_CURSOR_DOWN  },
            { SDLK_LEFT,      UKEY_CURSOR_LEFT  },
            { SDLK_RIGHT,     UKEY_CURSOR_RIGHT },
            { SDLK_LSHIFT,    UKEY_SHIFT_L      },
            { SDLK_RSHIFT,    UKEY_SHIFT_L      },
        };
        keyboard_matrix_apply(entries, bindings, keyboard_matrix_, key, pressed);
    } else {
        // Dragon keyboard: 53 keys (different scan code layout)
        static const KeyMatrixEntry entries[] = {
            // Row 0
            { 0, 0, '0', 0 }, { 0, 1, '1', 0 }, { 0, 2, '2', 0 },
            { 0, 3, '3', 0 }, { 0, 4, '4', 0 }, { 0, 5, '5', 0 },
            { 0, 6, '6', 0 },
            // Row 1
            { 1, 0, '7', 0 }, { 1, 1, '8', 0 }, { 1, 2, '9', 0 },
            { 1, 3, ':', 0 }, { 1, 4, ';', 0 }, { 1, 5, ',', 0 },
            { 1, 6, '-', 0 },
            // Row 2
            { 2, 0, '.', 0 }, { 2, 1, '/', 0 }, { 2, 2, '@', 0 },
            { 2, 3, 'a', 0 }, { 2, 4, 'b', 0 }, { 2, 5, 'c', 0 },
            { 2, 6, 'd', 0 },
            // Row 3
            { 3, 0, 'e', 0 }, { 3, 1, 'f', 0 }, { 3, 2, 'g', 0 },
            { 3, 3, 'h', 0 }, { 3, 4, 'i', 0 }, { 3, 5, 'j', 0 },
            { 3, 6, 'k', 0 },
            // Row 4
            { 4, 0, 'l', 0 }, { 4, 1, 'm', 0 }, { 4, 2, 'n', 0 },
            { 4, 3, 'o', 0 }, { 4, 4, 'p', 0 }, { 4, 5, 'q', 0 },
            { 4, 6, 'r', 0 },
            // Row 5
            { 5, 0, 's', 0 }, { 5, 1, 't', 0 }, { 5, 2, 'u', 0 },
            { 5, 3, 'v', 0 }, { 5, 4, 'w', 0 }, { 5, 5, 'x', 0 },
            { 5, 6, 'y', 0 },
            // Row 6
            { 6, 0, 'z', 0 }, { 6, 3, UKEY_CURSOR_UP, 0 },
            { 6, 4, UKEY_CURSOR_DOWN, 0 }, { 6, 5, UKEY_CURSOR_LEFT, 0 },
            { 6, 6, UKEY_CURSOR_RIGHT, 0 },
            // Row 7
            { 7, 0, ' ', 0 }, { 7, 1, '\r', 0 }, { 7, 2, '\x1B', 0 },
            { 7, 6, UKEY_SHIFT_L, 0 },
        };
        static const HostKeyBinding bindings[] = {
            { SDLK_UP,        UKEY_CURSOR_UP    },
            { SDLK_DOWN,      UKEY_CURSOR_DOWN  },
            { SDLK_LEFT,      UKEY_CURSOR_LEFT  },
            { SDLK_RIGHT,     UKEY_CURSOR_RIGHT },
            { SDLK_LSHIFT,    UKEY_SHIFT_L      },
            { SDLK_RSHIFT,    UKEY_SHIFT_L      },
        };
        keyboard_matrix_apply(entries, bindings, keyboard_matrix_, key, pressed);
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
