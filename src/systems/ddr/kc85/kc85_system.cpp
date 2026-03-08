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
    pins_ = cpu_->tick(pins_);
    // TODO: Bus dispatch, PIO system control, CTC timing/sound, video rendering
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

template<KC85Variant V> bus_state_t KC85System<V>::mem_tick(bus_state_t pins) { return pins; }
template<KC85Variant V> bus_state_t KC85System<V>::io_tick(bus_state_t pins) { return pins; }
template<KC85Variant V> void KC85System<V>::update_bank_state() {}
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
