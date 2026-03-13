/*
 * z9001_system.cpp — Robotron Z9001 / KC 87 system implementation
 */

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
    nullptr, {}, nullptr
};

static SystemDescriptor kc87_descriptor = {
    "Robotron KC 87", "KC87",
    "Robotron KC 87 — U880 @ 2.4576MHz, 48KB RAM, color text, BASIC (1987)",
    "z9001", {"KC87", "KC-87"},
    nullptr, {}, nullptr
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
Z9001System<V>::~Z9001System() { delete cpu_; }

template<Z9001Variant V>
const SystemDescriptor& Z9001System<V>::get_descriptor() const {
    if constexpr (V == Z9001Variant::Z9001) return z9001_descriptor;
    else return kc87_descriptor;
}

template<Z9001Variant V> bool Z9001System<V>::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
template<Z9001Variant V> bool Z9001System<V>::apply_configuration() { return true; }

template<Z9001Variant V>
bool Z9001System<V>::initialize() {
    printf("%s: Initializing system\n", Traits::name);
    register_board(&board_);

    // ── Create chips via factory, wire the bus ────────────────────────
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // Retrieve typed pointers for chips accessed after initialize()
    video_ram_chip_ = board_.template chip_as<RAMChip>(Traits::kVideoRamSlot);
    os_rom_chip_    = board_.template chip_as<ROMChip>(Traits::kOsRomSlot);
    if constexpr (Traits::has_basic_rom) {
        basic_rom_lo_chip_ = board_.template chip_as<ROMChip>(Traits::kBasicRomLoSlot);
        basic_rom_hi_chip_ = board_.template chip_as<ROMChip>(Traits::kBasicRomHiSlot);
    }
    if constexpr (Traits::has_color_ram) {
        color_ram_chip_ = board_.template chip_as<RAMChip>(Traits::kColorRamSlot);
    }

    // ── Trim RAM pages for KC87 (48 KB out of 64 KB allocated) ──────────
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────
    cpu_ = new U880();
    pins_ = cpu_->init();
    pio1_.init();
    pio2_.init();
    ctc_.init();

    // Character ROM — not bus-mapped, used for display rendering only
    char_rom_.resize(z9001_constants::CHAR_ROM_SIZE, 0xFF);

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    if (!load_roms()) {
        printf("%s: Warning — ROMs not loaded\n", Traits::name);
    }

    // ── Register chips for Hardware menu ────────────────────────────────
    register_chip(static_cast<ChipBase*>(cpu_),
        "U880 CPU", "U880", "CPU", 0x0000);
    register_chip(&pio1_,
        "U855 PIO #1", "U855", "I/O", z9001_constants::PIO1_PORT_A);
    register_chip(&pio2_,
        "U855 PIO #2", "U855", "I/O", z9001_constants::PIO2_PORT_A);
    register_chip(&ctc_,
        "U857 CTC", "U857", "I/O", z9001_constants::CTC_CH0);
    register_bus_chips(board_);

    printf("%s: System initialized (RAM: %d KB)\n", Traits::name, Traits::ram_size / 1024);
    system_ready_ = true;
    return true;
}

template<Z9001Variant V> void Z9001System<V>::shutdown() { delete cpu_; cpu_ = nullptr; system_ready_ = false; }
template<Z9001Variant V> void Z9001System<V>::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    pio1_.init();
    pio2_.init();
    ctc_.init();
    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<Z9001Variant V>
void Z9001System<V>::configure_bus_memory_map() {
    if constexpr (V == Z9001Variant::KC87) {
        // KC87 allocates 64 KB RAM but real hardware has 48 KB ($0000-$BFFF).
        // Trim RAM write pages above $C000 that weren't overlaid by Color/Video RAM.
        // (BASIC ROM is read-only so apply() didn't map write pages for it;
        //  Color RAM and Video RAM already overlaid their write pages.)
        constexpr size_t kRamSlot   = 0;
        constexpr size_t kRamBaseId = BT::kManifest.base_id(kRamSlot, BT::Spec::PageBits);
        constexpr size_t ram_pages  = z9001_constants::RAM_SIZE_KC87 / Bus::kPageSize;
        for (size_t page = ram_pages; page < 256; ++page) {
            auto wr = bus_.viewer(0).write_chip(page);
            // Only unmap if this page still points to a RAM chip id
            if (size_t(wr) == kRamBaseId + page)
                bus_.set_write_page(0, page, PT::kNoChipSelectedWrite);
        }
    }
    // Z9001: RAM is exactly 16 KB, manifest allocates exactly 16 KB — no trimming needed.
}

// ============================================================================
// TICK
// ============================================================================

template<Z9001Variant V>
void Z9001System<V>::tick() {
    if (!cpu_) return;

    // CPU tick (one T-state)
    pins_ = cpu_->tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(0, pins_);
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
}

template<Z9001Variant V> bool Z9001System<V>::load_file(const char*) { return false; }
template<Z9001Variant V> uint32_t* Z9001System<V>::get_framebuffer() { return framebuffer_; }
template<Z9001Variant V> void Z9001System<V>::get_display_dimensions(int* w, int* h) const {
    *w = z9001_constants::FB_WIDTH; *h = z9001_constants::FB_HEIGHT;
}
template<Z9001Variant V> void Z9001System<V>::set_framebuffer(uint32_t*, int, int) {}
template<Z9001Variant V> uint32_t Z9001System<V>::get_audio_samples(float*, uint32_t) { return 0; }
template<Z9001Variant V> void Z9001System<V>::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
template<Z9001Variant V> void Z9001System<V>::render_system_menu_items() {}
template<Z9001Variant V> void Z9001System<V>::render_configuration_ui() {}
template<Z9001Variant V> void Z9001System<V>::set_speed_multiplier(float m) { speed_multiplier_ = m; }

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
                uint8_t row  = pio1_.get_output(0) & 0x07;
                uint8_t cols = keyboard_matrix_[row];
                pio1_.set_input(1, cols);
            }
            uint8_t data = pio1_.read_data(port_sel);
            BUS_SET_DATA(pins, data);
        } else {
            uint8_t data = BUS_GET_DATA(pins);
            if (is_ctrl) {
                pio1_.write_control(port_sel, data);
            } else {
                pio1_.write_data(port_sel, data);
            }
        }
    }
    // PIO2 $90–$93
    else if (port >= z9001_constants::PIO2_PORT_A && port <= z9001_constants::PIO2_CTRL_B) {
        uint8_t pio_idx  = port - z9001_constants::PIO2_PORT_A;
        int     port_sel = pio_idx & 0x01;
        bool    is_ctrl  = (pio_idx & 0x02) != 0;
        if (is_rd) {
            uint8_t data = pio2_.read_data(port_sel);
            BUS_SET_DATA(pins, data);
        } else {
            uint8_t data = BUS_GET_DATA(pins);
            if (is_ctrl) {
                pio2_.write_control(port_sel, data);
            } else {
                pio2_.write_data(port_sel, data);
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
//
// Color byte format (KC 87): bits 2-0 = foreground, bits 5-3 = background.
// ============================================================================

template<Z9001Variant V>
void Z9001System<V>::render_frame() {
    static constexpr int COLS = z9001_constants::TEXT_COLS;
    static constexpr int ROWS = z9001_constants::TEXT_ROWS;
    static constexpr int CW   = 8;
    static constexpr int CH   = 8;

    // Z9001/KC87 color palette (8 standard CGA-like colors)
    static constexpr uint32_t kPalette[8] = {
        0xFF000000,  // 0: Black
        0xFF0000AA,  // 1: Blue
        0xFF00AA00,  // 2: Green
        0xFF00AAAA,  // 3: Cyan
        0xFFAA0000,  // 4: Red
        0xFFAA00AA,  // 5: Magenta
        0xFFAA5500,  // 6: Brown/Dark Yellow
        0xFFAAAAAA,  // 7: Light Grey
    };

    const uint8_t* vram = video_ram_chip_ ? video_ram_chip_->data() : nullptr;
    if (!vram) return;

    const uint8_t* cram = nullptr;
    if constexpr (Traits::has_color_ram) {
        cram = color_ram_chip_ ? color_ram_chip_->data() : nullptr;
    }

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            int    pos  = row * COLS + col;
            uint8_t chr = vram[pos];
            int fb_x = col * CW;
            int fb_y = row * CH;

            // Determine foreground / background colors
            uint32_t fg, bg;
            if constexpr (Traits::has_color_ram) {
                if (cram) {
                    uint8_t attr = cram[pos];
                    fg = kPalette[attr & 0x07];
                    bg = kPalette[(attr >> 3) & 0x07];
                } else {
                    fg = 0xFFFFFFFF;
                    bg = 0xFF000000;
                }
            } else {
                fg = 0xFFFFFFFF;   // White on black (monochrome)
                bg = 0xFF000000;
            }

            for (int gy = 0; gy < CH; gy++) {
                uint8_t bits = char_rom_[chr * 8 + gy];
                for (int gx = 0; gx < CW; gx++) {
                    int    px  = fb_x + gx;
                    int    py  = fb_y + gy;
                    bool   set = (bits & (0x80u >> gx)) != 0;
                    framebuffer_[py * z9001_constants::FB_WIDTH + px] = set ? fg : bg;
                }
            }
        }
    }
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<Z9001Variant V>
bool Z9001System<V>::load_roms() {
    char rom_root[512];
    const char* names[] = {"z9001", "kc87", "Z9001", nullptr};
    if (!system_config_discover_rom_root(names, rom_root, sizeof(rom_root))) {
        printf("%s: ROM path not found\n", Traits::name);
        return false;
    }

    bool ok = true;

    // OS ROM (4 KB at $F000)
    const char* os_names[] = {"z9001_os.rom", "os.rom", "OS.ROM", nullptr};
    if (!rom_loader_load_from_root(rom_root, os_names,
                                   z9001_constants::OS_ROM_SIZE,
                                   os_rom_chip_->data(), os_rom_chip_->size_bytes())) {
        printf("%s: OS ROM not loaded\n", Traits::name);
        ok = false;
    }

    // Character ROM (2 KB, not bus-mapped)
    const char* char_names[] = {"z9001_char.rom", "charrom.bin", "CHAR.ROM", nullptr};
    rom_loader_load_from_root(rom_root, char_names,
                              z9001_constants::CHAR_ROM_SIZE,
                              char_rom_.data(), char_rom_.size());  // optional

    // BASIC ROM (10 KB split into 8 KB + 2 KB, KC 87 only)
    if constexpr (Traits::has_basic_rom) {
        const char* basic_names[] = {"z9001_basic.rom", "BASIC.ROM", nullptr};
        // Try loading the full 10 KB ROM into a temp buffer, then split
        std::vector<uint8_t> full_basic(z9001_constants::BASIC_ROM_SIZE, 0xFF);
        if (rom_loader_load_from_root(rom_root, basic_names,
                                      z9001_constants::BASIC_ROM_SIZE,
                                      full_basic.data(), full_basic.size())) {
            // Split: first 8 KB → basic_rom_lo, next 2 KB → basic_rom_hi
            std::memcpy(basic_rom_lo_chip_->data(), full_basic.data(), 8192);
            std::memcpy(basic_rom_hi_chip_->data(), full_basic.data() + 8192, 2048);
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

template class Z9001System<Z9001Variant::Z9001>;
template class Z9001System<Z9001Variant::KC87>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(z9001_descriptor, [] { return std::make_unique<Z9001System<Z9001Variant::Z9001>>(); });
REGISTER_SYSTEM(kc87_descriptor,  [] { return std::make_unique<Z9001System<Z9001Variant::KC87>>(); });
