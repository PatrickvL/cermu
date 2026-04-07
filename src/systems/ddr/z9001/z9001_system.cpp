/*
 * z9001_system.cpp — Robotron Z9001 / KC 87 system implementation
 */

#include "core/cermu.hpp"
#include "systems/ddr/z9001/z9001_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor z9001_descriptor = {
    "Robotron Z9001", "Z9001",
    "Robotron Z9001 — U880 @ 2.4576MHz, 16KB RAM, 40×24 text (1984)",
    "z9001", {"Z9001", "KC85/1"},
    nullptr, {}, nullptr,
    "Robotron", 1984, z80::U880Traits.display_name, SystemType::Home
};

static SystemDescriptor kc87_descriptor = {
    "Robotron KC 87", "KC87",
    "Robotron KC 87 — U880 @ 2.4576MHz, 48KB RAM, color text, BASIC (1987)",
    "z9001", {"KC87", "KC-87"},
    nullptr, {}, nullptr,
    "Robotron", 1987, z80::U880Traits.display_name, SystemType::Home
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

template<Z9001Variant V>
Z9001System<V>::Z9001System() : System(), pins_(Z9001_BUS_DEFAULT_STATE) {
    HardwareTraits traits = {};
    traits.display.native_width    = z9001_constants::FB_WIDTH;
    traits.display.native_height   = z9001_constants::FB_HEIGHT;
    traits.display.visible_width   = z9001_constants::FB_WIDTH;
    traits.display.visible_height  = z9001_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = Traits::has_color_ram ? z9001_constants::COLOR_COUNT : 2;
    traits.timing.cpu_frequency_hz = z9001_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = 50;
    traits.timing.cycles_per_frame = z9001_constants::TSTATES_PER_FRAME;
    traits.timing.standard         = VideoStandard::PAL;
    hardware_traits_ = traits;
}

template<Z9001Variant V>
Z9001System<V>::~Z9001System() {}

template<Z9001Variant V>
const SystemDescriptor& Z9001System<V>::get_descriptor() const {
    if constexpr (V == Z9001Variant::Z9001) return z9001_descriptor;
    else return kc87_descriptor;
}

template<Z9001Variant V> bool Z9001System<V>::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
template<Z9001Variant V> bool Z9001System<V>::apply_configuration() { return true; }

template<Z9001Variant V>
bool Z9001System<V>::initialize() {
    log_info("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // ── Bind value-typed chips, then factory-create remaining ──────────
    bind_all(board_, board_.components_, BT::kManifest);
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // ── Set port manifest ───────────────────────────────────────────────
    port_manifest_       = BT::kManifest.port_slots;
    port_manifest_count_ = BT::kManifest.port_count;

    // ── Trim RAM pages for KC87 (48 KB out of 64 KB allocated) ──────────
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────────
    pins_ = board_.z80.init();
    board_.pio1.init();
    board_.pio2.init();
    board_.ctc.init();

    // Character ROM — not bus-mapped, used for display rendering only
    char_rom_.resize(z9001_constants::CHAR_ROM_SIZE, 0xFF);

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    if (!load_roms()) {
        log_info("%s: Warning — ROMs not loaded\n", Traits::name);
    }

    // ── Register chips for Hardware menu ────────────────────────────────

    register_bus_chips(board_);

    // Palette for GPU indexed rendering
    palette_.set(z9001_constants::PALETTE, 9);

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();
    video_port_->bind_display(nullptr, palette_.data(),
                              z9001_constants::FB_WIDTH, 1);
    video_port_->set_palette(palette_.data(), 9);
    video_port_->bind_frame_output(&last_frame_data_);

    // Video generator — models TTL character display circuitry
    video_gen_.set_video_out(&video_port_->output());
    video_gen_.set_geometry(z9001_constants::TEXT_COLS, z9001_constants::TEXT_ROWS,
                            8, 8,
                            z9001_constants::FB_WIDTH, z9001_constants::FB_HEIGHT);
    video_gen_.set_default_colors(8, 0);  // white on black

    log_info("%s: System initialized (RAM: %d KB)\n", Traits::name, Traits::ram_size / 1024);
    system_ready_ = true;
    return true;
}

template<Z9001Variant V> void Z9001System<V>::shutdown() { system_ready_ = false; }
template<Z9001Variant V> void Z9001System<V>::reset() {
    if (!system_ready_) return;
    board_.reset_chips();
    pins_ = board_.z80.reset(pins_);
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<Z9001Variant V>
void Z9001System<V>::configure_bus_memory_map() {
    // KC87 RAM trimming is now declarative (effective_size in manifest).
    // Z9001: RAM is exactly 16 KB — no trimming needed.
}

// ============================================================================
// TICK
// ============================================================================

template<Z9001Variant V>
void Z9001System<V>::tick() {
    if (!system_ready_) return;

    // CPU tick (one T-state)
    pins_ = board_.z80.tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    total_cycles_++;

    // Refresh display once per frame
    if (total_cycles_ % z9001_constants::TSTATES_PER_FRAME == 0) {
        render_frame();
    }
}

template<Z9001Variant V> void Z9001System<V>::run_frame() {
    const uint32_t cycles = static_cast<uint32_t>(
        z9001_constants::TSTATES_PER_FRAME * speed_multiplier_);
    for (uint32_t i = 0; i < cycles; ++i) tick();
    if (video_port_) video_port_->swap_frame();
}

// ============================================================================
// KEYBOARD HANDLING
// ============================================================================
//
// Z9001 / KC 87 keyboard matrix (8 rows × 8 columns):
//   PIO1 Port A: row select — CPU writes a row number (0–7)
//   PIO1 Port B: column data (active-low when key pressed)
//
// Standard layout (Z9001 / KC 87 German keyboard):
//   Row/Col:  7     6     5     4     3     2     1     0
//   0:       0     1     2     3     4     5     6     7
//   1:       8     9     :     ;     ,     =     .     _
//   2:       NL    A     B     C     D     E     F     G
//   3:       H     I     J     K     L     M     N     O
//   4:       P     Q     R     S     T     U     V     W
//   5:       X     Y     Z     +     -     <     >     ?
//   6:       BRK   CLR   INS   DEL   LEFT  RIGHT UP   DOWN
//   7:       HOME  END   TAB   CAPS  CTRL  SHIFT ALT  ENTER
// ============================================================================

template<Z9001Variant V>
void Z9001System<V>::handle_keyboard_event(SDL_Keycode key, bool pressed) {
    auto set_key = [&](int row, int col) {
        if (row < 0 || row >= z9001_constants::KEYBOARD_ROWS) return;
        if (pressed)
            keyboard_matrix_[row] &= ~(uint8_t)(1 << col);
        else
            keyboard_matrix_[row] |=  (uint8_t)(1 << col);
    };

    switch (key) {
    // Row 0: digits 0–7
    case SDLK_0: set_key(0,7); break;
    case SDLK_1: set_key(0,6); break;
    case SDLK_2: set_key(0,5); break;
    case SDLK_3: set_key(0,4); break;
    case SDLK_4: set_key(0,3); break;
    case SDLK_5: set_key(0,2); break;
    case SDLK_6: set_key(0,1); break;
    case SDLK_7: set_key(0,0); break;
    // Row 1: digits 8–9, punctuation
    case SDLK_8:         set_key(1,7); break;
    case SDLK_9:         set_key(1,6); break;
    case SDLK_SEMICOLON: set_key(1,5); break;
    case SDLK_COMMA:     set_key(1,3); break;
    case SDLK_EQUALS:    set_key(1,2); break;
    case SDLK_PERIOD:    set_key(1,1); break;
    // Row 2: A–G
    case SDLK_a: set_key(2,6); break;
    case SDLK_b: set_key(2,5); break;
    case SDLK_c: set_key(2,4); break;
    case SDLK_d: set_key(2,3); break;
    case SDLK_e: set_key(2,2); break;
    case SDLK_f: set_key(2,1); break;
    case SDLK_g: set_key(2,0); break;
    // Row 3: H–O
    case SDLK_h: set_key(3,7); break;
    case SDLK_i: set_key(3,6); break;
    case SDLK_j: set_key(3,5); break;
    case SDLK_k: set_key(3,4); break;
    case SDLK_l: set_key(3,3); break;
    case SDLK_m: set_key(3,2); break;
    case SDLK_n: set_key(3,1); break;
    case SDLK_o: set_key(3,0); break;
    // Row 4: P–W
    case SDLK_p: set_key(4,7); break;
    case SDLK_q: set_key(4,6); break;
    case SDLK_r: set_key(4,5); break;
    case SDLK_s: set_key(4,4); break;
    case SDLK_t: set_key(4,3); break;
    case SDLK_u: set_key(4,2); break;
    case SDLK_v: set_key(4,1); break;
    case SDLK_w: set_key(4,0); break;
    // Row 5: X–Z, punctuation
    case SDLK_x: set_key(5,7); break;
    case SDLK_y: set_key(5,6); break;
    case SDLK_z: set_key(5,5); break;
    // Row 6: cursor and editing
    case SDLK_INSERT:    set_key(6,5); break;
    case SDLK_DELETE:    set_key(6,4); break;
    case SDLK_LEFT:      set_key(6,3); break;
    case SDLK_RIGHT:     set_key(6,2); break;
    case SDLK_UP:        set_key(6,1); break;
    case SDLK_DOWN:      set_key(6,0); break;
    // Row 7: modifiers
    case SDLK_TAB:        set_key(7,5); break;
    case SDLK_CAPSLOCK:   set_key(7,4); break;
    case SDLK_LCTRL:
    case SDLK_RCTRL:      set_key(7,3); break;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:     set_key(7,2); break;
    case SDLK_RETURN:     set_key(7,0); break;
    case SDLK_SPACE:      set_key(2,7); break;  // row 2, col 7 = Space/NL key position
    default: break;
    }
}

// ============================================================================
// I/O BUS DISPATCH
// ============================================================================
//
// I/O port map:
//   $80/$83 — CTC channels 0–3
//   $88/$8B — PIO1 (keyboard)
//   $90/$93 — PIO2 (cassette / joystick)
// ============================================================================

template<Z9001Variant V>
bus_state_t Z9001System<V>::io_tick(bus_state_t pins) {
    uint8_t port  = static_cast<uint8_t>(BUS_GET_ADDR(pins) & 0xFF);
    bool    is_rd = BUS_GET_BIT(pins, BUS_RW_BIT);

    // CTC channels $80–$83
    if (port >= z9001_constants::CTC_CH0 && port <= z9001_constants::CTC_CH3) {
        // CTC access — not yet wired; ignore for now
        (void)is_rd;
    }
    // PIO1 $88–$8B  (A0=port select, A1=data vs control)
    else if (port >= z9001_constants::PIO1_PORT_A && port <= z9001_constants::PIO1_CTRL_B) {
        uint8_t pio_idx  = port - z9001_constants::PIO1_PORT_A;
        int     port_sel = pio_idx & 0x01;      // 0=Port A, 1=Port B
        bool    is_ctrl  = (pio_idx & 0x02) != 0;
        if (is_rd) {
            // Port A output holds the keyboard row select; Port B returns column data
            if (port_sel == 0) {
                uint8_t row  = board_.pio1.get_output(0) & 0x07;
                uint8_t cols = keyboard_matrix_[row];
                board_.pio1.set_input(1, cols);
            }
            uint8_t data = board_.pio1.read_data(port_sel);
            BUS_SET_DATA(pins, data);
        } else {
            uint8_t data = BUS_GET_DATA(pins);
            if (is_ctrl) {
                board_.pio1.write_control(port_sel, data);
            } else {
                board_.pio1.write_data(port_sel, data);
            }
        }
    }
    // PIO2 $90–$93
    else if (port >= z9001_constants::PIO2_PORT_A && port <= z9001_constants::PIO2_CTRL_B) {
        uint8_t pio_idx  = port - z9001_constants::PIO2_PORT_A;
        int     port_sel = pio_idx & 0x01;
        bool    is_ctrl  = (pio_idx & 0x02) != 0;
        if (is_rd) {
            uint8_t data = board_.pio2.read_data(port_sel);
            BUS_SET_DATA(pins, data);
        } else {
            uint8_t data = BUS_GET_DATA(pins);
            if (is_ctrl) {
                board_.pio2.write_control(port_sel, data);
            } else {
                board_.pio2.write_data(port_sel, data);
            }
        }
    }

    return pins;
}

// ============================================================================
// DISPLAY RENDERING
// ============================================================================
//
// The Z9001 displays 40×24 characters (320×192 pixels).
// The screen buffer is at $EC00 (video_ram_, 1 KB).
// The color RAM (KC 87 only) is at $E800 (color_ram_, 1 KB).
// The character ROM provides 8×8 bitmaps for 256 characters (2 KB).
// ============================================================================
// VIDEO RENDERING — delegate to CharDisplayGenerator
// ============================================================================

template<Z9001Variant V>
void Z9001System<V>::render_frame() {
    const uint8_t* vram = board_.video_ram.data();

    video_gen_.set_vram(vram);
    video_gen_.set_char_rom(char_rom_.data());

    if constexpr (Traits::has_color_ram) {
        const uint8_t* cram = board_.color_ram.data();
        // KC87: per-character fg (bits 2:0) and bg (bits 5:3) from color RAM
        video_gen_.set_color_attr({cram, 0x07, 0, 0x38, 3});
    }

    video_gen_.render_frame();
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<Z9001Variant V>
bool Z9001System<V>::load_roms() {
    char rom_root[512];
    const char* names[] = {"z9001", "kc87", "Z9001", nullptr};
    if (!system_config_discover_rom_root(names, rom_root, sizeof(rom_root))) {
        log_info("%s: ROM path not found\n", Traits::name);
        return false;
    }

    // Load manifest-declared ROMs (OS ROM)
    bool ok = board_.load_roms(rom_root, Traits::name);

    // Character ROM (2 KB, not bus-mapped)
    rom_loader_load_from_root(rom_root,
                              "z9001_char.rom|charrom.bin|CHAR.ROM",
                              z9001_constants::CHAR_ROM_SIZE,
                              char_rom_.data(), char_rom_.size());  // optional

    // BASIC ROM (10 KB split into 8 KB + 2 KB, KC 87 only)
    if constexpr (Traits::has_basic_rom) {
        std::vector<uint8_t> full_basic(z9001_constants::BASIC_ROM_SIZE, 0xFF);
        if (rom_loader_load_from_root(rom_root,
                                      "z9001_basic.rom|BASIC.ROM",
                                      z9001_constants::BASIC_ROM_SIZE,
                                      full_basic.data(), full_basic.size())) {
            // Split: first 8 KB → basic_rom_lo, next 2 KB → basic_rom_hi
            std::memcpy(board_.basic_rom_lo.data(), full_basic.data(), 8192);
            std::memcpy(board_.basic_rom_hi.data(), full_basic.data() + 8192, 2048);
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

template class Z9001System<Z9001Variant::Z9001>;
template class Z9001System<Z9001Variant::KC87>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(z9001_descriptor, [] { return std::make_unique<Z9001System<Z9001Variant::Z9001>>(); });
REGISTER_SYSTEM(kc87_descriptor,  [] { return std::make_unique<Z9001System<Z9001Variant::KC87>>(); });
