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
#include "core/standard_chips.hpp"
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
// MTX chip manifests
// =============================================================================

inline constexpr auto kMTX500Chips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 16384, 0, "OS+BASIC ROM"}.with_rom("mtx500.rom|mtx.rom|MTX.ROM"),
    Slot<RAMChip>{0x4000, 32768, 0, "RAM"},
    // Non-bus chips
    Slot<ZilogZ80A>  {0, 0, 0, "Z80A"},
    Slot<TMS9918A>   {0, 0, 0, "TMS9918A"},
    Slot<AY_3_8910>  {0, 0, 0, "AY-3-8910"},
    Slot<z80_ctc_t>  {0, 0, 0, "Z80 CTC"}
);

inline constexpr auto kMTX512Chips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 16384, 0, "OS+BASIC ROM"}.with_rom("mtx512.rom|mtx.rom|MTX.ROM"),
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    // Non-bus chips
    Slot<ZilogZ80A>  {0, 0, 0, "Z80A"},
    Slot<TMS9918A>   {0, 0, 0, "TMS9918A"},
    Slot<AY_3_8910>  {0, 0, 0, "AY-3-8910"},
    Slot<z80_ctc_t>  {0, 0, 0, "Z80 CTC"}
);

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
// MTX ChipSet — value-typed chips embedded in Board
// ============================================================================

struct MTXChips : StandardChips<ZilogZ80A, TMS9918A, AY_3_8910, z80_ctc_t> {};

// ============================================================================
// Memotech MTX System
// ============================================================================

template<MTXVariant V>
class MemotechMTXSystem : public System {
    using Traits = MTXVariantTraits<V>;
    using BT     = MTXBusTraits<V>;
    using Chips  = MTXChips;

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


private:
    // ── Board + bus (chips live inside board_) ────────────────────────────
    using Bus       = MemoryBus<typename BT::Spec>;
    using PT        = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec, Chips>;
    Bus       bus_;
    MainBoard board_{BT::kManifest};

    // ── Display ──────────────────────────────────────────────────────────
    IndexedFrameBuffer display_;
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
