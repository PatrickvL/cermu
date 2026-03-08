/*
 * bombjack_system.cpp — Bomb Jack arcade system implementation
 */

#include "bombjack_system.h"
#include "../../../core/system_registry.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor bombjack_descriptor = {
    "Bomb Jack", "BombJack",
    "Tehkan Bomb Jack — dual Z80A, 3× AY-3-8910, 256×224 sprites+tiles (1984)",
    "bombjack", {"BombJack", "Bomb Jack"},
    nullptr, {}, nullptr
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

BombJackSystem::BombJackSystem()
    : EmulatedSystem()
    , main_pins_(BOMBJACK_BUS_DEFAULT_STATE)
    , sound_pins_(BOMBJACK_BUS_DEFAULT_STATE)
{
    HardwareTraits traits = {};
    traits.display.native_width    = bombjack_constants::FB_WIDTH;
    traits.display.native_height   = bombjack_constants::FB_HEIGHT;
    traits.display.visible_width   = bombjack_constants::FB_WIDTH;
    traits.display.visible_height  = bombjack_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = bombjack_constants::PALETTE_ENTRIES;
    traits.audio.format            = AudioFormat::CUSTOM;
    traits.audio.sample_rate_hz    = bombjack_constants::DEFAULT_SAMPLE_RATE;
    traits.audio.channels          = 1;
    traits.audio.chip_name         = "3× AY-3-8910";
    traits.timing.cpu_frequency_hz = bombjack_constants::MAIN_CPU_FREQ_HZ;
    traits.timing.target_fps       = bombjack_constants::REFRESH_HZ;
    traits.timing.cycles_per_frame = bombjack_constants::MAIN_CYCLES_PER_FRAME;
    traits.timing.standard         = VideoStandard::NTSC;
    hardware_traits_ = traits;
    bombjack_descriptor.hardware_traits = traits;
}

BombJackSystem::~BombJackSystem() { delete main_cpu_; delete sound_cpu_; }

const SystemDescriptor& BombJackSystem::get_descriptor() const { return bombjack_descriptor; }
bool BombJackSystem::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool BombJackSystem::apply_configuration() { return true; }

bool BombJackSystem::initialize() {
    printf("Bomb Jack: Initializing arcade system\n");
    main_cpu_  = new ZilogZ80A();
    sound_cpu_ = new ZilogZ80A();
    main_pins_  = main_cpu_->init();
    sound_pins_ = sound_cpu_->init();
    for (auto& ay : ay_) ay.init();

    main_rom_.resize(bombjack_constants::MAIN_ROM_SIZE, 0xFF);
    main_ram_.resize(bombjack_constants::MAIN_RAM_SIZE, 0x00);
    fg_tilemap_.resize(bombjack_constants::FG_TILEMAP_SIZE, 0x00);
    fg_attr_.resize(bombjack_constants::FG_TILEMAP_SIZE, 0x00);
    sprite_ram_.resize(bombjack_constants::SPRITE_RAM_SIZE, 0x00);
    palette_ram_.resize(bombjack_constants::PALETTE_RAM_SIZE, 0x00);
    sound_rom_.resize(bombjack_constants::SOUND_ROM_SIZE, 0xFF);
    sound_ram_.resize(bombjack_constants::SOUND_RAM_SIZE, 0x00);
    load_roms();
    system_ready_ = true;
    return true;
}

void BombJackSystem::shutdown() {
    delete main_cpu_;  main_cpu_  = nullptr;
    delete sound_cpu_; sound_cpu_ = nullptr;
    system_ready_ = false;
}

void BombJackSystem::reset() {
    if (!main_cpu_ || !sound_cpu_) return;
    main_pins_  = main_cpu_->reset(main_pins_);
    sound_pins_ = sound_cpu_->reset(sound_pins_);
    for (auto& ay : ay_) ay.reset();
    sound_latch_ = 0;
    sound_nmi_ = false;
}

void BombJackSystem::tick() {
    if (!main_cpu_ || !sound_cpu_) return;
    // Main CPU tick
    main_pins_ = main_cpu_->tick(main_pins_);
    // Sound CPU runs at 3/4 speed of main (3 MHz vs 4 MHz)
    // Simplified: tick sound CPU 3 times per 4 main ticks
    // TODO: Proper interleaved scheduling
    total_cycles_++;
}

void BombJackSystem::run_frame() {
    for (uint32_t i = 0; i < bombjack_constants::MAIN_CYCLES_PER_FRAME; ++i) tick();
}

bool BombJackSystem::load_file(const char*) { return false; }
uint32_t* BombJackSystem::get_framebuffer() { return framebuffer_; }
void BombJackSystem::get_display_dimensions(int* w, int* h) const {
    *w = bombjack_constants::FB_WIDTH; *h = bombjack_constants::FB_HEIGHT;
}
void BombJackSystem::set_framebuffer(uint32_t*, int, int) {}
uint32_t BombJackSystem::get_audio_samples(float*, uint32_t) { return 0; }
void BombJackSystem::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
void BombJackSystem::handle_keyboard_event(SDL_Keycode, bool) {}
void BombJackSystem::render_system_menu_items() {}
void BombJackSystem::render_configuration_ui() {}
void BombJackSystem::set_speed_multiplier(float m) { speed_multiplier_ = m; }

bus_state_t BombJackSystem::main_mem_tick(bus_state_t pins) { return pins; }
bus_state_t BombJackSystem::main_io_tick(bus_state_t pins) { return pins; }
bus_state_t BombJackSystem::sound_mem_tick(bus_state_t pins) { return pins; }
bus_state_t BombJackSystem::sound_io_tick(bus_state_t pins) { return pins; }
bool BombJackSystem::load_roms() { return false; }

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(bombjack_descriptor, [] { return std::make_unique<BombJackSystem>(); });
