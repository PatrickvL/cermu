/*
 * z1013_system.cpp — Robotron Z1013 system implementation
 */

#include "systems/ddr/z1013/z1013_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "utils/charset_renderer.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor z1013_01_descriptor = {
    "Robotron Z1013.01", "Z1013.01",
    "Robotron Z1013.01 — U880 @ 2MHz, 16KB RAM, 32×32 text (1985)",
    "z1013", {"Z1013", "Z1013.01"},
    nullptr, {}, nullptr
};

static SystemDescriptor z1013_16_descriptor = {
    "Robotron Z1013.16", "Z1013.16",
    "Robotron Z1013.16 — U880 @ 2MHz, 16KB RAM, membrane keyboard (1987)",
    "z1013", {"Z1013.16"},
    nullptr, {}, nullptr
};

static SystemDescriptor z1013_64_descriptor = {
    "Robotron Z1013.64", "Z1013.64",
    "Robotron Z1013.64 — U880 @ 2MHz, 64KB RAM, ROM BASIC (1988)",
    "z1013", {"Z1013.64"},
    nullptr, {}, nullptr
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
    printf("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // ── Create memory chips from manifest and wire bus ────────────────────
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // Retain pointers for post-init access (rendering, ROM loading)
    video_ram_chip_   = board_.template chip_as<RAMChip>(Traits::kVideoRamSlot);
    monitor_rom_chip_ = board_.template chip_as<ROMChip>(Traits::kMonitorRomSlot);
    if constexpr (Traits::has_basic_rom) {
        basic_rom_lo_chip_ = board_.template chip_as<ROMChip>(Traits::kBasicRomLoSlot);
        basic_rom_hi_chip_ = board_.template chip_as<ROMChip>(Traits::kBasicRomHiSlot);
    }

    // ── Init chips ──────────────────────────────────────────────────────
    cpu_ = board_.template cpu<U880>();
    pio_ = board_.template chip_as<z80_pio_t>(Traits::kPioSlot);
    pins_ = board_.cpu_chip()->init();
    pio_->init();

    // Character ROM — not bus-mapped, used for display rendering only
    char_rom_.resize(z1013_constants::CHAR_ROM_SIZE, 0xFF);

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
    keyboard_column_select_ = 0xFF;

    if (!load_roms()) {
        printf("%s: Warning — ROMs not loaded\n", Traits::name);
    }

    // ── Register chips for Hardware menu ────────────────────────────────
    register_bus_chips(board_);

    // Display surface — owns index buffer, framebuffer, palette, pixel unit
    display_.init(z1013_constants::FB_WIDTH, z1013_constants::FB_HEIGHT);
    display_.set_palette(z1013_constants::PALETTE, 2);
    register_display(&display_);

    system_ready_ = true;
    return true;
}

template<Z1013Variant V> void Z1013System<V>::shutdown() { cpu_ = nullptr; system_ready_ = false; }
template<Z1013Variant V> void Z1013System<V>::reset() {
    if (!cpu_) return;
    board_.reset_chips();
    pins_ = board_.cpu_chip()->reset(pins_);
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
    keyboard_column_select_ = 0xFF;
}

template<Z1013Variant V>
void Z1013System<V>::tick() {
    if (!cpu_) return;

    // CPU tick (one T-state)
    pins_ = cpu_->tick(pins_);

    // Bus dispatch — check Z80-specific MREQ/IORQ signals
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(0, pins_);
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
}

template<Z1013Variant V> bool Z1013System<V>::load_file(const char*) { return false; }
template<Z1013Variant V> void Z1013System<V>::get_display_dimensions(int* w, int* h) const {
    *w = z1013_constants::FB_WIDTH; *h = z1013_constants::FB_HEIGHT;
}
template<Z1013Variant V> uint32_t Z1013System<V>::get_audio_samples(float*, uint32_t) { return 0; }
template<Z1013Variant V> void Z1013System<V>::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
template<Z1013Variant V> void Z1013System<V>::render_system_menu_items() {}
template<Z1013Variant V> void Z1013System<V>::render_configuration_ui() {}
template<Z1013Variant V> void Z1013System<V>::set_speed_multiplier(float m) { speed_multiplier_ = m; }

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
    auto set_key = [&](int row, int col) {
        if (row < 0 || row >= z1013_constants::KEYBOARD_ROWS) return;
        if (pressed)
            keyboard_matrix_[row] &= ~(uint8_t)(1 << col);
        else
            keyboard_matrix_[row] |=  (uint8_t)(1 << col);
    };

    switch (key) {
    // Row 4
    case SDLK_a: set_key(4,3); break;
    case SDLK_9: set_key(4,2); break;
    case SDLK_8: set_key(4,1); break;
    case SDLK_7: set_key(4,0); break;
    // Row 5
    case SDLK_6: set_key(5,3); break;
    case SDLK_5: set_key(5,2); break;
    case SDLK_4: set_key(5,1); break;
    case SDLK_3: set_key(5,0); break;
    // Row 6
    case SDLK_2:     set_key(6,3); break;
    case SDLK_1:     set_key(6,2); break;
    case SDLK_0:     set_key(6,1); break;
    case SDLK_SPACE: set_key(6,0); break;
    // Row 7
    case SDLK_LCTRL:
    case SDLK_RCTRL:  set_key(7,3); break;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT: set_key(7,2); break;
    case SDLK_RETURN: set_key(7,1); break;
    case SDLK_BACKSPACE:
    case SDLK_DELETE: set_key(7,0); break;
    // Row 0
    case SDLK_p: set_key(0,2); break;
    case SDLK_o: set_key(0,1); break;
    case SDLK_n: set_key(0,0); break;
    // Row 1
    case SDLK_m: set_key(1,3); break;
    case SDLK_l: set_key(1,2); break;
    case SDLK_k: set_key(1,1); break;
    case SDLK_j: set_key(1,0); break;
    // Row 2
    case SDLK_i: set_key(2,3); break;
    case SDLK_h: set_key(2,2); break;
    case SDLK_g: set_key(2,1); break;
    case SDLK_f: set_key(2,0); break;
    // Row 3
    case SDLK_e: set_key(3,3); break;
    case SDLK_d: set_key(3,2); break;
    case SDLK_c: set_key(3,1); break;
    case SDLK_b: set_key(3,0); break;
    default: break;
    }
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
                pio_->set_input(0, cols);
            }
            uint8_t data = pio_->read_data(port_sel);
            BUS_SET_DATA(pins, data);
        } else {
            uint8_t data = BUS_GET_DATA(pins);
            if (is_ctrl) {
                pio_->write_control(port_sel, data);
            } else {
                pio_->write_data(port_sel, data);
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
//
// If the character ROM has not been loaded, all characters render as blank.
// ============================================================================

template<Z1013Variant V>
void Z1013System<V>::render_frame() {
    static constexpr int COLS = z1013_constants::TEXT_COLS;
    static constexpr int ROWS = z1013_constants::TEXT_ROWS;

    const uint8_t* vram = video_ram_chip_ ? video_ram_chip_->data() : nullptr;
    if (!vram) return;

    display_.clear();
    charset_renderer::render_screen(display_.indices(), z1013_constants::FB_WIDTH,
                                    vram, char_rom_.data(),
                                    COLS, ROWS, 8, 8, 1, 0);
    display_.flush();
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<Z1013Variant V>
bool Z1013System<V>::load_roms() {
    char rom_root[512];
    const char* names[] = {"z1013", "Z1013", nullptr};
    if (!system_config_discover_rom_root(names, rom_root, sizeof(rom_root))) {
        printf("%s: ROM path not found\n", Traits::name);
        return false;
    }

    // Load manifest-declared ROMs (Monitor ROM)
    bool ok = board_.load_roms(rom_root, Traits::name);

    // Character ROM (2 KB) — not bus-mapped, used for rendering
    const char* char_names[] = {"z1013_char.rom", "charrom.bin", "CHAR.ROM", nullptr};
    rom_loader_load_from_root(rom_root, char_names,
                              z1013_constants::CHAR_ROM_SIZE,
                              char_rom_.data(), char_rom_.size());  // optional

    // BASIC ROM (10 KB, Z1013.64 only — split into 8 KB + 2 KB chips)
    if constexpr (Traits::has_basic_rom) {
        const char* basic_names[] = {"z1013_basic.rom", "BASIC.ROM", nullptr};
        uint8_t basic_buf[z1013_constants::BASIC_ROM_SIZE];
        std::memset(basic_buf, 0xFF, sizeof(basic_buf));
        if (rom_loader_load_from_root(rom_root, basic_names,
                                       z1013_constants::BASIC_ROM_SIZE,
                                       basic_buf, sizeof(basic_buf))) {
            std::memcpy(basic_rom_lo_chip_->data(), basic_buf, 8192);
            std::memcpy(basic_rom_hi_chip_->data(), basic_buf + 8192, 2048);
        } else {
            printf("%s: BASIC ROM not loaded\n", Traits::name);
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
