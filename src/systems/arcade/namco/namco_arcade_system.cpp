/*
 * namco_arcade_system.cpp — Namco Pac-Man / Pengo arcade system implementation
 */

#include "namco_arcade_system.h"
#include "../../../core/system_registry.h"
#include "../../../core/storage/rom_loader.h"
#include "../../../core/config/path_discovery.h"
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

    // ── Create memory chips from manifest and wire bus ────────────────
    bus_mem_.create_chips(&pins_);
    bus_mem_.apply(bus_);

    // ── Init CPU + sound ────────────────────────────────────────────────
    cpu_ = new ZilogZ80A();
    pins_ = cpu_->init();
    wsg_.init();

    // Graphics ROMs — not bus-mapped
    char_rom_.resize(Traits::char_rom_size, 0xFF);
    sprite_rom_.resize(namco_arcade_constants::SPRITE_ROM_SIZE, 0xFF);
    palette_prom_.resize(namco_arcade_constants::PALETTE_PROM_SIZE, 0x00);
    colortable_prom_.resize(namco_arcade_constants::COLORTABLE_PROM_SIZE, 0x00);
    waveform_rom_.resize(namco_arcade_constants::WAVEFORM_ROM_SIZE, 0x00);

    load_roms();

    // ── Register chips for Hardware menu ────────────────────────────────
    register_chip(static_cast<ChipBase*>(cpu_),
        "Z80A CPU", "Z80A", "CPU", 0x0000);
    register_bus_chips(bus_mem_);

    printf("%s: System initialized (ROM: %d KB)\n",
           Traits::name, Traits::rom_size / 1024);
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

// ============================================================================
// TICK
// ============================================================================

template<NamcoGame G>
void NamcoArcadeSystem<G>::tick() {
    if (!cpu_) return;

    // CPU tick
    pins_ = cpu_->tick(pins_);

    // Bus dispatch — Namco hardware uses memory-mapped I/O only
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    if (mreq) {
        uint16_t addr = BUS_GET_ADDR(pins_);
        // I/O region ($5xxx Pac-Man / $9xxx Pengo) needs manual dispatch
        // due to asymmetric read/write behavior
        if ((addr & 0xF000) == Traits::io_base) {
            pins_ = io_tick(pins_);
        } else {
            pins_ = bus_.tick(0, pins_);
        }
    }

    // VBLANK IRQ generation
    constexpr uint32_t total_scanlines = 264;
    constexpr uint32_t cycles_per_scanline = namco_arcade_constants::TSTATES_PER_FRAME / total_scanlines;
    if (cycles_per_scanline > 0 && total_cycles_ % cycles_per_scanline == 0) {
        scanline_++;
        if (scanline_ >= total_scanlines) {
            scanline_ = 0;
        }
    }

    // Assert IRQ during VBLANK period when enabled, deassert otherwise
    if (int_enable_ && scanline_ >= namco_arcade_constants::VBLANK_SCANLINE) {
        BUS_CLR_BIT(pins_, BUS_IRQ_BIT);
    } else {
        BUS_SET_BIT(pins_, BUS_IRQ_BIT);
    }

    // WSG tick (~96 kHz = CPU_FREQ / 32)
    if ((total_cycles_ & 31) == 0) {
        wsg_.tick();
    }

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

// ============================================================================
// I/O DISPATCH
// ============================================================================
//
// I/O is memory-mapped at $5000-$50FF (Pac-Man) / $9000-$90FF (Pengo).
// Reads and writes at the SAME addresses serve DIFFERENT purposes:
//
//   Reads:
//     $x000-$x03F: IN0 (joystick + coins)
//     $x040-$x07F: IN1 (P2 + start buttons)
//     $x080-$x0BF: DSW1 (DIP switches)
//
//   Writes:
//     $x000-$x007: Control registers (int_enable, sound_enable, flip_screen, …)
//     $x040-$x05F: WSG3 sound registers (frequency, volume, waveform)
//     $x060-$x06F: Sprite positions (write-only)
//     $x0C0:       Watchdog reset (ignored)
//
template<NamcoGame G>
bus_state_t NamcoArcadeSystem<G>::io_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);

    if (is_read) {
        uint8_t data = 0xFF;
        if ((addr & ~0x3F) == Traits::io_base) {
            data = in0_;
        } else if ((addr & ~0x3F) == (Traits::io_base + 0x40)) {
            data = in1_;
        } else if ((addr & ~0x3F) == (Traits::io_base + 0x80)) {
            data = dsw1_;
        }
        BUS_SET_DATA(pins, data);
    } else {
        uint8_t data = BUS_GET_DATA(pins);
        // Control registers at $x000-$x007
        if (addr >= Traits::io_base && addr < Traits::io_base + 0x08) {
            uint8_t reg = addr & 0x07;
            if (reg == 0)      int_enable_   = data & 0x01;
            else if (reg == 1) sound_enable_ = data & 0x01;
            else if (reg == 3) flip_screen_  = data & 0x01;
            // reg 7 = watchdog (ignored)
        }
        // WSG sound registers at $x040-$x05F
        else if (addr >= Traits::io_base + 0x40 && addr < Traits::io_base + 0x60) {
            uint8_t offset = addr - (Traits::io_base + 0x40);
            if (offset < 5) {
                wsg_.write_freq(0, offset, data);
            } else if (offset < 10) {
                wsg_.write_freq(1, offset - 5, data);
            } else if (offset < 15) {
                wsg_.write_freq(2, offset - 10, data);
            } else if (offset == 0x0F) {
                wsg_.write_wave_vol(0, (data >> 4) & 0x07, data & 0x0F);
            } else if (offset == 0x10) {
                wsg_.write_wave_vol(1, (data >> 4) & 0x07, data & 0x0F);
            } else if (offset == 0x14) {
                wsg_.write_wave_vol(2, (data >> 4) & 0x07, data & 0x0F);
            }
        }
        // Sprite positions at $x060-$x06F (write-only from CPU side)
        // Watchdog at $x0C0 (ignored)
    }

    return pins;
}

// ============================================================================
// ROM LOADING
// ============================================================================

template<NamcoGame G>
bool NamcoArcadeSystem<G>::load_roms() {
    return false;  // ROM loading not yet implemented — requires romset handling
}

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
