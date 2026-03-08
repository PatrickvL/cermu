/*
 * z1013_system.cpp — Robotron Z1013 system implementation
 */

#include "z1013_system.h"
#include "../../core/system_registry.h"
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
Z1013System<V>::Z1013System() : EmulatedSystem(), pins_(Z1013_BUS_DEFAULT_STATE) {
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
Z1013System<V>::~Z1013System() { delete cpu_; }

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
    cpu_ = new U880();
    pins_ = cpu_->init();
    pio_.init();
    ram_.resize(Traits::ram_size, 0x00);
    monitor_rom_.resize(z1013_constants::MONITOR_ROM_SIZE, 0xFF);
    video_ram_.resize(z1013_constants::VIDEO_RAM_SIZE, 0x00);
    char_rom_.resize(z1013_constants::CHAR_ROM_SIZE, 0xFF);
    if constexpr (Traits::has_basic_rom) {
        basic_rom_.resize(z1013_constants::BASIC_ROM_SIZE, 0xFF);
    }
    load_roms();
    system_ready_ = true;
    return true;
}

template<Z1013Variant V> void Z1013System<V>::shutdown() { delete cpu_; cpu_ = nullptr; system_ready_ = false; }
template<Z1013Variant V> void Z1013System<V>::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    pio_.init();
}

template<Z1013Variant V>
void Z1013System<V>::tick() {
    if (!cpu_) return;
    pins_ = cpu_->tick(pins_);
    // TODO: Memory dispatch, PIO keyboard scanning, video refresh
    total_cycles_++;
}

template<Z1013Variant V> void Z1013System<V>::run_frame() {
    for (uint32_t i = 0; i < z1013_constants::TSTATES_PER_FRAME; ++i) tick();
}

template<Z1013Variant V> bool Z1013System<V>::load_file(const char*) { return false; }
template<Z1013Variant V> uint32_t* Z1013System<V>::get_framebuffer() { return framebuffer_; }
template<Z1013Variant V> void Z1013System<V>::get_display_dimensions(int* w, int* h) const {
    *w = z1013_constants::FB_WIDTH; *h = z1013_constants::FB_HEIGHT;
}
template<Z1013Variant V> void Z1013System<V>::set_framebuffer(uint32_t*, int, int) {}
template<Z1013Variant V> uint32_t Z1013System<V>::get_audio_samples(float*, uint32_t) { return 0; }
template<Z1013Variant V> void Z1013System<V>::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
template<Z1013Variant V> void Z1013System<V>::handle_keyboard_event(SDL_Keycode, bool) {}
template<Z1013Variant V> void Z1013System<V>::render_system_menu_items() {}
template<Z1013Variant V> void Z1013System<V>::render_configuration_ui() {}
template<Z1013Variant V> void Z1013System<V>::set_speed_multiplier(float m) { speed_multiplier_ = m; }

template<Z1013Variant V> bus_state_t Z1013System<V>::mem_tick(bus_state_t pins) { return pins; }
template<Z1013Variant V> bus_state_t Z1013System<V>::io_tick(bus_state_t pins) { return pins; }
template<Z1013Variant V> bool Z1013System<V>::load_roms() { return false; }

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
