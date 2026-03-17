/*
 * kc85_system.cpp — KC 85/2, /3, /4 system implementation
 */

#include "systems/ddr/kc85/kc85_system.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor kc85_2_descriptor = {
    "KC 85/2", "KC85/2",
    "VEB Mühlhausen KC 85/2 (HC 900) — U880 @ 1.77MHz, 16KB RAM, CAOS 2.2 (1984)",
    "kc85", {"KC85/2", "HC900", "HC-900"},
    nullptr, {}, nullptr
};

static SystemDescriptor kc85_3_descriptor = {
    "KC 85/3", "KC85/3",
    "VEB Mühlhausen KC 85/3 — U880 @ 1.77MHz, 16KB RAM, BASIC, CAOS 3.1 (1986)",
    "kc85", {"KC85/3"},
    nullptr, {}, nullptr
};

static SystemDescriptor kc85_4_descriptor = {
    "KC 85/4", "KC85/4",
    "VEB Mühlhausen KC 85/4 — U880 @ 1.77MHz, 64KB RAM, dual-plane video, CAOS 4.2 (1989)",
    "kc85", {"KC85/4"},
    nullptr, {}, nullptr
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

template<KC85Variant V>
KC85System<V>::KC85System() : System(), pins_(KC85_BUS_DEFAULT_STATE) {
    HardwareTraits traits = {};
    traits.display.native_width    = kc85_constants::FB_WIDTH;
    traits.display.native_height   = kc85_constants::FB_HEIGHT;
    traits.display.visible_width   = kc85_constants::FB_WIDTH;
    traits.display.visible_height  = kc85_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = kc85_constants::COLOR_COUNT;
    traits.timing.cpu_frequency_hz = kc85_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = 50;
    traits.timing.cycles_per_frame = kc85_constants::TSTATES_PER_FRAME;
    traits.timing.standard         = VideoStandard::PAL;
    hardware_traits_ = traits;
}

template<KC85Variant V>
KC85System<V>::~KC85System() {}

template<KC85Variant V>
const SystemDescriptor& KC85System<V>::get_descriptor() const {
    if constexpr (V == KC85Variant::KC85_2) return kc85_2_descriptor;
    else if constexpr (V == KC85Variant::KC85_3) return kc85_3_descriptor;
    else return kc85_4_descriptor;
}

template<KC85Variant V> bool KC85System<V>::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
template<KC85Variant V> bool KC85System<V>::apply_configuration() { return true; }

template<KC85Variant V>
bool KC85System<V>::initialize() {
    printf("%s: Initializing system (CAOS %s)\n", Traits::name, Traits::caos_version);
    register_board(&board_);

    // ── Create chips via factory, wire the bus ────────────────────────
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // Retrieve typed pointers for chips accessed after initialize()
    caos_rom_chip_ = board_.template chip_as<ROMChip>(Traits::kCaosRomSlot);
    if constexpr (Traits::has_basic_rom) {
        basic_rom_chip_ = board_.template chip_as<ROMChip>(Traits::kBasicRomSlot);
    }
    irm_chip_ = board_.template chip_as<RAMChip>(1);  // IRM always at slot 1
    cpu_     = board_.template cpu<U880>();
    pio1_    = board_.template chip_as<z80_pio_t>(Traits::kPio1Slot);
    pio2_    = board_.template chip_as<z80_pio_t>(Traits::kPio2Slot);
    ctc_     = board_.template chip_as<z80_ctc_t>(Traits::kCtcSlot);
    modules_ = board_.template chip_as<kc85_module_system_t>(Traits::kModulesSlot);

    // ── Set initial banking state ───────────────────────────────────────
    caos_rom_on_  = true;
    irm_enabled_  = true;
    basic_rom_on_ = false;
    bank_ctrl_    = 0;
    bank_ctrl2_   = 0;
    active_plane_ = 0;
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────
    pins_ = board_.cpu_chip()->init();
    pio1_->init();
    pio2_->init();
    ctc_->init();
    modules_->init();

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    load_roms();

    // ── Register chips for Hardware menu ──────────────────────────────
    register_bus_chips(board_);

    // ── GPU indexed palette rendering ───────────────────────────────
    // Display surface
    display_.init(kc85_constants::FB_WIDTH, kc85_constants::FB_HEIGHT);
    display_.set_palette(kc85_constants::PALETTE, kc85_constants::COLOR_COUNT);
    register_display(&display_);

    printf("%s: System initialized (RAM: %d KB, IRM: %d KB)\n",
           Traits::name, Traits::ram_size / 1024,
           Traits::has_extended_video ? 64 : 16);
    system_ready_ = true;
    return true;
}

template<KC85Variant V> void KC85System<V>::shutdown() { cpu_ = nullptr; system_ready_ = false; }
template<KC85Variant V> void KC85System<V>::reset() {
    if (!cpu_) return;
    board_.reset_chips();
    pins_ = board_.cpu_chip()->reset(pins_);
    modules_->init();
    bank_ctrl_ = 0;
    bank_ctrl2_ = 0;
    caos_rom_on_ = true;
    basic_rom_on_ = false;
    irm_enabled_ = true;
    active_plane_ = 0;
    configure_bus_memory_map();
}

// ============================================================================
// BUS CONFIGURATION
// ============================================================================

template<KC85Variant V>
void KC85System<V>::configure_bus_memory_map() {
    // apply() maps base-layer chips (RAM, plus IRM bank 0 for KC85/4) and
    // skips overlay_group > 0 slots (IRM for /2-/3, ROMs for all variants).
    // KC85/4 IRM effective_size=16384 limits Phase 1 to one 16 KB bank;
    // the full 64 KB buffer is still available for select_bank_at().
    board_.apply(bus_);

    // Build overlay snapshots from the manifest's overlay_group tags.
    board_.build_overlay_snapshots(bus_, 1, snapshots_);

    // Apply initial banking state.
    apply_banking();
}

// ============================================================================
// DYNAMIC BANKING
// ============================================================================

// ── Apply combined banking state ─────────────────────────────────────────
// Loads the pre-computed overlay snapshot for the current ROM/IRM enable
// flags, then manually adjusts IRM on KC85/4 (bank selection).
template<KC85Variant V>
void KC85System<V>::apply_banking() {
    // Compute overlay mode index from enable flags.
    // KC85/2: bit 0=IRM, bit 1=CAOS                     (groups 1,2 → 4 modes)
    // KC85/3: bit 0=IRM, bit 1=BASIC, bit 2=CAOS        (groups 1,2,3 → 8 modes)
    // KC85/4: bit 0=BASIC, bit 1=CAOS                   (groups 1,2 → 4 modes)
    size_t mode = 0;
    if constexpr (Traits::has_extended_video) {
        // KC85/4: IRM is base layer (not in overlay groups)
        if (basic_rom_on_) mode |= 1;
        if (caos_rom_on_)  mode |= 2;
    } else if constexpr (Traits::has_basic_rom) {
        // KC85/3: IRM=group 1, BASIC=group 2, CAOS=group 3
        if (irm_enabled_)  mode |= 1;
        if (basic_rom_on_) mode |= 2;
        if (caos_rom_on_)  mode |= 4;
    } else {
        // KC85/2: IRM=group 1, CAOS=group 2
        if (irm_enabled_)  mode |= 1;
        if (caos_rom_on_)  mode |= 2;
    }
    bus_.load_snapshot(0, snapshots_[0][mode]);

    // ── KC85/4: manual IRM handling (not in overlay groups) ─────────────
    if constexpr (Traits::has_extended_video) {
        if (irm_enabled_) {
            constexpr size_t kIrmSlot = 1;
            uint8_t bank = (bank_ctrl_ >> 1) & 0x03;  // io84 bits [2:1] select CPU bank
            board_.select_bank_at(bus_, 0, kIrmSlot, bank, 0x80);
        } else {
            // IRM disabled — unmap $80-$BF
            bus_.map_no_chip_selected(0, 0x80, 0x40);
        }
        active_plane_ = bank_ctrl_ & 0x01;
    }
}

// ── PIO B banking update ─────────────────────────────────────────────────
// Reads current PIO B output, updates enable flags, and re-applies banking
// if any flag changed.
template<KC85Variant V>
void KC85System<V>::update_bank_state() {
    // Banking is controlled by PIO Port A (not Port B)
    uint8_t pio_a = pio1_->get_output(0);

    bool new_caos = (pio_a & 0x01) != 0;   // PIO-A bit 0: CAOS ROM at E000
    bool new_irm  = (pio_a & 0x04) != 0;   // PIO-A bit 2: IRM at 8000
    bool new_basic = basic_rom_on_;
    if constexpr (Traits::has_basic_rom)
        new_basic = (pio_a & 0x80) != 0;   // PIO-A bit 7: BASIC ROM at C000

    if (new_irm != irm_enabled_ || new_caos != caos_rom_on_ ||
        new_basic != basic_rom_on_) {
        irm_enabled_  = new_irm;
        caos_rom_on_  = new_caos;
        basic_rom_on_ = new_basic;
        apply_banking();
    }
}

// ============================================================================
// TICK
// ============================================================================

template<KC85Variant V>
void KC85System<V>::tick() {
    if (!cpu_) return;

    pins_ = cpu_->tick(pins_);

    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);

    if (mreq) {
        pins_ = bus_.tick(0, pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    ctc_->tick();
    // CTC channel 2 zero-count toggles the foreground blink flag
    if (ctc_->check_zero_count(2)) {
        blink_flag_ = !blink_flag_;
    }
    total_cycles_++;
}

template<KC85Variant V> void KC85System<V>::run_frame() {
    for (uint32_t i = 0; i < kc85_constants::TSTATES_PER_FRAME; ++i) tick();
    render_frame();
}

template<KC85Variant V> bool KC85System<V>::load_file(const char*) { return false; }
template<KC85Variant V> void KC85System<V>::get_display_dimensions(int* w, int* h) const {
    *w = kc85_constants::FB_WIDTH; *h = kc85_constants::FB_HEIGHT;
}
template<KC85Variant V> uint32_t KC85System<V>::get_audio_samples(float*, uint32_t) { return 0; }
template<KC85Variant V> void KC85System<V>::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
template<KC85Variant V> void KC85System<V>::handle_keyboard_event(SDL_Keycode, bool) {}
template<KC85Variant V> void KC85System<V>::render_system_menu_items() {}
template<KC85Variant V> void KC85System<V>::render_configuration_ui() {}
template<KC85Variant V> void KC85System<V>::set_speed_multiplier(float m) { speed_multiplier_ = m; }

// ============================================================================
// VIDEO RENDERING — decode IRM into indexed framebuffer
// ============================================================================
//
// KC85/2,3: IRM = 16 KB at $8000-$BFFF, ZX Spectrum-like interleaved addressing
//   Left 256×256 area (pixel cols 0..31, 8 KB pixel + 2 KB color):
//     pixel_offset = x | (((y>>2)&3)<<5) | ((y&3)<<7) | (((y>>4)&0xF)<<9)
//     color_offset = x | (((y>>2)&0x3F)<<5)
//   Right 64×256 area (pixel cols 32..39, 2 KB pixel + 0.5 KB color):
//     pixel_offset = 0x2000 + ((x&7) | (((y>>4)&3)<<3) | (((y>>2)&3)<<5) | ((y&3)<<7) | (((y>>6)&3)<<9))
//     color_offset = 0x0800 + ((x&7) | (((y>>4)&3)<<3) | (((y>>2)&3)<<5) | (((y>>6)&3)<<7))
//   Color bytes stored at irm[$2800 + color_offset]
//   Each pixel byte = 8 horizontal pixels (MSB = leftmost)
//   Each color byte: bits [2:0] = background (8 colors), bits [5:3] = foreground base, bit 6 = fg intensity, bit 7 = blink
//   Display: 320×256 pixels (256 left + 64 right), full framebuffer width
//
// KC85/4: IRM = 64 KB in 4 × 16 KB banks, column-major layout
//   Bank 0: pixel plane 0, Bank 1: color plane 0
//   Bank 2: pixel plane 1, Bank 3: color plane 1
//   Pixel data: bank_base[col * 256 + row]  (col = 0..39) = 10240 bytes used
//   Color data: same layout in next bank, per-byte color resolution
//   Display: 320×256 pixels, full framebuffer width

template<KC85Variant V>
void KC85System<V>::render_frame() {
    if (!irm_chip_) return;

    const uint8_t* irm = irm_chip_->data();
    // Blink: when PIO-B bit 7 enables blinking AND blink_flag_ is true,
    // color bytes with bit 7 set force foreground to background
    const uint8_t pio_b = pio1_->get_output(1);
    const bool blink_bg = blink_flag_ && (pio_b & 0x80);

    if constexpr (Traits::has_extended_video) {
        // KC85/4: 320×256, dual-plane with per-byte color
        // Banks are: pixel0, color0, pixel1, color1 (each 16 KB)
        const uint8_t* pixel_base = irm + active_plane_ * 2 * 16384;
        const uint8_t* color_base = pixel_base + 16384;

        for (int y = 0; y < kc85_constants::FB_HEIGHT; y++) {
            uint8_t* dst = display_.indices() + y * kc85_constants::FB_WIDTH;
            for (int col = 0; col < kc85_constants::KC4_PIXEL_COLS; col++) {
                uint8_t pixels = pixel_base[col * 256 + y];
                uint8_t color  = color_base[col * 256 + y];
                uint8_t bg = color & 0x07;              // bits 0-2: background (8 colors)
                uint8_t fg = (blink_bg && (color & 0x80)) ? bg : ((color >> 3) & 0x0F);
                int x = col * 8;
                for (int bit = 7; bit >= 0; --bit) {
                    dst[x++] = (pixels & (1 << bit)) ? fg : bg;
                }
            }
        }
    } else {
        // KC85/2,3: 320×256, ZX Spectrum-like interleaved addressing
        // Left 256×256 area (cols 0..31) + right 64×256 area (cols 32..39)
        for (int y = 0; y < kc85_constants::FB_HEIGHT; y++) {
            uint8_t* dst = display_.indices() + y * kc85_constants::FB_WIDTH;

            // Left 256×256 area (columns 0..31)
            for (int x = 0; x < 32; x++) {
                int pixel_offset = x | (((y >> 2) & 0x3) << 5)
                                     | ((y & 0x3) << 7)
                                     | (((y >> 4) & 0xF) << 9);
                int color_offset = x | (((y >> 2) & 0x3F) << 5);
                uint8_t pixels = irm[pixel_offset];
                uint8_t color  = irm[0x2800 + color_offset];
                uint8_t bg = color & 0x07;
                uint8_t fg = (blink_bg && (color & 0x80)) ? bg : ((color >> 3) & 0x0F);
                int px = x * 8;
                for (int bit = 7; bit >= 0; --bit) {
                    dst[px++] = (pixels & (1 << bit)) ? fg : bg;
                }
            }

            // Right 64×256 area (columns 32..39)
            for (int x = 32; x < 40; x++) {
                int pixel_offset = 0x2000 + ((x & 0x7)
                                     | (((y >> 4) & 0x3) << 3)
                                     | (((y >> 2) & 0x3) << 5)
                                     | ((y & 0x3) << 7)
                                     | (((y >> 6) & 0x3) << 9));
                int color_offset = 0x0800 + ((x & 0x7)
                                     | (((y >> 4) & 0x3) << 3)
                                     | (((y >> 2) & 0x3) << 5)
                                     | (((y >> 6) & 0x3) << 7));
                uint8_t pixels = irm[pixel_offset];
                uint8_t color  = irm[0x2800 + color_offset];
                uint8_t bg = color & 0x07;
                uint8_t fg = (blink_bg && (color & 0x80)) ? bg : ((color >> 3) & 0x0F);
                int px = x * 8;
                for (int bit = 7; bit >= 0; --bit) {
                    dst[px++] = (pixels & (1 << bit)) ? fg : bg;
                }
            }
        }
    }

    display_.flush();
}

// ============================================================================
// I/O BUS DISPATCH
// ============================================================================

template<KC85Variant V>
bus_state_t KC85System<V>::io_tick(bus_state_t pins) {
    // Interrupt acknowledge: IORQ + M1
    if (!BUS_GET_BIT(pins, Z80_M1_BIT)) {
        if (ctc_->interrupt_pending()) {
            BUS_SET_DATA(pins, ctc_->interrupt_vector());
        } else {
            BUS_SET_DATA(pins, 0xFF);
        }
        return pins;
    }

    uint8_t port = static_cast<uint8_t>(BUS_GET_ADDR(pins));
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);
    uint8_t data = BUS_GET_DATA(pins);

    // PIO 1 at $88-$8B (system + keyboard)
    if ((port & 0xFC) == kc85_constants::PIO_A_DATA) {
        int port_idx = port & 0x01;
        bool is_ctrl = (port >> 1) & 0x01;
        if (is_read) {
            BUS_SET_DATA(pins, pio1_->read_data(port_idx));
        } else {
            if (is_ctrl) {
                pio1_->write_control(port_idx, data);
            } else {
                pio1_->write_data(port_idx, data);
            }
            // PIO 1 controls memory banking (Port A enables, Port B for KC85/4)
            if (!is_ctrl) {
                update_bank_state();
            }
        }
        return pins;
    }

    // CTC at $8C-$8F (4 channels)
    if ((port & 0xFC) == kc85_constants::CTC_CH0) {
        int channel = port & 0x03;
        if (is_read) {
            BUS_SET_DATA(pins, ctc_->read(channel));
        } else {
            ctc_->write(channel, data);
        }
        return pins;
    }

    // Module system at $80-$81
    if (port == kc85_constants::MODULE_PORT || port == kc85_constants::MODULE_DATA_PORT) {
        if (is_read) {
            BUS_SET_DATA(pins, 0xFF);
        }
        return pins;
    }

    // KC85/4: additional banking control ports
    if constexpr (Traits::has_extended_video) {
        if (port == kc85_constants::KC4_CTRL_PORT && !is_read) {
            uint8_t old_bank = (bank_ctrl_ >> 1) & 0x03;  // io84 bits [2:1]
            bank_ctrl_ = data;
            uint8_t new_bank = (bank_ctrl_ >> 1) & 0x03;

            // Remap IRM bank if changed and IRM is enabled
            if (new_bank != old_bank && irm_enabled_) {
                constexpr size_t kIrmSlot = 1;
                board_.select_bank_at(bus_, 0, kIrmSlot, new_bank, 0x80);
            }
            active_plane_ = bank_ctrl_ & 0x01;
            return pins;
        }
        if (port == kc85_constants::KC4_CTRL2_PORT && !is_read) {
            bank_ctrl2_ = data;
            return pins;
        }
    }

    return pins;
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<KC85Variant V>
bool KC85System<V>::load_roms() {
    char rom_root[512];
    const char* names[] = {"kc85", "KC85", nullptr};
    if (!system_config_discover_rom_root(names, rom_root, sizeof(rom_root))) {
        printf("%s: ROM path not found\n", Traits::name);
        return false;
    }
    return board_.load_roms(rom_root, Traits::name);
}

// ============================================================================
// EXPLICIT INSTANTIATIONS
// ============================================================================

template class KC85System<KC85Variant::KC85_2>;
template class KC85System<KC85Variant::KC85_3>;
template class KC85System<KC85Variant::KC85_4>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(kc85_2_descriptor, [] { return std::make_unique<KC85System<KC85Variant::KC85_2>>(); });
REGISTER_SYSTEM(kc85_3_descriptor, [] { return std::make_unique<KC85System<KC85Variant::KC85_3>>(); });
REGISTER_SYSTEM(kc85_4_descriptor, [] { return std::make_unique<KC85System<KC85Variant::KC85_4>>(); });
