#pragma once
/*
 * tatung_einstein_system.hpp — Tatung Einstein TC-01 Emulated System
 *
 * The Tatung Einstein (1984) is a Z80-based CP/M-compatible home computer
 * with PAL TMS9929A video, AY-3-8910 sound, Z80 CTC, and Z80 PIO.
 *
 * Notable features:
 *   - Built-in Tatung 3" disk drive
 *   - ROM can be banked out for full 64KB RAM (for CP/M)
 *   - Keyboard read via AY-3-8910 I/O ports A and B
 */

#include "systems/tatung_einstein/tatung_einstein_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/tms9918/tms9929a.hpp"
#include "chip/sound/ay_psg/ay_3_8910.hpp"
#include "chip/io/z80_ctc.hpp"
#include "chip/io/z80_pio.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// Einstein default bus state
// ============================================================================

#define EINSTEIN_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

inline constexpr auto kEinsteinManifest = make_manifest(
    // Chips
    Slot<ZilogZ80A>{.base_addr = 0x0000, .label = "Z80A"},
    Slot<AY_3_8910>{.base_addr = 0x0000, .label = "AY-3-8910"},
    Slot<ROMChip>{.base_addr = 0x0000, .size_bytes = 0x2000, .label = "OS ROM", .rom = {"einstein.rom|EINSTEIN.ROM|tcei.rom"}},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x10000, .label = "Main RAM"},
    Slot<TMS9929A>{.base_addr = 0x0003, .label = "TMS9929A"},
    Slot<z80_ctc_t>{.base_addr = 0x0008, .label = "Z80 CTC"},
    Slot<z80_pio_t>{.base_addr = 0x0010, .label = "Z80 PIO"},
    // Ports
    Slot<PortExpansion>{.name = "Expansion Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv_pal"},
    Slot<PortAudioMono>{.name = "Audio Out"}
);

inline constexpr size_t kEinsteinChipCount = decltype(kEinsteinManifest)::chip_count;
using EinsteinBusSpec = ManifestBusSpec<kEinsteinManifest, 16, 8>;

struct EinsteinBoard : Board<EinsteinBusSpec> {
    using ComponentTuple = decltype(kEinsteinManifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    ZilogZ80A& z80 = std::get<0>(components_);
    AY_3_8910& psg = std::get<1>(components_);
    ROMChip&   rom = std::get<2>(components_);
    RAMChip&   ram = std::get<3>(components_);
    TMS9929A&  vdp = std::get<4>(components_);
    z80_ctc_t& ctc = std::get<5>(components_);
    z80_pio_t& pio = std::get<6>(components_);

    // Port aliases
    PortExpansion&      expansion_port = std::get<7>(components_);
    PortCompositeVideo& video_port     = std::get<8>(components_);
    PortAudioMono&      audio_port     = std::get<9>(components_);

    EinsteinBoard() : Board(kEinsteinManifest) {}
};
// ============================================================================
// Tatung Einstein System
// ============================================================================

class TatungEinsteinSystem : public System {
public:
    TatungEinsteinSystem();
    ~TatungEinsteinSystem() override;

    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    bool load_file(const char* filepath) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<EinsteinBusSpec>;
    using PT        = PackingTraits<EinsteinBusSpec>;
    using MainBoard = EinsteinBoard;
    Bus       bus_;
    MainBoard board_;

    // ── ROM banking ──────────────────────────────────────────────────────
    bool rom_enabled_ = true;  // ROM overlays RAM at boot, disabled by port $23

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_    = einstein_constants::DEFAULT_SAMPLE_RATE;
    uint32_t audio_sample_counter_ = 0;
    uint32_t audio_sample_period_  = 0;
    AudioRingBuffer audio_ring_buf_{8192};
    std::unique_ptr<AudioPort> audio_port_;

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[einstein_constants::KEYBOARD_ROWS]{};

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_         = EINSTEIN_BUS_DEFAULT_STATE;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
