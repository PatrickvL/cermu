/*
 * acorn_atom_system.cpp — Acorn Atom system implementation
 */

#include "acorn_atom_system.h"
#include "../../core/system_registry.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor atom_descriptor = {
    "Acorn Atom", "Atom",
    "Acorn Atom — MOS 6502 @ 1MHz, MC6847 VDG, 2KB–12KB RAM (1980)",
    "acorn_atom", {"Atom", "AcornAtom"},
    nullptr, {}, nullptr
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

AcornAtomSystem::AcornAtomSystem() : EmulatedSystem(), pins_(ATOM_BUS_DEFAULT_STATE) {
    HardwareTraits traits = {};
    traits.display.native_width    = acorn_atom_constants::FB_WIDTH;
    traits.display.native_height   = acorn_atom_constants::FB_HEIGHT;
    traits.display.visible_width   = acorn_atom_constants::FB_WIDTH;
    traits.display.visible_height  = acorn_atom_constants::FB_HEIGHT;
    traits.display.format          = FramebufferFormat::RGBA8888;
    traits.display.palette_size    = acorn_atom_constants::COLOR_COUNT;
    traits.timing.cpu_frequency_hz = acorn_atom_constants::CPU_FREQ_HZ;
    traits.timing.target_fps       = 50;
    traits.timing.cycles_per_frame = acorn_atom_constants::CYCLES_PER_FRAME_PAL;
    traits.timing.standard         = VideoStandard::PAL;
    hardware_traits_ = traits;
    atom_descriptor.hardware_traits = traits;
}

AcornAtomSystem::~AcornAtomSystem() { delete cpu_; }

const SystemDescriptor& AcornAtomSystem::get_descriptor() const { return atom_descriptor; }
bool AcornAtomSystem::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool AcornAtomSystem::apply_configuration() { return true; }

bool AcornAtomSystem::initialize() {
    printf("Acorn Atom: Initializing system\n");
    cpu_ = new MOS6502();
    pins_ = cpu_->init();
    vdg_.init();
    ppi_.init();
    via_.reset();
    ram_.resize(ram_size_kb_ * 1024, 0x00);
    video_ram_.resize(acorn_atom_constants::VIDEO_RAM_SIZE, 0x00);
    basic_rom_.resize(acorn_atom_constants::BASIC_ROM_SIZE, 0xFF);
    fp_rom_.resize(acorn_atom_constants::FP_ROM_SIZE, 0xFF);
    os_rom_.resize(acorn_atom_constants::OS_ROM_SIZE, 0xFF);
    load_roms();
    system_ready_ = true;
    return true;
}

void AcornAtomSystem::shutdown() { delete cpu_; cpu_ = nullptr; system_ready_ = false; }
void AcornAtomSystem::reset() {
    if (!cpu_) return;
    pins_ = cpu_->reset(pins_);
    vdg_.init();
    ppi_.init();
    via_.reset();
}

void AcornAtomSystem::tick() {
    if (!cpu_) return;
    pins_ = cpu_->tick<MOS6502::Phase::PHI2>(pins_);
    // TODO: Memory dispatch (mem_tick), PPI keyboard scan, VIA tick, VDG scanline rendering
    pins_ = cpu_->tick<MOS6502::Phase::PHI1>(pins_);
    total_cycles_++;
}

void AcornAtomSystem::run_frame() {
    for (uint32_t i = 0; i < acorn_atom_constants::CYCLES_PER_FRAME_PAL; ++i) tick();
}

bool AcornAtomSystem::load_file(const char*) { return false; }
uint32_t* AcornAtomSystem::get_framebuffer() { return framebuffer_; }
void AcornAtomSystem::get_display_dimensions(int* w, int* h) const {
    *w = acorn_atom_constants::FB_WIDTH; *h = acorn_atom_constants::FB_HEIGHT;
}
void AcornAtomSystem::set_framebuffer(uint32_t*, int, int) {}
uint32_t AcornAtomSystem::get_audio_samples(float*, uint32_t) { return 0; }
void AcornAtomSystem::set_audio_sample_rate(int hz) { audio_sample_rate_ = hz; }
void AcornAtomSystem::handle_keyboard_event(SDL_Keycode, bool) {}
void AcornAtomSystem::render_system_menu_items() {}
void AcornAtomSystem::render_configuration_ui() {}
void AcornAtomSystem::set_speed_multiplier(float m) { speed_multiplier_ = m; }

bus_state_t AcornAtomSystem::mem_tick(bus_state_t pins) { return pins; }
bus_state_t AcornAtomSystem::io_tick(bus_state_t pins) { return pins; }
bool AcornAtomSystem::load_roms() { return false; }

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(atom_descriptor, [] { return std::make_unique<AcornAtomSystem>(); });
