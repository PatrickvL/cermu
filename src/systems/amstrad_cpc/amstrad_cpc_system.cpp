/*
 * amstrad_cpc_system.cpp — Amstrad CPC 464/664/6128 system implementation
 *
 * Tick loop:
 *   Z80A @ 4 MHz.  Gate Array generates interrupts every 52 HSYNCs.
 *   MC6845 drives display timing; Gate Array translates CRTC addresses to
 *   screen memory + mode/color decoding.  AY-3-8912 driven via PPI port.
 */

#include "amstrad_cpc_system.h"
#include "../../core/system_registry.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// HARDWARE TRAITS
// ============================================================================

template<CPCModel M>
static HardwareTraits create_cpc_hardware_traits() {
    using Traits = CPCModelTraits<M>;
    HardwareTraits traits = {};

    traits.display.native_width    = amstrad_cpc_constants::FB_WIDTH;
    traits.display.native_height   = amstrad_cpc_constants::FB_HEIGHT;
    traits.display.visible_width   = amstrad_cpc_constants::FB_WIDTH;
    traits.display.visible_height  = amstrad_cpc_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = amstrad_cpc_constants::GA_COLOR_COUNT;
    traits.display.has_overscan    = false;

    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = amstrad_cpc_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "AY-3-8912";

    traits.timing.cpu_frequency_hz   = amstrad_cpc_constants::CPU_FREQ_HZ;
    traits.timing.video_frequency_hz = amstrad_cpc_constants::CPU_FREQ_HZ;
    traits.timing.audio_sample_rate_hz = amstrad_cpc_constants::DEFAULT_SAMPLE_RATE;
    traits.timing.target_fps         = 50;
    traits.timing.cycles_per_frame   = amstrad_cpc_constants::TSTATES_PER_FRAME;
    traits.timing.standard           = VideoStandard::PAL;

    return traits;
}

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor cpc464_descriptor = {
    "Amstrad CPC 464", "CPC464",
    "Amstrad CPC 464 — Z80A @ 4MHz, 64KB RAM, integrated tape (1984)",
    "amstrad_cpc", {"CPC464", "CPC", "AmstradCPC"},
    nullptr, create_cpc_hardware_traits<CPCModel::CPC464>(), nullptr
};

static SystemDescriptor cpc664_descriptor = {
    "Amstrad CPC 664", "CPC664",
    "Amstrad CPC 664 — Z80A @ 4MHz, 64KB RAM, 3\" floppy (1985)",
    "amstrad_cpc", {"CPC664"},
    nullptr, create_cpc_hardware_traits<CPCModel::CPC664>(), nullptr
};

static SystemDescriptor cpc6128_descriptor = {
    "Amstrad CPC 6128", "CPC6128",
    "Amstrad CPC 6128 — Z80A @ 4MHz, 128KB RAM, 3\" floppy (1985)",
    "amstrad_cpc", {"CPC6128"},
    nullptr, create_cpc_hardware_traits<CPCModel::CPC6128>(), nullptr
};

// ============================================================================
// IMPLEMENTATION (stub — follows Spectrum pattern)
// ============================================================================

template<CPCModel M>
AmstradCPCSystem<M>::AmstradCPCSystem()
    : EmulatedSystem()
    , ay_(AYVariant::AY_3_8912)
    , pins_(CPC_BUS_DEFAULT_STATE)
{
    hardware_traits_ = create_cpc_hardware_traits<M>();
}

template<CPCModel M>
AmstradCPCSystem<M>::~AmstradCPCSystem() { delete cpu_; }

template<CPCModel M>
const SystemDescriptor& AmstradCPCSystem<M>::get_descriptor() const {
    if constexpr (M == CPCModel::CPC464) return cpc464_descriptor;
    else if constexpr (M == CPCModel::CPC664) return cpc664_descriptor;
    else return cpc6128_descriptor;
}

template<CPCModel M>
bool AmstradCPCSystem<M>::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
template<CPCModel M>
bool AmstradCPCSystem<M>::apply_configuration() { return true; }

template<CPCModel M>
bool AmstradCPCSystem<M>::initialize() {
    printf("%s: Initializing system\n", Traits::name);
    cpu_ = new ZilogZ80A();
    pins_ = cpu_->init();
    crtc_.init();
    ppi_.init();
    ay_.init();
    gate_array_.reset();
    ram_.resize(Traits::ram_size_kb * 1024, 0x00);
    lower_rom_.resize(amstrad_cpc_constants::ROM_SIZE, 0xFF);
    upper_rom_.resize(amstrad_cpc_constants::ROM_SIZE, 0xFF);
    load_roms();
    system_ready_ = true;
    return true;
}

template<CPCModel M> void AmstradCPCSystem<M>::shutdown() { delete cpu_; cpu_ = nullptr; system_ready_ = false; }
template<CPCModel M> void AmstradCPCSystem<M>::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    crtc_.init();
    ppi_.init();
    ay_.reset();
    gate_array_.reset();
}

template<CPCModel M>
void AmstradCPCSystem<M>::tick() {
    if (!cpu_) return;
    pins_ = cpu_->tick(pins_);
    // TODO: Bus dispatch, gate array interrupt, CRTC timing, AY clocking
    total_cycles_++;
}

template<CPCModel M> void AmstradCPCSystem<M>::run_frame() {
    for (uint32_t i = 0; i < amstrad_cpc_constants::TSTATES_PER_FRAME; ++i) tick();
}

template<CPCModel M> bool AmstradCPCSystem<M>::load_file(const char*) { return false; }
template<CPCModel M> uint32_t* AmstradCPCSystem<M>::get_framebuffer() { return framebuffer_; }
template<CPCModel M> void AmstradCPCSystem<M>::get_display_dimensions(int* w, int* h) const {
    *w = amstrad_cpc_constants::FB_WIDTH; *h = amstrad_cpc_constants::FB_HEIGHT;
}
template<CPCModel M> void AmstradCPCSystem<M>::set_framebuffer(uint32_t*, int, int) {}
template<CPCModel M> uint32_t AmstradCPCSystem<M>::get_audio_samples(float*, uint32_t) { return 0; }
template<CPCModel M> void AmstradCPCSystem<M>::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
template<CPCModel M> void AmstradCPCSystem<M>::handle_keyboard_event(SDL_Keycode, bool) {}
template<CPCModel M> void AmstradCPCSystem<M>::render_system_menu_items() {}
template<CPCModel M> void AmstradCPCSystem<M>::render_configuration_ui() {}
template<CPCModel M> void AmstradCPCSystem<M>::set_speed_multiplier(float m) { speed_multiplier_ = m; }

template<CPCModel M> bus_state_t AmstradCPCSystem<M>::mem_tick(bus_state_t pins) { return pins; }
template<CPCModel M> bus_state_t AmstradCPCSystem<M>::io_tick(bus_state_t pins) { return pins; }
template<CPCModel M> bool AmstradCPCSystem<M>::load_roms() { return false; }

// ============================================================================
// EXPLICIT INSTANTIATIONS
// ============================================================================

template class AmstradCPCSystem<CPCModel::CPC464>;
template class AmstradCPCSystem<CPCModel::CPC664>;
template class AmstradCPCSystem<CPCModel::CPC6128>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(cpc464_descriptor, [] { return std::make_unique<AmstradCPCSystem<CPCModel::CPC464>>(); });
REGISTER_SYSTEM(cpc664_descriptor, [] { return std::make_unique<AmstradCPCSystem<CPCModel::CPC664>>(); });
REGISTER_SYSTEM(cpc6128_descriptor, [] { return std::make_unique<AmstradCPCSystem<CPCModel::CPC6128>>(); });
