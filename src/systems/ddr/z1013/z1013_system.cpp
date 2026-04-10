/*
 * z1013_system.cpp — Robotron Z1013 system implementation
 */

#include "core/cermu.hpp"
#include "systems/ddr/z1013/z1013_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include "utils/keyboard_matrix.hpp"
#include "utils/guest_key_chars.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor z1013_01_descriptor = {
    "Robotron Z1013.01", "Z1013.01",
    "Robotron Z1013.01 — U880 @ 2MHz, 16KB RAM, 32×32 text (1985)",
    "z1013", {"Z1013", "Z1013.01"},
    nullptr, {}, nullptr,
    "Robotron", 1985, z80::U880Traits.display_name, SystemType::Home
};

static SystemDescriptor z1013_16_descriptor = {
    "Robotron Z1013.16", "Z1013.16",
    "Robotron Z1013.16 — U880 @ 2MHz, 16KB RAM, membrane keyboard (1987)",
    "z1013", {"Z1013.16"},
    nullptr, {}, nullptr,
    "Robotron", 1987, z80::U880Traits.display_name, SystemType::Home
};

static SystemDescriptor z1013_64_descriptor = {
    "Robotron Z1013.64", "Z1013.64",
    "Robotron Z1013.64 — U880 @ 2MHz, 64KB RAM, ROM BASIC (1988)",
    "z1013", {"Z1013.64"},
    nullptr, {}, nullptr,
    "Robotron", 1988, z80::U880Traits.display_name, SystemType::Home
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

template<Z1013Variant V>
Z1013System<V>::Z1013System() : System(), pins_(Z1013_BUS_DEFAULT_STATE) {
    HardwareTraits traits = {};
    traits.display.native_width    = z1013_constants::FB_WIDTH;
    traits.display.native_height   = z1013_constants::FB_HEIGHT;
    traits.display.visible_width   = z1013_constants::FB_WIDTH;
    traits.display.visible_height  = z1013_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 2;  // Monochrome
    traits.timing.cpu_frequency_hz = z1013_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = 50;
    traits.timing.cycles_per_frame = z1013_constants::TSTATES_PER_FRAME;
    traits.timing.standard         = VideoStandard::PAL;
    hardware_traits_ = traits;
}

template<Z1013Variant V>
Z1013System<V>::~Z1013System() = default;

template<Z1013Variant V>
const SystemDescriptor& Z1013System<V>::get_descriptor() const {
    if constexpr (V == Z1013Variant::Z1013_01) return z1013_01_descriptor;
    else if constexpr (V == Z1013Variant::Z1013_16) return z1013_16_descriptor;
    else return z1013_64_descriptor;
}

template<Z1013Variant V> bool Z1013System<V>::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
template<Z1013Variant V> bool Z1013System<V>::apply_configuration() { return true; }

template<Z1013Variant V>
bool Z1013System<V>::initialize() {
    log_info("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // ── Bind and create chips from manifest, wire bus ───────────────────
    bind_all(board_, board_.components_, BT::kManifest);
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // ── Set port manifest ───────────────────────────────────────────────
    port_manifest_       = BT::kManifest.port_slots;
    port_manifest_count_ = BT::kManifest.port_count;

    // ── Init chips ──────────────────────────────────────────────────────
    pins_ = board_.z80.init();
    board_.pio.init();

    // Character ROM — not bus-mapped, used for display rendering only
    char_rom_.resize(z1013_constants::CHAR_ROM_SIZE, 0xFF);

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
    keyboard_column_select_ = 0xFF;

    if (!load_roms()) {
        log_info("%s: Warning — ROMs not loaded\n", Traits::name);
    }

    // ── Register chips for Hardware menu ────────────────────────────────
    register_bus_chips(board_);

    // Palette for GPU indexed rendering
    palette_.set(z1013_constants::PALETTE, 2);

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_display(nullptr, palette_.data(),
                              z1013_constants::FB_WIDTH, 1);
    video_port_->set_palette(palette_.data(), 2);
    video_port_->bind_frame_output(&last_frame_data_);

    // Video generator — models TTL character display circuitry
    video_gen_.set_video_out(&video_port_->output());
    video_gen_.set_geometry(z1013_constants::TEXT_COLS, z1013_constants::TEXT_ROWS,
                            8, 8,
                            z1013_constants::FB_WIDTH, z1013_constants::FB_HEIGHT);
    video_gen_.set_default_colors(1, 0);  // green on black

    system_ready_ = true;
    return true;
}

template<Z1013Variant V> void Z1013System<V>::shutdown() { system_ready_ = false; }
template<Z1013Variant V> void Z1013System<V>::reset() {
    if (!system_ready_) return;
    board_.reset_chips();
    pins_ = board_.z80.reset(pins_);
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
    keyboard_column_select_ = 0xFF;
}

template<Z1013Variant V>
void Z1013System<V>::tick() {
    if (!system_ready_) return;

    // CPU tick (one T-state)
    pins_ = board_.z80.tick(pins_);

    // Bus dispatch — check Z80-specific MREQ/IORQ signals
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    total_cycles_++;

    // Refresh display once per frame
    if (total_cycles_ % z1013_constants::TSTATES_PER_FRAME == 0) {
        render_frame();
    }
}

template<Z1013Variant V> void Z1013System<V>::run_frame() {
    const uint32_t cycles = static_cast<uint32_t>(
        z1013_constants::TSTATES_PER_FRAME * speed_multiplier_);
    for (uint32_t i = 0; i < cycles; ++i) tick();
    if (video_port_) video_port_->swap_frame();
}

// ============================================================================
// KEYBOARD HANDLING
// ============================================================================
//
// Z1013 keyboard matrix (8 rows × 4 columns), scanned through the Z80 PIO:
//   Port A (input):  column data (active-low when key pressed)
//   I/O port $08:    row select (one bit high = scan that row)
//
// Matrix layout (Z1013.01 membrane keyboard):
//   Row/Col:  3       2       1       0
//   0:       DEL     P       O       N
//   1:       M       L       K       J
//   2:       I       H       G       F
//   3:       E       D       C       B
//   4:       A       9       8       7
//   5:       6       5       4       3
//   6:       2       1       0       SPACE
//   7:       CTRL    SHIFT   ENTER   BACK
// ============================================================================

template<Z1013Variant V>
void Z1013System<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    // Z1013 keyboard matrix: 8 rows × 4 columns, active-low
    static const KeyMatrixEntry entries[] = {
        // Row 0: P O N
        { 0, 2, 'p', 0 }, { 0, 1, 'o', 0 }, { 0, 0, 'n', 0 },
        // Row 1: M L K J
        { 1, 3, 'm', 0 }, { 1, 2, 'l', 0 }, { 1, 1, 'k', 0 }, { 1, 0, 'j', 0 },
        // Row 2: I H G F
        { 2, 3, 'i', 0 }, { 2, 2, 'h', 0 }, { 2, 1, 'g', 0 }, { 2, 0, 'f', 0 },
        // Row 3: E D C B
        { 3, 3, 'e', 0 }, { 3, 2, 'd', 0 }, { 3, 1, 'c', 0 }, { 3, 0, 'b', 0 },
        // Row 4: A 9 8 7
        { 4, 3, 'a', 0 }, { 4, 2, '9', 0 }, { 4, 1, '8', 0 }, { 4, 0, '7', 0 },
        // Row 5: 6 5 4 3
        { 5, 3, '6', 0 }, { 5, 2, '5', 0 }, { 5, 1, '4', 0 }, { 5, 0, '3', 0 },
        // Row 6: 2 1 0 SPACE
        { 6, 3, '2', 0 }, { 6, 2, '1', 0 }, { 6, 1, '0', 0 }, { 6, 0, ' ', 0 },
        // Row 7: CTRL SHIFT ENTER BACK
        { 7, 3, UKEY_CTRL_L, 0 },
        { 7, 2, UKEY_SHIFT_L, 0 },
        { 7, 1, '\r', 0 },
        { 7, 0, '\b', 0 },
    };

    static const HostKeyBinding bindings[] = {
        { SDLK_LCTRL,     UKEY_CTRL_L  },
        { SDLK_RCTRL,     UKEY_CTRL_L  },
        { SDLK_LSHIFT,    UKEY_SHIFT_L },
        { SDLK_RSHIFT,    UKEY_SHIFT_L },
        { SDLK_DELETE,    '\b'         },  // DELETE → same as BACKSPACE
    };

    keyboard_matrix_apply(entries, bindings, keyboard_matrix_, key, pressed);
}

// ============================================================================
// I/O BUS DISPATCH
// ============================================================================

template<Z1013Variant V>
bus_state_t Z1013System<V>::io_tick(bus_state_t pins) {
    uint8_t port  = static_cast<uint8_t>(BUS_GET_ADDR(pins) & 0xFF);
    bool    is_rd = BUS_GET_BIT(pins, BUS_RW_BIT);

    if (port <= z1013_constants::PIO_CTRL_B) {
        // $00–$03: Z80 PIO (keyboard + cassette)
        // Address bits: A0=port select (0=Port A, 1=Port B); A1=type (0=data, 1=control)
        int  port_sel = port & 0x01;     // 0=Port A, 1=Port B
        bool is_ctrl  = (port & 0x02) != 0;
        if (is_rd) {
            // Before reading Port A, update column data from keyboard matrix.
            // keyboard_column_select_ holds the active row bitmask.
            if (port_sel == 0) {
                uint8_t cols = 0xFF;
                for (int r = 0; r < z1013_constants::KEYBOARD_ROWS; r++) {
                    if (keyboard_column_select_ & (1u << r)) {
                        cols &= keyboard_matrix_[r];
                    }
                }
                board_.pio.set_input(0, cols);
            }
            uint8_t data = board_.pio.read_data(port_sel);
            BUS_SET_DATA(pins, data);
        } else {
            uint8_t data = BUS_GET_DATA(pins);
            if (is_ctrl) {
                board_.pio.write_control(port_sel, data);
            } else {
                board_.pio.write_data(port_sel, data);
            }
        }
    } else if (port == z1013_constants::KEYBOARD_SEL_PORT) {
        // $08: keyboard row select register
        if (!is_rd) {
            keyboard_column_select_ = BUS_GET_DATA(pins);
        }
    }

    return pins;
}

// ============================================================================
// DISPLAY RENDERING
// ============================================================================
//
// The Z1013 has a 32×32 character display. Each screen position maps to one
// byte in video RAM ($EC00–$EFFF). The character ROM provides 8×8 bitmaps
// for 256 characters (2 KB ROM). Each character is rendered as 8×8 pixels.
// ============================================================================
// VIDEO RENDERING — delegate to CharDisplayGenerator
// ============================================================================

template<Z1013Variant V>
void Z1013System<V>::render_frame() {
    const uint8_t* vram = board_.vram.data();
    if (!vram) return;

    video_gen_.set_vram(vram);
    video_gen_.set_char_rom(char_rom_.data());
    video_gen_.render_frame();
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<Z1013Variant V>
bool Z1013System<V>::load_roms() {
    char rom_root[512];
    const char* names[] = {"z1013", "Z1013", nullptr};
    if (!system_config_discover_rom_root(names, rom_root, sizeof(rom_root))) {
        log_info("%s: ROM path not found\n", Traits::name);
        return false;
    }

    // Load manifest-declared ROMs (Monitor ROM)
    bool ok = board_.load_roms(rom_root, Traits::name);

    // Character ROM (2 KB) — not bus-mapped, used for rendering
    rom_loader_load_from_root(rom_root,
                              "z1013_char.rom|charrom.bin|CHAR.ROM",
                              z1013_constants::CHAR_ROM_SIZE,
                              char_rom_.data(), char_rom_.size());  // optional

    // BASIC ROM (10 KB, Z1013.64 only — split into 8 KB + 2 KB chips)
    if constexpr (Traits::has_basic_rom) {
        uint8_t basic_buf[z1013_constants::BASIC_ROM_SIZE];
        std::memset(basic_buf, 0xFF, sizeof(basic_buf));
        if (rom_loader_load_from_root(rom_root,
                                       "z1013_basic.rom|BASIC.ROM",
                                       z1013_constants::BASIC_ROM_SIZE,
                                       basic_buf, sizeof(basic_buf))) {
            std::memcpy(board_.basic_lo.data(), basic_buf, 8192);
            std::memcpy(board_.basic_hi.data(), basic_buf + 8192, 2048);
        } else {
            log_info("%s: BASIC ROM not loaded\n", Traits::name);
            ok = false;
        }
    }

    return ok;
}

// ============================================================================
// EXPLICIT INSTANTIATIONS
// ============================================================================

template class Z1013System<Z1013Variant::Z1013_01>;
template class Z1013System<Z1013Variant::Z1013_16>;
template class Z1013System<Z1013Variant::Z1013_64>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(z1013_01_descriptor, [] { return std::make_unique<Z1013System<Z1013Variant::Z1013_01>>(); });
REGISTER_SYSTEM(z1013_16_descriptor, [] { return std::make_unique<Z1013System<Z1013Variant::Z1013_16>>(); });
REGISTER_SYSTEM(z1013_64_descriptor, [] { return std::make_unique<Z1013System<Z1013Variant::Z1013_64>>(); });
