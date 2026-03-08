/*
 * z9001_system.cpp — Robotron Z9001 / KC 87 system implementation
 */

#include "z9001_system.h"
#include "../../core/system_registry.h"
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
Z9001System<V>::Z9001System() : EmulatedSystem(), pins_(Z9001_BUS_DEFAULT_STATE) {
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
    cpu_ = new U880();
    pins_ = cpu_->init();
    pio1_.init();
    pio2_.init();
    ctc_.init();
    ram_.resize(Traits::ram_size, 0x00);
    os_rom_.resize(z9001_constants::OS_ROM_SIZE, 0xFF);
    video_ram_.resize(z9001_constants::VIDEO_RAM_SIZE, 0x00);
    char_rom_.resize(z9001_constants::CHAR_ROM_SIZE, 0xFF);
    if constexpr (Traits::has_color_ram) {
        color_ram_.resize(z9001_constants::COLOR_RAM_SIZE, 0x07);  // White-on-black default
    }
    if constexpr (Traits::has_basic_rom) {
        basic_rom_.resize(z9001_constants::BASIC_ROM_SIZE, 0xFF);
    }
    load_roms();
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
}

template<Z9001Variant V>
void Z9001System<V>::tick() {
    if (!cpu_) return;
    pins_ = cpu_->tick(pins_);
    // TODO: Bus dispatch, PIO keyboard scan, CTC timing, text rendering
    total_cycles_++;
}

template<Z9001Variant V> void Z9001System<V>::run_frame() {
    for (uint32_t i = 0; i < z9001_constants::TSTATES_PER_FRAME; ++i) tick();
}

template<Z9001Variant V> bool Z9001System<V>::load_file(const char*) { return false; }
template<Z9001Variant V> uint32_t* Z9001System<V>::get_framebuffer() { return framebuffer_; }
template<Z9001Variant V> void Z9001System<V>::get_display_dimensions(int* w, int* h) const {
    *w = z9001_constants::FB_WIDTH; *h = z9001_constants::FB_HEIGHT;
}
template<Z9001Variant V> void Z9001System<V>::set_framebuffer(uint32_t*, int, int) {}
template<Z9001Variant V> uint32_t Z9001System<V>::get_audio_samples(float*, uint32_t) { return 0; }
template<Z9001Variant V> void Z9001System<V>::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
template<Z9001Variant V> void Z9001System<V>::handle_keyboard_event(SDL_Keycode, bool) {}
template<Z9001Variant V> void Z9001System<V>::render_system_menu_items() {}
template<Z9001Variant V> void Z9001System<V>::render_configuration_ui() {}
template<Z9001Variant V> void Z9001System<V>::set_speed_multiplier(float m) { speed_multiplier_ = m; }

template<Z9001Variant V> bus_state_t Z9001System<V>::mem_tick(bus_state_t pins) { return pins; }
template<Z9001Variant V> bus_state_t Z9001System<V>::io_tick(bus_state_t pins) { return pins; }
template<Z9001Variant V> bool Z9001System<V>::load_roms() { return false; }

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
