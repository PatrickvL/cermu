/*
 * lc80_system.cpp — LC 80 learning computer system implementation
 */

#include "lc80_system.h"
#include "../../../core/system_registry.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor lc80_descriptor = {
    "LC 80", "LC80",
    "VEB Mikroelektronik LC 80 — U880 @ 900kHz, 1KB RAM, 7-segment LED display (1984)",
    "lc80", {"LC80", "LC-80"},
    nullptr, {}, nullptr
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

LC80System::LC80System() : EmulatedSystem(), pins_(LC80_BUS_DEFAULT_STATE) {
    HardwareTraits traits = {};
    // The LC80 has no CRT — LED display rendered into small framebuffer
    traits.display.native_width    = FB_WIDTH;
    traits.display.native_height   = FB_HEIGHT;
    traits.display.visible_width   = FB_WIDTH;
    traits.display.visible_height  = FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = 2;
    traits.timing.cpu_frequency_hz = lc80_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = lc80_constants::UPDATE_RATE_HZ;
    traits.timing.cycles_per_frame = lc80_constants::CYCLES_PER_UPDATE;
    traits.timing.standard         = VideoStandard::PAL;
    hardware_traits_ = traits;
    lc80_descriptor.hardware_traits = traits;
}

LC80System::~LC80System() { delete cpu_; }

const SystemDescriptor& LC80System::get_descriptor() const { return lc80_descriptor; }
bool LC80System::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool LC80System::apply_configuration() { return true; }

bool LC80System::initialize() {
    printf("LC 80: Initializing system\n");
    cpu_ = new U880();
    pins_ = cpu_->init();
    pio1_.init();
    pio2_.init();
    ctc_.init();
    rom_.resize(lc80_constants::ROM_SIZE, 0xFF);
    ram_.resize(lc80_constants::RAM_SIZE_MIN, 0x00);
    load_roms();
    system_ready_ = true;
    return true;
}

void LC80System::shutdown() { delete cpu_; cpu_ = nullptr; system_ready_ = false; }
void LC80System::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    pio1_.init();
    pio2_.init();
    ctc_.init();
    std::memset(led_segments_, 0, sizeof(led_segments_));
}

void LC80System::tick() {
    if (!cpu_) return;
    pins_ = cpu_->tick(pins_);
    // TODO: Bus dispatch → ROM/RAM, I/O decode → PIO1/PIO2/CTC, speaker
    total_cycles_++;
}

void LC80System::run_frame() {
    for (uint32_t i = 0; i < lc80_constants::CYCLES_PER_UPDATE; ++i) tick();
}

bool LC80System::load_file(const char*) { return false; }
uint32_t* LC80System::get_framebuffer() { return framebuffer_; }
void LC80System::get_display_dimensions(int* w, int* h) const { *w = FB_WIDTH; *h = FB_HEIGHT; }
void LC80System::set_framebuffer(uint32_t*, int, int) {}
uint32_t LC80System::get_audio_samples(float*, uint32_t) { return 0; }
void LC80System::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
void LC80System::handle_keyboard_event(SDL_Keycode, bool) {}
void LC80System::render_system_menu_items() {}
void LC80System::render_configuration_ui() {}
void LC80System::set_speed_multiplier(float m) { speed_multiplier_ = m; }

bus_state_t LC80System::mem_tick(bus_state_t pins) { return pins; }
bus_state_t LC80System::io_tick(bus_state_t pins) { return pins; }
bool LC80System::load_roms() { return false; }

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(lc80_descriptor, [] { return std::make_unique<LC80System>(); });
