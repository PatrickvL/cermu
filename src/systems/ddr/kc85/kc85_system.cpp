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
KC85System<V>::~KC85System() { delete cpu_; }

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

    // ── Create chips via factory, wire the bus ────────────────────────
    bus_mem_.create_chips(&pins_);
    bus_mem_.apply(bus_);

    // Retrieve typed pointers for chips accessed after initialize()
    caos_rom_chip_ = bus_mem_.template chip_as<ROMChip>(Traits::kCaosRomSlot);
    if constexpr (Traits::has_basic_rom) {
        basic_rom_chip_ = bus_mem_.template chip_as<ROMChip>(Traits::kBasicRomSlot);
    }

    // ── Set initial banking state ───────────────────────────────────────
    caos_rom_on_  = true;
    irm_enabled_  = true;
    basic_rom_on_ = false;
    bank_ctrl_    = 0;
    bank_ctrl2_   = 0;
    active_plane_ = 0;
    configure_bus_memory_map();

    // ── Init chips ──────────────────────────────────────────────────────
    cpu_ = new U880();
    pins_ = cpu_->init();
    pio1_.init();
    pio2_.init();
    ctc_.init();
    modules_.init();

    std::memset(keyboard_matrix_, 0xFF, sizeof(keyboard_matrix_));

    load_roms();

    // ── Register chips for Hardware menu ────────────────────────────────
    register_chip(static_cast<ChipBase*>(cpu_),
        "U880 CPU", "U880", "CPU", 0x0000);
    register_chip(&pio1_,
        "U855 PIO #1", "U855", "I/O", kc85_constants::PIO_A_DATA);
    register_chip(&pio2_,
        "U855 PIO #2", "U855", "I/O", 0x00);
    register_chip(&ctc_,
        "U857 CTC", "U857", "I/O", kc85_constants::CTC_CH0);
    register_bus_chips(bus_mem_);

    printf("%s: System initialized (RAM: %d KB, IRM: %d KB)\n",
           Traits::name, Traits::ram_size / 1024,
           Traits::has_extended_video ? 64 : 16);
    system_ready_ = true;
    return true;
}

template<KC85Variant V> void KC85System<V>::shutdown() { delete cpu_; cpu_ = nullptr; system_ready_ = false; }
template<KC85Variant V> void KC85System<V>::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    pio1_.init();
    pio2_.init();
    ctc_.init();
    modules_.init();
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
    using ChipId      = typename PT::ChipId;
    using WriteChipId = typename PT::WriteChipId;

    // apply() maps all slots per the manifest.  We adjust for initial banking.
    bus_mem_.apply(bus_);

    // ── KC85/4: Trim IRM write pages that spill into ROM area ───────────
    // The 64 KB IRM at $8000 maps write pages $80-$FF (128 pages, clipped).
    // Pages $C0-$FF overlap with BASIC/CAOS ROM — unmap those writes.
    if constexpr (Traits::has_extended_video) {
        for (size_t page = 0xC0; page < 0x100; ++page)
            bus_.set_write_page(0, page, PT::kNoChipSelectedWrite);
    }

    // ── BASIC ROM: initially disabled — unmap read pages $C0-$DF ────────
    if constexpr (Traits::has_basic_rom) {
        if (!basic_rom_on_) {
            constexpr size_t kBasicSlot = (V == KC85Variant::KC85_2) ? 0 : 2;
            (void)kBasicSlot;
            for (size_t page = 0xC0; page < 0xE0; ++page)
                bus_.set_read_page(0, page, PT::kNoChipSelected);
        }
    }

    // ── CAOS ROM: apply current enable state ────────────────────────────
    if (!caos_rom_on_) {
        for (size_t page = 0xE0; page < 0x100; ++page)
            bus_.set_read_page(0, page, PT::kNoChipSelected);
    }

    // ── IRM: apply current enable state ─────────────────────────────────
    if (irm_enabled_) {
        if constexpr (Traits::has_extended_video) {
            // KC85/4: Map the currently selected IRM bank to $80-$BF
            constexpr size_t kIrmSlot = 1;
            constexpr size_t kIrmBaseId = BT::kManifest.base_id(kIrmSlot, BT::Spec::PageBits);
            constexpr size_t kPagesPerBank = 64;  // 16 KB / 256 = 64 pages

            // Bank layout: pixel0=0, pixel1=1, color0=2, color1=3
            // bank_ctrl_ bits: bit 0 = plane, bit 1 = pixel/color
            uint8_t bank = bank_ctrl_ & 0x03;
            size_t bank_offset = bank * kPagesPerBank;
            bus_.fill_pages(0, 0x80, kPagesPerBank,
                ChipId(kIrmBaseId + bank_offset),
                WriteChipId(kIrmBaseId + bank_offset));
        }
        // KC85/2-3: apply() already mapped IRM pages $80-$BF correctly
    } else {
        // IRM disabled — unmap $80-$BF
        for (size_t page = 0x80; page < 0xC0; ++page) {
            bus_.set_read_page(0, page, PT::kNoChipSelected);
            bus_.set_write_page(0, page, PT::kNoChipSelectedWrite);
        }
    }
}

// ============================================================================
// DYNAMIC BANKING
// ============================================================================

template<KC85Variant V>
void KC85System<V>::update_bank_state() {
    using ChipId      = typename PT::ChipId;
    using WriteChipId = typename PT::WriteChipId;

    uint8_t pio_b = pio1_.get_output(1);

    bool new_caos = (pio_b & 0x01) != 0;
    bool new_irm  = (pio_b & 0x04) != 0;
    bool new_basic = (pio_b & 0x40) != 0;

    // ── IRM at $8000-$BFFF ──────────────────────────────────────────────
    if (new_irm != irm_enabled_) {
        irm_enabled_ = new_irm;
        if (irm_enabled_) {
            if constexpr (Traits::has_extended_video) {
                // KC85/4: Map selected bank
                constexpr size_t kIrmSlot = 1;
                constexpr size_t kIrmBaseId = BT::kManifest.base_id(kIrmSlot, BT::Spec::PageBits);
                constexpr size_t kPagesPerBank = 64;
                uint8_t bank = bank_ctrl_ & 0x03;
                bus_.fill_pages(0, 0x80, kPagesPerBank,
                    ChipId(kIrmBaseId + bank * kPagesPerBank),
                    WriteChipId(kIrmBaseId + bank * kPagesPerBank));
            } else {
                // KC85/2-3: Map single IRM plane
                constexpr size_t kIrmSlot = 1;
                constexpr size_t kIrmBaseId = BT::kManifest.base_id(kIrmSlot, BT::Spec::PageBits);
                bus_.fill_pages(0, 0x80, 64,
                    ChipId(kIrmBaseId), WriteChipId(kIrmBaseId));
            }
        } else {
            for (size_t p = 0x80; p < 0xC0; ++p) {
                bus_.set_read_page(0, p, PT::kNoChipSelected);
                bus_.set_write_page(0, p, PT::kNoChipSelectedWrite);
            }
        }
    }

    // ── CAOS ROM at $E000-$FFFF ─────────────────────────────────────────
    if (new_caos != caos_rom_on_) {
        caos_rom_on_ = new_caos;
        constexpr size_t kCaosSlot = (V == KC85Variant::KC85_2) ? 2 : 3;
        constexpr size_t kCaosBaseId = BT::kManifest.base_id(kCaosSlot, BT::Spec::PageBits);
        if (caos_rom_on_) {
            bus_.fill_read_pages(0, 0xE0, 32, ChipId(kCaosBaseId));
        } else {
            for (size_t p = 0xE0; p < 0x100; ++p)
                bus_.set_read_page(0, p, PT::kNoChipSelected);
        }
    }

    // ── BASIC ROM at $C000-$DFFF ────────────────────────────────────────
    if constexpr (Traits::has_basic_rom) {
        if (new_basic != basic_rom_on_) {
            basic_rom_on_ = new_basic;
            constexpr size_t kBasicSlot = 2;
            constexpr size_t kBasicBaseId = BT::kManifest.base_id(kBasicSlot, BT::Spec::PageBits);
            if (basic_rom_on_) {
                bus_.fill_read_pages(0, 0xC0, 32, ChipId(kBasicBaseId));
            } else {
                for (size_t p = 0xC0; p < 0xE0; ++p)
                    bus_.set_read_page(0, p, PT::kNoChipSelected);
            }
        }
    }

    if constexpr (Traits::has_extended_video) {
        active_plane_ = bank_ctrl_ & 0x01;
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

    ctc_.tick();
    total_cycles_++;
}

template<KC85Variant V> void KC85System<V>::run_frame() {
    for (uint32_t i = 0; i < kc85_constants::TSTATES_PER_FRAME; ++i) tick();
}

template<KC85Variant V> bool KC85System<V>::load_file(const char*) { return false; }
template<KC85Variant V> uint32_t* KC85System<V>::get_framebuffer() { return framebuffer_; }
template<KC85Variant V> void KC85System<V>::get_display_dimensions(int* w, int* h) const {
    *w = kc85_constants::FB_WIDTH; *h = kc85_constants::FB_HEIGHT;
}
template<KC85Variant V> void KC85System<V>::set_framebuffer(uint32_t*, int, int) {}
template<KC85Variant V> uint32_t KC85System<V>::get_audio_samples(float*, uint32_t) { return 0; }
template<KC85Variant V> void KC85System<V>::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
template<KC85Variant V> void KC85System<V>::handle_keyboard_event(SDL_Keycode, bool) {}
template<KC85Variant V> void KC85System<V>::render_system_menu_items() {}
template<KC85Variant V> void KC85System<V>::render_configuration_ui() {}
template<KC85Variant V> void KC85System<V>::set_speed_multiplier(float m) { speed_multiplier_ = m; }

// ============================================================================
// I/O BUS DISPATCH
// ============================================================================

template<KC85Variant V>
bus_state_t KC85System<V>::io_tick(bus_state_t pins) {
    // Interrupt acknowledge: IORQ + M1
    if (!BUS_GET_BIT(pins, Z80_M1_BIT)) {
        if (ctc_.interrupt_pending()) {
            BUS_SET_DATA(pins, ctc_.interrupt_vector());
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
            BUS_SET_DATA(pins, pio1_.read_data(port_idx));
        } else {
            if (is_ctrl) {
                pio1_.write_control(port_idx, data);
            } else {
                pio1_.write_data(port_idx, data);
            }
            // PIO 1 Port B controls memory banking
            if (!is_ctrl && port_idx == 1) {
                update_bank_state();
            }
        }
        return pins;
    }

    // CTC at $8C-$8F (4 channels)
    if ((port & 0xFC) == kc85_constants::CTC_CH0) {
        int channel = port & 0x03;
        if (is_read) {
            BUS_SET_DATA(pins, ctc_.read(channel));
        } else {
            ctc_.write(channel, data);
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
            using ChipId      = typename PT::ChipId;
            using WriteChipId = typename PT::WriteChipId;

            uint8_t old_bank = bank_ctrl_ & 0x03;
            bank_ctrl_ = data;
            uint8_t new_bank = bank_ctrl_ & 0x03;

            // Remap IRM bank if changed and IRM is enabled
            if (new_bank != old_bank && irm_enabled_) {
                constexpr size_t kIrmSlot = 1;
                constexpr size_t kIrmBaseId = BT::kManifest.base_id(kIrmSlot, BT::Spec::PageBits);
                constexpr size_t kPagesPerBank = 64;
                bus_.fill_pages(0, 0x80, kPagesPerBank,
                    ChipId(kIrmBaseId + new_bank * kPagesPerBank),
                    WriteChipId(kIrmBaseId + new_bank * kPagesPerBank));
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

    bool ok = true;

    // CAOS (OS) ROM (8 KB at $E000)
    const char* caos_names[] = {"caos.rom", "CAOS.ROM", nullptr};
    if (!rom_loader_load_from_root(rom_root, caos_names,
                                   kc85_constants::OS_ROM_SIZE,
                                   caos_rom_chip_->data(), caos_rom_chip_->size_bytes())) {
        printf("%s: CAOS ROM not loaded\n", Traits::name);
        ok = false;
    }

    // BASIC ROM (8 KB, KC85/3 and /4 only)
    if constexpr (Traits::has_basic_rom) {
        const char* basic_names[] = {"basic.rom", "BASIC.ROM", nullptr};
        if (!rom_loader_load_from_root(rom_root, basic_names,
                                       kc85_constants::BASIC_ROM_SIZE,
                                       basic_rom_chip_->data(), basic_rom_chip_->size_bytes())) {
            printf("%s: BASIC ROM not loaded\n", Traits::name);
            ok = false;
        }
    }

    return ok;
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
