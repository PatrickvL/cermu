/*
 * bombjack_system.cpp — Bomb Jack arcade system implementation
 */

#include "core/cermu.hpp"
#include "systems/arcade/bombjack/bombjack_system.hpp"
#include "utils/resistor_dac.hpp"
#include "core/system_registry.hpp"
#include <cstring>
#include <cstdio>

// ============================================================================
// SYSTEM DESCRIPTOR
// ============================================================================

static SystemDescriptor bombjack_descriptor = {
    "Bomb Jack", "BombJack",
    "Tehkan Bomb Jack — dual Z80A, 3× AY-3-8910, 256×224 sprites+tiles (1984)",
    "bombjack", {"BombJack", "Bomb Jack"},
    nullptr, {}, nullptr,
    "Tehkan", 1984, "Z80A (x2)", SystemType::Arcade
};

// ============================================================================
// IMPLEMENTATION
// ============================================================================

BombJackSystem::BombJackSystem()
    : System()
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

BombJackSystem::~BombJackSystem() {}

const SystemDescriptor& BombJackSystem::get_descriptor() const { return bombjack_descriptor; }
bool BombJackSystem::set_configuration(const SystemConfiguration& config) { config_ = config; return true; }
bool BombJackSystem::apply_configuration() { return true; }

bool BombJackSystem::initialize() {
    log_info("Bomb Jack: Initializing arcade system\n");
    register_board(&main_board_);

    // ── Bind value-typed chips, then create remaining ─────────────────
    bind_all(main_board_, main_board_.components_, kBombJackMainManifest);
    main_board_.create_chips(&main_pins_);
    main_board_.apply(main_bus_);

    bind_all(sound_board_, sound_board_.components_, kBombJackSoundManifest);
    sound_board_.create_chips(&sound_pins_);
    sound_board_.apply(sound_bus_);

    // ── Set port manifest ───────────────────────────────────────────────
    port_manifest_       = kBombJackPortManifest.port_slots;
    port_manifest_count_ = kBombJackPortManifest.port_count;

    // ── Init chips ─────────────────────────────────────────────────────
    main_pins_  = main_board_.cpu.init();
    sound_pins_ = sound_board_.cpu.init();
    for (int i = 0; i < 3; i++) {
        ay_[i].init();
        // AY clock = sound CPU / 2 = 1.5 MHz
        ay_[i].set_clock_frequency(1500000);
        ay_[i].set_audio_sample_rate(bombjack_constants::DEFAULT_SAMPLE_RATE);
    }

    // Wire 3× AY to audio thread — cpu_cycles_per_tick=2 (AY = sound CPU / 2)
    for (int i = 0; i < 3; i++) {
        ay_adapter_[i] = std::make_unique<WriteOnlySynthAdapter<AY_3_8910, true>>(
            &ay_[i], 2);
        audio_thread_.register_engine(ay_adapter_[i].get());
        // Wire each AY to its own audio signal port
        audio_port_[i] = std::make_unique<AudioPort>();
        ay_[i].set_audio_port(audio_port_[i].get());
    }
    audio_thread_.start();

    load_roms();

    // ── GPU indexed palette rendering ───────────────────────────────
    palette_ = PaletteTable(bombjack_constants::PALETTE_ENTRIES);
    decode_palette();

    // Video output
    video_port_ = std::make_unique<CompositeVideoPort>();

    // Video generator — models TTL foreground tile rendering hardware
    video_gen_.set_video_out(&video_port_->output());
    video_gen_.set_char_rom(char_rom_.data(), static_cast<int>(char_rom_.size()));

    // Auto-reconstruct signal → framebuffer (palette is dynamically decoded)
    video_port_->bind_display(nullptr, palette_.data(),
                              bombjack_constants::FB_WIDTH, 1);
    video_port_->set_palette(palette_.data(), bombjack_constants::PALETTE_ENTRIES);
    video_port_->bind_frame_output(&last_frame_data_);

    // ── Register chips for Hardware menu ─────────────────────────────────

    register_bus_chips(main_board_);
    register_bus_chips(sound_board_);

    system_ready_ = true;
    return true;
}

void BombJackSystem::shutdown() {
    audio_thread_.stop();
    system_ready_ = false;
}

void BombJackSystem::reset() {
    main_pins_  = main_board_.cpu.reset(main_pins_);
    sound_pins_ = sound_board_.cpu.reset(sound_pins_);
    audio_thread_.stop();
    for (int i = 0; i < 3; i++) {
        ay_[i].reset();
        if (ay_adapter_[i]) ay_adapter_[i]->reset();
    }
    audio_thread_.start();
    sound_latch_ = 0;
    sound_nmi_ = false;
    sound_cycles_ = 0;
}

void BombJackSystem::tick() {

    // Main CPU tick
    main_pins_ = main_board_.cpu.tick(main_pins_);

    // Main CPU bus dispatch — memory-mapped only (no IORQ for main CPU)
    if (!BUS_GET_BIT(main_pins_, Z80_MREQ_BIT)) {
        uint16_t addr = BUS_GET_ADDR(main_pins_);
        if ((addr & 0xF000) == 0xB000) {
            main_pins_ = main_io_tick(main_pins_);
        } else {
            main_pins_ = main_bus_.tick(main_pins_);
        }
    }

    // Sound CPU runs at 3/4 speed (3 MHz vs 4 MHz main)
    // Tick sound CPU on 3 out of every 4 main cycles
    if (total_cycles_ % 4 != 3) {
        // Manage sound NMI line (edge-triggered: assert when latch written)
        if (sound_nmi_) {
            BUS_CLR_BIT(sound_pins_, BUS_NMI_BIT);
        } else {
            BUS_SET_BIT(sound_pins_, BUS_NMI_BIT);
        }

        sound_pins_ = sound_board_.cpu.tick(sound_pins_);

        // Sound CPU bus dispatch
        if (!BUS_GET_BIT(sound_pins_, Z80_MREQ_BIT)) {
            uint16_t addr = BUS_GET_ADDR(sound_pins_);
            if (addr == 0x6000) {
                // Sound latch read — clear NMI
                if (BUS_GET_BIT(sound_pins_, BUS_RW_BIT)) {
                    BUS_SET_DATA(sound_pins_, sound_latch_);
                    sound_nmi_ = false;
                }
            } else {
                sound_pins_ = sound_bus_.tick(sound_pins_);
            }
        } else if (!BUS_GET_BIT(sound_pins_, Z80_IORQ_BIT)) {
            sound_pins_ = sound_io_tick(sound_pins_);
        }

        // AY synthesis is driven by the audio thread — track sound CPU cycles
        // for timestamping register writes.
        sound_cycles_++;
    }

    // VBLANK NMI to main CPU (edge-triggered, once per frame)
    uint32_t frame_cycle = total_cycles_ % bombjack_constants::MAIN_CYCLES_PER_FRAME;
    if (frame_cycle == 0 && total_cycles_ > 0) {
        BUS_CLR_BIT(main_pins_, BUS_NMI_BIT);  // Assert NMI (falling edge)
    } else if (frame_cycle == 1) {
        BUS_SET_BIT(main_pins_, BUS_NMI_BIT);  // Deassert NMI
    }

    total_cycles_++;
}

void BombJackSystem::run_frame() {
    for (uint32_t i = 0; i < bombjack_constants::MAIN_CYCLES_PER_FRAME; ++i) tick();
    audio_thread_.signal_progress(sound_cycles_);
    decode_palette();
    render_frame();
    if (video_port_) video_port_->swap_frame();
}

uint32_t BombJackSystem::get_audio_samples(float* buffer, uint32_t max_samples) {
    if (!buffer || max_samples == 0) return 0;

    // AudioPort path: read from each AY's port and mix
    if (audio_port_[0]) {
        uint32_t avail = std::min({audio_port_[0]->available(),
                                   audio_port_[1]->available(),
                                   audio_port_[2]->available()});
        uint32_t count = std::min(avail, max_samples);
        if (count == 0) return 0;

        count = static_cast<uint32_t>(audio_port_[0]->read_samples(buffer, static_cast<int>(count)));

        constexpr uint32_t MIX_CHUNK = 256;
        float tmp[MIX_CHUNK];
        for (int chip = 1; chip < 3; chip++) {
            uint32_t remaining = count;
            uint32_t offset = 0;
            while (remaining > 0) {
                uint32_t n = std::min(MIX_CHUNK, remaining);
                audio_port_[chip]->read_samples(tmp, static_cast<int>(n));
                for (uint32_t i = 0; i < n; i++)
                    buffer[offset + i] += tmp[i];
                offset += n;
                remaining -= n;
            }
        }

        constexpr float inv3 = 1.0f / 3.0f;
        for (uint32_t i = 0; i < count; i++)
            buffer[i] *= inv3;

        return count;
    }

    // Legacy path: read from chip ring buffers
    // Read minimum available from all 3 AYs for synchronization
    uint32_t avail = std::min({ay_[0].audio_available(),
                               ay_[1].audio_available(),
                               ay_[2].audio_available()});
    uint32_t count = std::min(avail, max_samples);
    if (count == 0) return 0;

    // Read first AY into output buffer
    count = ay_[0].audio_read(buffer, count);

    // Mix in remaining AYs
    constexpr uint32_t MIX_CHUNK = 256;
    float tmp[MIX_CHUNK];
    for (int chip = 1; chip < 3; chip++) {
        uint32_t remaining = count;
        uint32_t offset = 0;
        while (remaining > 0) {
            uint32_t n = std::min(MIX_CHUNK, remaining);
            ay_[chip].audio_read(tmp, n);
            for (uint32_t i = 0; i < n; i++)
                buffer[offset + i] += tmp[i];
            offset += n;
            remaining -= n;
        }
    }

    // Normalize: average of 3 channels
    constexpr float inv3 = 1.0f / 3.0f;
    for (uint32_t i = 0; i < count; i++)
        buffer[i] *= inv3;

    return count;
}
void BombJackSystem::set_audio_sample_rate(int hz) {
    audio_sample_rate_ = hz;
    for (int i = 0; i < 3; i++) ay_[i].set_audio_sample_rate(hz);
}

// ============================================================================
// PALETTE DECODE — rebuild RGBA palette from palette RAM each frame
// ============================================================================
//
// Palette RAM at $9C00 (128 bytes used): each byte encodes one color
// using the same 3-3-2 resistor DAC as Pac-Man hardware.
//   bits [2:0] = red   (3-bit, weights 0x21/0x47/0x97)
//   bits [5:3] = green (3-bit, weights 0x21/0x47/0x97)
//   bits [7:6] = blue  (2-bit, weights 0x51/0xAE)

void BombJackSystem::decode_palette() {
    const uint8_t* pal = main_board_.palette.data();
    palette_.decode_from(pal, bombjack_constants::PALETTE_ENTRIES,
                         resistor_dac::decode_3_3_2);
}

// ============================================================================
// VIDEO RENDERING — delegate to BombJackVideo
// ============================================================================

void BombJackSystem::render_frame() {
    if (char_rom_.empty()) return;

    video_gen_.set_tilemap(main_board_.fg_map.data());
    video_gen_.set_attr_map(main_board_.fg_attr.data());
    video_gen_.render_frame();
}

// ============================================================================
// I/O DISPATCH — Main CPU ($B000-$BFFF memory-mapped registers)
// ============================================================================

bus_state_t BombJackSystem::main_io_tick(bus_state_t pins) {
    uint16_t addr = BUS_GET_ADDR(pins);

    if (BUS_GET_BIT(pins, BUS_RW_BIT)) {
        // Reads: input ports and DIP switches
        uint8_t data = 0xFF;
        if (addr == bombjack_constants::INPUT_P1)         data = input_p1_;
        else if (addr == bombjack_constants::INPUT_P2)    data = input_p2_;
        else if (addr == bombjack_constants::INPUT_SYSTEM) data = input_system_;
        else if (addr == bombjack_constants::DSW1)        data = main_board_.dsw1.bank.value;
        else if (addr == bombjack_constants::DSW2)        data = main_board_.dsw2.bank.value;
        BUS_SET_DATA(pins, data);
    } else {
        // Writes: sound latch, background select, watchdog
        uint8_t data = BUS_GET_DATA(pins);
        if (addr == bombjack_constants::SOUND_LATCH) {
            sound_latch_ = data;
            sound_nmi_ = true;
        } else if (addr == bombjack_constants::BG_SELECT) {
            bg_image_select_ = data & 0x07;
        }
        // Watchdog write at $B800 is intentionally ignored
    }

    return pins;
}
bus_state_t BombJackSystem::sound_io_tick(bus_state_t pins) {
    uint8_t port = static_cast<uint8_t>(BUS_GET_ADDR(pins));
    bool is_read = BUS_GET_BIT(pins, BUS_RW_BIT);
    uint8_t data = BUS_GET_DATA(pins);

    // Determine which AY chip and whether address or data register
    int ay_idx = -1;
    bool is_data = false;

    if ((port & 0xFE) == bombjack_constants::AY1_ADDR) {
        ay_idx = 0; is_data = port & 0x01;
    } else if ((port & 0xFE) == bombjack_constants::AY2_ADDR) {
        ay_idx = 1; is_data = port & 0x01;
    } else if ((port & 0xFE) == bombjack_constants::AY3_ADDR) {
        ay_idx = 2; is_data = port & 0x01;
    }

    if (ay_idx >= 0) {
        if (is_data) {
            if (is_read) {
                BUS_SET_DATA(pins, ay_[ay_idx].read_register());
            } else {
                // Shadow write for immediate readback, enqueue for audio thread
                ay_[ay_idx].write_register_shadow(data);
                ay_adapter_[ay_idx]->cmd_queue().push_write(
                    sound_cycles_, ay_latch_[ay_idx], data);
            }
        } else {
            if (!is_read) {
                ay_latch_[ay_idx] = data & 0x0F;
                ay_[ay_idx].latch_address(data);
            }
        }
    }

    return pins;
}
bool BombJackSystem::load_roms() { return false; }

// ============================================================================
// SYSTEM REGISTRATION
// ============================================================================

REGISTER_SYSTEM(bombjack_descriptor, [] { return std::make_unique<BombJackSystem>(); });
