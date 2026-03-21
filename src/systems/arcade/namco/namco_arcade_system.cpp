/*
 * namco_arcade_system.cpp — Namco Pac-Man / Pengo arcade system implementation
 */

#include "systems/arcade/namco/namco_arcade_system.hpp"
#include "utils/resistor_dac.hpp"
#include "core/system_registry.hpp"
#include "core/storage/rom_loader.hpp"
#include "core/config/path_discovery.hpp"
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
    : System()
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
NamcoArcadeSystem<G>::~NamcoArcadeSystem() {
    audio_thread_.stop();
}

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
    register_board(&board_);

    // ── Create memory chips from manifest and wire bus ────────────────
    board_.bind_chipset();
    board_.create_chips(&pins_);
    board_.apply(bus_);

    // ── Init CPU + sound ────────────────────────────────────────────
    vram_chip_ = board_.template find<RAMChip>();      // Video RAM
    cram_chip_ = board_.template find<RAMChip>(1);     // Color RAM
    pins_ = board_.cpu().init();
    board_.sound().init();
    // WSG clock = CPU / 32 = 96 kHz
    board_.sound().set_clock_frequency(namco_arcade_constants::CPU_FREQ_HZ / 32);
    board_.sound().set_audio_sample_rate(namco_arcade_constants::DEFAULT_SAMPLE_RATE);

    // Wire WSG to audio thread — WSG clocked at CPU/32
    wsg_adapter_ = std::make_unique<WriteOnlySynthAdapter<namco_wsg_t, true>>(&board_.sound(), 32);
    audio_thread_.register_engine(wsg_adapter_.get());
    audio_thread_.start();

    // Wire WSG to audio signal port
    audio_port_ = std::make_unique<AudioPort>();
    board_.sound().set_audio_port(audio_port_.get());

    // Graphics ROMs — not bus-mapped
    char_rom_.resize(Traits::char_rom_size, 0xFF);
    sprite_rom_.resize(namco_arcade_constants::SPRITE_ROM_SIZE, 0xFF);
    palette_prom_.resize(namco_arcade_constants::PALETTE_PROM_SIZE, 0x00);
    colortable_prom_.resize(namco_arcade_constants::COLORTABLE_PROM_SIZE, 0x00);
    waveform_rom_.resize(namco_arcade_constants::WAVEFORM_ROM_SIZE, 0x00);

    load_roms();
    decode_palette();

    // ── GPU indexed palette rendering ───────────────────────────────
    display_.init(namco_arcade_constants::FB_WIDTH, namco_arcade_constants::FB_HEIGHT);
    decode_palette();
    register_display(&display_);

    // Video stream output
    video_port_ = std::make_unique<CompositeVideoPort>();

    // Video generator — models TTL tile rendering (224×288 already-rotated output)
    video_gen_.set_stream(&video_port_->stream());
    video_gen_.set_char_rom(char_rom_.data(), static_cast<int>(char_rom_.size()));
    video_gen_.set_colortable_prom(colortable_prom_.data());

    // Auto-reconstruct stream → framebuffer (palette is dynamically decoded)
    video_port_->bind_display(&display_, display_.palette_data(),
                              namco_arcade_constants::FB_WIDTH, 1);
    video_port_->bind_frame_output(&last_frame_data_);

    // ── Register chips for Hardware menu ────────────────────────────────
    register_bus_chips(board_);

    printf("%s: System initialized (ROM: %d KB)\n",
           Traits::name, Traits::rom_size / 1024);
    system_ready_ = true;
    return true;
}

template<NamcoGame G> void NamcoArcadeSystem<G>::shutdown() {
    audio_thread_.stop();
    system_ready_ = false;
}
template<NamcoGame G> void NamcoArcadeSystem<G>::reset() {
    if (!system_ready_) return;
    pins_ = board_.cpu().reset(pins_);
    audio_thread_.stop();
    if (wsg_adapter_) wsg_adapter_->reset();
    audio_thread_.start();
    int_enable_ = false;
    sound_enable_ = false;
    scanline_ = 0;
}

// ============================================================================
// TICK
// ============================================================================

template<NamcoGame G>
void NamcoArcadeSystem<G>::tick() {
    if (!system_ready_) return;

    // CPU tick
    pins_ = board_.cpu().tick(pins_);

    // Bus dispatch — Namco hardware uses memory-mapped I/O only
    bool mreq = !BUS_GET_BIT(pins_, Z80_MREQ_BIT);
    if (mreq) {
        uint16_t addr = BUS_GET_ADDR(pins_);
        // I/O region ($5xxx Pac-Man / $9xxx Pengo) needs manual dispatch
        // due to asymmetric read/write behavior
        if ((addr & 0xF000) == Traits::io_base) {
            pins_ = io_tick(pins_);
        } else {
            pins_ = bus_.tick(pins_);
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

    // WSG synthesis is driven by the audio thread — no direct tick here.
    total_cycles_++;
}

template<NamcoGame G> void NamcoArcadeSystem<G>::run_frame() {
    for (uint32_t i = 0; i < namco_arcade_constants::TSTATES_PER_FRAME; ++i) tick();
    // Signal audio thread once per frame with accumulated cycle count
    audio_thread_.signal_progress(total_cycles_);
    render_frame();
    if (video_port_) video_port_->swap_frame();
}

template<NamcoGame G> uint32_t NamcoArcadeSystem<G>::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;
    if (audio_port_) {
        return static_cast<uint32_t>(audio_port_->read_samples(buffer, static_cast<int>(max_samples)));
    }
    return board_.sound().audio_read(buffer, max_samples);
}
template<NamcoGame G> void NamcoArcadeSystem<G>::set_audio_sample_rate(int hz) {
    audio_sample_rate_ = hz;
    board_.sound().set_audio_sample_rate(hz);
}

// ============================================================================
// PALETTE DECODE — build RGBA palette from palette PROM
// ============================================================================
//
// Each palette PROM byte encodes one color using resistor-weighted DAC:
//   bits [2:0] = red   (3-bit, weights 0x21/0x47/0x97)
//   bits [5:3] = green (3-bit, weights 0x21/0x47/0x97)
//   bits [7:6] = blue  (2-bit, weights 0x51/0xAE)

template<NamcoGame G>
void NamcoArcadeSystem<G>::decode_palette() {
    const uint8_t* prom = palette_prom_.data();
    int count = std::min(static_cast<int>(palette_prom_.size()),
                         namco_arcade_constants::PALETTE_ENTRIES);
    display_.palette().decode_from(prom, count, resistor_dac::decode_3_3_2);
}

// ============================================================================
// VIDEO RENDERING — delegate to NamcoVideo
// ============================================================================

template<NamcoGame G>
void NamcoArcadeSystem<G>::render_frame() {
    if (!vram_chip_ || !cram_chip_) return;

    video_gen_.set_vram(vram_chip_->data());
    video_gen_.set_cram(cram_chip_->data());
    video_gen_.render_frame();
}

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
        // WSG sound registers at $x040-$x05F — enqueue for audio thread
        else if (addr >= Traits::io_base + 0x40 && addr < Traits::io_base + 0x60) {
            uint8_t offset = addr - (Traits::io_base + 0x40);
            if (wsg_adapter_) {
                wsg_adapter_->cmd_queue().push_write(total_cycles_, offset, data);
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
