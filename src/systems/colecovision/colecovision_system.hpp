#pragma once
/*
 * colecovision_system.hpp — ColecoVision Emulated System
 *
 * The ColecoVision (1982) was a home video game console by Coleco.
 * It shared the same TMS9918A+SN76489 chipset as the SG-1000 and MSX1,
 * making it architecturally similar but with different I/O decoding
 * and a unique controller design (12-key keypad + joystick).
 */

#include "systems/colecovision/colecovision_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "core/system_chip_visitors.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/tms9918/tms9918a.hpp"
#include "chip/sound/sn76489/sn76489.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// ColecoVision default bus state
// ============================================================================

#define COLECO_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// =============================================================================
// ColecoVision chip declaration — single source of truth
// =============================================================================
//
// Row: X(ctx, type, chip, base, mask, overlay, label, info_label, rom_files)
//
//   Slot 0: BIOS ROM — 8KB at $0000
//   Slot 1: Cart ROM — 32KB at $8000
//   Slot 2: RAM      — 1KB at $6000 (mirrored)
//   Slot 3: Z80A     — not bus-mapped
//   Slot 4: TMS9918A — not bus-mapped (I/O port-accessed)
//   Slot 5: SN76489  — not bus-mapped (I/O port-accessed)
//

#define COLECO_FOR_EACH_SYSTEM_CHIP(X, ctx)                                                                \
    X(ctx, ROMChip,     bios,  0x0000, 0x2000, 0, 0, "BIOS ROM",  "BIOS ROM",  "coleco.rom|colecovision.rom|COLECO.ROM") \
    X(ctx, ROMChip,     cart,  0x8000, 0x8000, 0, 0, "Cart ROM",  "Cart ROM",  nullptr)                   \
    X(ctx, RAMChip,     ram,   0x6000, 0x0400, 0, 0, "RAM",       "RAM",       nullptr)                   \
    X(ctx, ZilogZ80A,   z80,   0x0000,      0, 0, 0, "Z80A",      "Z80A",      nullptr)                   \
    X(ctx, TMS9918A,    vdp,   0x0000,      0, 0, 0, "TMS9918A",  "TMS9918A",  nullptr)                   \
    X(ctx, sn76489_t,   psg,   0x0000,      0, 0, 0, "SN76489",   "SN76489",   nullptr)

static constexpr size_t kColecoChipCount = 0 COLECO_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kColecoChipCount> kColecoChips = ChipManifest<kColecoChipCount>{{
    COLECO_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

using ColecoBusSpec = ManifestBusSpec<kColecoChips, 16, 8>;

// ============================================================================
// ColecoVision Chips — value-typed chips owned by Board (auto-generated)
// ============================================================================

struct ColecoChips {
    COLECO_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
};

// ============================================================================
// ColecoVision System
// ============================================================================

class ColecoVisionSystem : public System {
public:
    ColecoVisionSystem();
    ~ColecoVisionSystem() override;

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
    // ── Board + bus (chips live inside board_) ────────────────────────────
    using Bus       = MemoryBus<ColecoBusSpec>;
    using PT        = PackingTraits<ColecoBusSpec>;
    using MainBoard = Board<ColecoBusSpec, ColecoChips>;
    Bus       bus_;
    MainBoard board_{kColecoChips};

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_ = coleco_constants::DEFAULT_SAMPLE_RATE;
    std::unique_ptr<AudioPort> audio_port_;

    // ── Controllers ──────────────────────────────────────────────────────
    uint8_t ctrl_mode_ = 0;          // 0 or 1, selected by I/O write
    uint8_t ctrl1_joystick_ = 0x7F;  // Joystick state (active-low)
    uint8_t ctrl1_keypad_   = 0x0F;  // Keypad state

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_         = COLECO_BUS_DEFAULT_STATE;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
