#pragma once
/*
 * memotech_mtx_system.hpp — Memotech MTX500 / MTX512 Emulated System
 *
 * The Memotech MTX series (1983) are Z80-based home computers
 * with TMS9918A video, AY-3-8910 sound, and Z80 CTC for timing.
 *
 * Templated on MTXVariant for MTX500 (32KB) vs MTX512 (64KB).
 */

#include "systems/memotech_mtx/memotech_mtx_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/system_chip_visitors.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/tms9918/tms9918a.hpp"
#include "chip/sound/ay_psg/ay_3_8910.hpp"
#include "chip/io/z80_ctc.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// MTX Variant Template
// ============================================================================

enum class MTXVariant { MTX500, MTX512 };

template<MTXVariant V> struct MTXVariantTraits;

template<> struct MTXVariantTraits<MTXVariant::MTX500> {
    static constexpr const char* name         = "Memotech MTX500";
    static constexpr const char* short_name   = "MTX500";
    static constexpr const char* description  = "Memotech MTX500 — Z80A @ 4MHz, TMS9918A, AY-3-8910, 32KB RAM (1983)";
    static constexpr const char* data_folder  = "memotech_mtx";
    static constexpr uint32_t    ram_size     = mtx_constants::RAM_SIZE_MTX500;
    static std::vector<const char*> get_aliases() {
        return {"MTX500", "MTX 500"};
    }
};

template<> struct MTXVariantTraits<MTXVariant::MTX512> {
    static constexpr const char* name         = "Memotech MTX512";
    static constexpr const char* short_name   = "MTX512";
    static constexpr const char* description  = "Memotech MTX512 — Z80A @ 4MHz, TMS9918A, AY-3-8910, 64KB RAM (1983)";
    static constexpr const char* data_folder  = "memotech_mtx";
    static constexpr uint32_t    ram_size     = mtx_constants::RAM_SIZE_MTX512;
    static std::vector<const char*> get_aliases() {
        return {"MTX512", "MTX 512"};
    }
};

// ============================================================================
// MTX default bus state
// ============================================================================

#define MTX_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// =============================================================================
// MTX chip declarations — variant-specific single source of truth
// =============================================================================
//
// Row: X(ctx, type, chip, base, size, mask, overlay, label, rom_files)
//
// MTX500: ROM 16KB at $0000, RAM 32KB at $4000
// MTX512: ROM 16KB at $0000, RAM 64KB at $0000
//

#define MTX500_FOR_EACH_SYSTEM_CHIP(X, ctx)                                                                               \
    X(ctx, ZilogZ80A,  z80,  0x0000,      0, 0, 0, "Z80A",         nullptr)                                               \
    X(ctx, ROMChip,    rom,  0x0000, 0x4000, 0, 0, "OS+BASIC ROM", "mtx500.rom|mtx.rom|MTX.ROM")                         \
    X(ctx, TMS9918A,   vdp,  0x0001,      0, 0, 0, "TMS9918A",     nullptr)                                               \
    X(ctx, AY_3_8910,  psg,  0x0003,      0, 0, 0, "AY-3-8910",    nullptr)                                               \
    X(ctx, z80_ctc_t,  ctc,  0x0008,      0, 0, 0, "Z80 CTC",      nullptr)                                               \
    X(ctx, RAMChip,    ram,  0x4000, 0x8000, 0, 0, "RAM",          nullptr)

#define MTX512_FOR_EACH_SYSTEM_CHIP(X, ctx)                                                                               \
    X(ctx, ZilogZ80A,  z80,  0x0000,       0, 0, 0, "Z80A",         nullptr)                                              \
    X(ctx, ROMChip,    rom,  0x0000,  0x4000, 0, 0, "OS+BASIC ROM", "mtx512.rom|mtx.rom|MTX.ROM")                        \
    X(ctx, RAMChip,    ram,  0x0000, 0x10000, 0, 0, "RAM",          nullptr)                                              \
    X(ctx, TMS9918A,   vdp,  0x0001,       0, 0, 0, "TMS9918A",     nullptr)                                              \
    X(ctx, AY_3_8910,  psg,  0x0003,       0, 0, 0, "AY-3-8910",    nullptr)                                              \
    X(ctx, z80_ctc_t,  ctc,  0x0008,       0, 0, 0, "Z80 CTC",      nullptr)

static constexpr size_t kMTX500ChipCount = 0 MTX500_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);
static constexpr size_t kMTX512ChipCount = 0 MTX512_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kMTX500ChipCount> kMTX500Chips = ChipManifest<kMTX500ChipCount>{{
    MTX500_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

inline constexpr ChipManifest<kMTX512ChipCount> kMTX512Chips = ChipManifest<kMTX512ChipCount>{{
    MTX512_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

template<MTXVariant V> struct MTXBusTraits;

template<> struct MTXBusTraits<MTXVariant::MTX500> {
    static constexpr const auto& kManifest = kMTX500Chips;
    using Spec = ManifestBusSpec<kMTX500Chips, 16, 8>;
};

template<> struct MTXBusTraits<MTXVariant::MTX512> {
    static constexpr const auto& kManifest = kMTX512Chips;
    using Spec = ManifestBusSpec<kMTX512Chips, 16, 8>;
};

// ============================================================================
// MTX Chips — value-typed chips owned by Board (auto-generated)
// ============================================================================

struct MTXChips {
    MTX500_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
};

// ============================================================================
// Memotech MTX System
// ============================================================================

template<MTXVariant V>
class MemotechMTXSystem : public System {
    using Traits = MTXVariantTraits<V>;
    using BT     = MTXBusTraits<V>;

public:
    MemotechMTXSystem();
    ~MemotechMTXSystem() override;

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
    using Bus       = MemoryBus<typename BT::Spec>;
    using PT        = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec, MTXChips>;
    Bus       bus_;
    MainBoard board_{BT::kManifest};

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_    = mtx_constants::DEFAULT_SAMPLE_RATE;
    uint32_t audio_sample_counter_ = 0;
    uint32_t audio_sample_period_  = 0;
    AudioRingBuffer audio_ring_buf_{8192};
    std::unique_ptr<AudioPort> audio_port_;

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[mtx_constants::KEYBOARD_ROWS]{};
    uint8_t keyboard_drive_ = 0;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_         = MTX_BUS_DEFAULT_STATE;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
