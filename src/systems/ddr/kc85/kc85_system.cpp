/*
 * kc85_system.cpp — KC 85/2, /3, /4 system implementation
 */

#include "kc85_system.h"
#include "../../../core/system_registry.h"
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
KC85System<V>::KC85System() : EmulatedSystem(), pins_(KC85_BUS_DEFAULT_STATE) {
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
    cpu_ = new U880();
    pins_ = cpu_->init();
    pio1_.init();
    pio2_.init();
    ctc_.init();
    modules_.init();

    ram_.resize(Traits::ram_size, 0x00);
    os_rom_.resize(kc85_constants::OS_ROM_SIZE, 0xFF);
    pixel_ram_.resize(kc85_constants::PIXEL_RAM_SIZE, 0x00);
    color_ram_.resize(kc85_constants::PIXEL_RAM_SIZE, 0x07);  // Default: white-on-black

    if constexpr (Traits::has_basic_rom) {
        basic_rom_.resize(kc85_constants::BASIC_ROM_SIZE, 0xFF);
    }
    if constexpr (Traits::has_extended_video) {
        // KC85/4: second screen plane
        pixel_ram_2_.resize(kc85_constants::KC4_PIXEL_RAM_SIZE, 0x00);
        color_ram_2_.resize(kc85_constants::KC4_COLOR_RAM_SIZE, 0x07);
    }

    load_roms();
    caos_rom_on_ = true;
    irm_enabled_ = true;
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
}

template<KC85Variant V>
void KC85System<V>::tick() {
    if (!cpu_) return;

    // CPU tick
    pins_ = cpu_->tick(pins_);

    // Bus dispatch
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);  // Active-low
    bool iorq = !BUS_GET_BIT(pins_, Z80_IORQ_BIT);  // Active-low

    if (mreq) {
        pins_ = mem_tick(pins_);
    } else if (iorq) {
        pins_ = io_tick(pins_);
    }

    // CTC tick (drives timing and sound)
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

template<KC85Variant V>
bus_state_t KC85System<V>::mem_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    if (is_read) {
        uint8_t data = 0xFF;
        if (addr < 0x4000) {
            // Base RAM (always present)
            if (addr < ram_.size()) data = ram_[addr];
        } else if (addr < 0x8000) {
            // KC85/4: extended RAM; KC85/2+3: expansion modules (open bus)
            if constexpr (Traits::has_extended_video) {
                if (addr < ram_.size()) data = ram_[addr];
            }
        } else if (addr < 0xC000) {
            // IRM (video RAM) — accessible only when enabled
            if (irm_enabled_) {
                uint16_t offset = addr - 0x8000;
                if constexpr (Traits::has_extended_video) {
                    // KC85/4: bank_ctrl_ selects plane and pixel/color
                    bool is_color = bank_ctrl_ & 0x02;
                    bool plane1   = bank_ctrl_ & 0x01;
                    if (is_color) {
                        data = plane1 ? color_ram_2_[offset] : color_ram_[offset];
                    } else {
                        data = plane1 ? pixel_ram_2_[offset] : pixel_ram_[offset];
                    }
                } else {
                    // KC85/2+3: single interleaved plane
                    data = pixel_ram_[offset];
                }
            }
        } else if (addr < 0xE000) {
            // BASIC ROM (KC85/3, /4) or open bus
            if constexpr (Traits::has_basic_rom) {
                if (basic_rom_on_ && static_cast<size_t>(addr - 0xC000) < basic_rom_.size())
                    data = basic_rom_[addr - 0xC000];
            }
        } else {
            // CAOS (OS) ROM
            if (caos_rom_on_ && static_cast<size_t>(addr - 0xE000) < os_rom_.size())
                data = os_rom_[addr - 0xE000];
        }
        BUS_SET_DATA(pins, data);
    } else {
        uint8_t data = BUS_GET_DATA(pins);
        if (addr < 0x4000) {
            if (addr < ram_.size()) ram_[addr] = data;
        } else if (addr < 0x8000) {
            if constexpr (Traits::has_extended_video) {
                if (addr < ram_.size()) ram_[addr] = data;
            }
        } else if (addr < 0xC000) {
            // IRM write
            if (irm_enabled_) {
                uint16_t offset = addr - 0x8000;
                if constexpr (Traits::has_extended_video) {
                    bool is_color = bank_ctrl_ & 0x02;
                    bool plane1   = bank_ctrl_ & 0x01;
                    if (is_color) {
                        (plane1 ? color_ram_2_ : color_ram_)[offset] = data;
                    } else {
                        (plane1 ? pixel_ram_2_ : pixel_ram_)[offset] = data;
                    }
                } else {
                    pixel_ram_[offset] = data;
                }
            }
        }
        // ROM regions ($C000-$FFFF) are read-only
    }

    return pins;
}
template<KC85Variant V>
bus_state_t KC85System<V>::io_tick(bus_state_t pins) {
    // Interrupt acknowledge: IORQ + M1
    if (!BUS_GET_BIT(pins, Z80_M1_BIT)) {
        // CTC provides the interrupt vector in daisy chain
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
    // Bit 0: port (0=A, 1=B), Bit 1: data/control (0=data, 1=control)
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
            bank_ctrl_ = data;
            update_bank_state();
            return pins;
        }
        if (port == kc85_constants::KC4_CTRL2_PORT && !is_read) {
            bank_ctrl2_ = data;
            update_bank_state();
            return pins;
        }
    }

    return pins;
}
template<KC85Variant V>
void KC85System<V>::update_bank_state() {
    uint8_t pio_b = pio1_.get_output(1);
    caos_rom_on_  = pio_b & 0x01;    // Bit 0: CAOS ROM enable
    irm_enabled_  = pio_b & 0x04;    // Bit 2: IRM (video RAM) enable
    basic_rom_on_ = pio_b & 0x40;    // Bit 6: BASIC ROM enable

    if constexpr (Traits::has_extended_video) {
        // KC85/4: bank_ctrl_ selects video plane and type
        active_plane_ = bank_ctrl_ & 0x01;
    }
}
template<KC85Variant V> bool KC85System<V>::load_roms() { return false; }

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
