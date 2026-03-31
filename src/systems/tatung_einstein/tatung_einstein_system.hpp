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
#include "core/system_chip_visitors.hpp"
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

// =============================================================================
// Einstein chip declaration — single source of truth
// =============================================================================
//
// Row: V(ctx, type, chip, base, size, mask, overlay, label, rom_files)
//
//   Slot 0: RAM       — 64KB at $0000
//   Slot 1: OS ROM    — 8KB at $0000 (overlay, banked out by writing port $23)
//   Slot 2: Z80A      — not bus-mapped
//   Slot 3: TMS9929A  — not bus-mapped (I/O port-accessed)
//   Slot 4: AY-3-8910 — not bus-mapped (I/O port-accessed)
//   Slot 5: Z80 CTC   — not bus-mapped (I/O port-accessed)
//   Slot 6: Z80 PIO   — not bus-mapped (I/O port-accessed)
//

#define EINSTEIN_FOR_EACH_SYSTEM_CHIP(V, ctx)                                                                         \
    V(ctx, ZilogZ80A,   z80,  0x0000,       0, 0, 0, "Z80A",       nullptr)                                            \
    V(ctx, AY_3_8910,   psg,  0x0000,       0, 0, 0, "AY-3-8910",  nullptr)                                            \
    V(ctx, ROMChip,     rom,  0x0000,  0x2000, 0, 0, "OS ROM",     "einstein.rom|EINSTEIN.ROM|tcei.rom")                \
    V(ctx, RAMChip,     ram,  0x0000, 0x10000, 0, 0, "Main RAM",   nullptr)                                            \
    V(ctx, TMS9929A,    vdp,  0x0003,       0, 0, 0, "TMS9929A",   nullptr)                                            \
    V(ctx, z80_ctc_t,   ctc,  0x0008,       0, 0, 0, "Z80 CTC",    nullptr)                                            \
    V(ctx, z80_pio_t,   pio,  0x0010,       0, 0, 0, "Z80 PIO",    nullptr)

static constexpr size_t kEinsteinChipCount = 0 EINSTEIN_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kEinsteinChipCount> kEinsteinChips = ChipManifest<kEinsteinChipCount>{{
    EINSTEIN_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

using EinsteinBusSpec = ManifestBusSpec<kEinsteinChips, 16, 8>;

// ============================================================================
// Einstein Chips — value-typed chips owned by Board (auto-generated)
// ============================================================================

struct EinsteinChips {
    EINSTEIN_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
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
    using MainBoard = Board<EinsteinBusSpec, EinsteinChips>;
    Bus       bus_;
    MainBoard board_{kEinsteinChips};

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
