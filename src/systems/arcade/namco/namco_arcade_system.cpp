/*
 * namco_arcade_system.cpp — Namco Pac-Man / Pengo arcade system implementation
 */

#include "namco_arcade_system.h"
#include "../../../core/system_registry.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor pacman_descriptor = {
    "Pac-Man", "PacMan",
    "Namco Pac-Man — Z80A @ 3.072MHz, WSG3 sound, 224×288 (1980)",
    "pacman", {"PacMan", "Pac-Man", "Puckman"},
    nullptr, {}, nullptr
};

static SystemDescriptor pengo_descriptor = {
    "Pengo", "Pengo",
    "Sega/Coreland Pengo — Z80A @ 3.072MHz, WSG3 sound, 224×288 (1982)",
    "pengo", {"Pengo"},
    nullptr, {}, nullptr
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

template<NamcoGame G>
NamcoArcadeSystem<G>::NamcoArcadeSystem()
    : EmulatedSystem()
    , wsg_(WSGVariant::WSG3)
    , pins_(NAMCO_BUS_DEFAULT_STATE)
{
    HardwareTraits traits = {};
    traits.display.native_width    = namco_arcade_constants::FB_WIDTH;
    traits.display.native_height   = namco_arcade_constants::FB_HEIGHT;
    traits.display.visible_width   = namco_arcade_constants::FB_WIDTH;
    traits.display.visible_height  = namco_arcade_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = namco_arcade_constants::PALETTE_ENTRIES;
    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = namco_arcade_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "Namco WSG3";
    traits.timing.cpu_frequency_hz = namco_arcade_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = namco_arcade_constants::REFRESH_HZ;
    traits.timing.cycles_per_frame = namco_arcade_constants::TSTATES_PER_FRAME;
    traits.timing.standard         = VideoStandard::NTSC;
    hardware_traits_ = traits;
}

template<NamcoGame G>
NamcoArcadeSystem<G>::~NamcoArcadeSystem() { delete cpu_; }

template<NamcoGame G>
const SystemDescriptor& NamcoArcadeSystem<G>::get_descriptor() const {
    if constexpr (G == NamcoGame::PacMan) return pacman_descriptor;
    else return pengo_descriptor;
}

template<NamcoGame G> bool NamcoArcadeSystem<G>::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
template<NamcoGame G> bool NamcoArcadeSystem<G>::apply_configuration() { return true; }

template<NamcoGame G>
bool NamcoArcadeSystem<G>::initialize() {
    printf("%s: Initializing arcade system\n", Traits::name);
    cpu_ = new ZilogZ80A();
    pins_ = cpu_->init();
    wsg_.init();

    rom_.resize(Traits::rom_size, 0xFF);
    ram_.resize(namco_arcade_constants::RAM_SIZE, 0x00);
    video_ram_.resize(namco_arcade_constants::VIDEO_RAM_SIZE, 0x00);
    color_ram_.resize(namco_arcade_constants::COLOR_RAM_SIZE, 0x00);
    char_rom_.resize(Traits::char_rom_size, 0xFF);
    sprite_rom_.resize(namco_arcade_constants::SPRITE_ROM_SIZE, 0xFF);
    palette_prom_.resize(namco_arcade_constants::PALETTE_PROM_SIZE, 0x00);
    colortable_prom_.resize(namco_arcade_constants::COLORTABLE_PROM_SIZE, 0x00);
    waveform_rom_.resize(namco_arcade_constants::WAVEFORM_ROM_SIZE, 0x00);
    load_roms();
    system_ready_ = true;
    return true;
}

template<NamcoGame G> void NamcoArcadeSystem<G>::shutdown() { delete cpu_; cpu_ = nullptr; system_ready_ = false; }
template<NamcoGame G> void NamcoArcadeSystem<G>::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    wsg_.reset();
    int_enable_ = false;
    sound_enable_ = false;
    scanline_ = 0;
}

template<NamcoGame G>
void NamcoArcadeSystem<G>::tick() {
    if (!cpu_) return;
    pins_ = cpu_->tick(pins_);
    // TODO: Memory dispatch, VBLANK IRQ at scanline 224, WSG sound tick
    total_cycles_++;
}

template<NamcoGame G> void NamcoArcadeSystem<G>::run_frame() {
    for (uint32_t i = 0; i < namco_arcade_constants::TSTATES_PER_FRAME; ++i) tick();
}

template<NamcoGame G> bool NamcoArcadeSystem<G>::load_file(const char*) { return false; }
template<NamcoGame G> uint32_t* NamcoArcadeSystem<G>::get_framebuffer() { return framebuffer_; }
template<NamcoGame G> void NamcoArcadeSystem<G>::get_display_dimensions(int* w, int* h) const {
    *w = namco_arcade_constants::FB_WIDTH; *h = namco_arcade_constants::FB_HEIGHT;
}
template<NamcoGame G> void NamcoArcadeSystem<G>::set_framebuffer(uint32_t*, int, int) {}
template<NamcoGame G> uint32_t NamcoArcadeSystem<G>::get_audio_samples(float*, uint32_t) { return 0; }
template<NamcoGame G> void NamcoArcadeSystem<G>::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
template<NamcoGame G> void NamcoArcadeSystem<G>::handle_keyboard_event(SDL_Keycode, bool) {}
template<NamcoGame G> void NamcoArcadeSystem<G>::render_system_menu_items() {}
template<NamcoGame G> void NamcoArcadeSystem<G>::render_configuration_ui() {}
template<NamcoGame G> void NamcoArcadeSystem<G>::set_speed_multiplier(float m) { speed_multiplier_ = m; }

template<NamcoGame G> bus_state_t NamcoArcadeSystem<G>::mem_tick(bus_state_t pins) { return pins; }
template<NamcoGame G> bool NamcoArcadeSystem<G>::load_roms() { return false; }

// ============================================================================
// EXPLICIT INSTANTIATIONS
// ============================================================================

template class NamcoArcadeSystem<NamcoGame::PacMan>;
template class NamcoArcadeSystem<NamcoGame::Pengo>;

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(pacman_descriptor, [] { return std::make_unique<NamcoArcadeSystem<NamcoGame::PacMan>>(); });
REGISTER_SYSTEM(pengo_descriptor,  [] { return std::make_unique<NamcoArcadeSystem<NamcoGame::Pengo>>(); });
