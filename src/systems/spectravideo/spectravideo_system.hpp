#pragma once
/*
 * spectravideo_system.hpp — Spectravideo SVI-318 / SVI-328 Emulated System
 *
 * The Spectravideo SVI series (1983) are Z80-based home computers
 * with TMS9918A video, AY-3-8910 sound, and i8255 PPI.
 * The SVI-328 is the MSX-predecessor with 64KB RAM.
 *
 * Templated on SVIVariant for SVI-318 (16KB) vs SVI-328 (64KB).
 */

#include "systems/spectravideo/spectravideo_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/standard_chips.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/tms9918/tms9918a.hpp"
#include "chip/sound/ay_psg/ay_3_8910.hpp"
#include "chip/io/i8255.hpp"
#include "chip/memory/memory_chip.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// SVI Variant Template
// ============================================================================

enum class SVIVariant { SVI318, SVI328 };

template<SVIVariant V> struct SVIVariantTraits;

template<> struct SVIVariantTraits<SVIVariant::SVI318> {
    static constexpr const char* name         = "Spectravideo SVI-318";
    static constexpr const char* short_name   = "SVI-318";
    static constexpr const char* description  = "Spectravideo SVI-318 — Z80A, TMS9918A, AY-3-8910, 16KB RAM (1983)";
    static constexpr const char* data_folder  = "spectravideo";
    static constexpr uint32_t    ram_size     = svi_constants::RAM_SIZE_SVI318;
    static std::vector<const char*> get_aliases() {
        return {"SVI318", "SVI-318", "Spectravideo318"};
    }
};

template<> struct SVIVariantTraits<SVIVariant::SVI328> {
    static constexpr const char* name         = "Spectravideo SVI-328";
    static constexpr const char* short_name   = "SVI-328";
    static constexpr const char* description  = "Spectravideo SVI-328 — Z80A, TMS9918A, AY-3-8910, 64KB RAM (1983)";
    static constexpr const char* data_folder  = "spectravideo";
    static constexpr uint32_t    ram_size     = svi_constants::RAM_SIZE_SVI328;
    static std::vector<const char*> get_aliases() {
        return {"SVI328", "SVI-328", "Spectravideo328"};
    }
};

// ============================================================================
// SVI default bus state
// ============================================================================

#define SVI_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// =============================================================================
// SVI chip manifests
// =============================================================================

inline constexpr auto kSVI318Chips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 32768, 0, "BASIC ROM"}.with_rom("svi318.rom|SVI318.ROM|svi.rom"),
    Slot<RAMChip>{0x8000, 16384, 0, "RAM"},
    // Non-bus chips
    Slot<ZilogZ80A>  {0, 0, 0, "Z80A"},
    Slot<TMS9918A>   {0, 0, 0, "TMS9918A"},
    Slot<AY_3_8910>  {0, 0, 0, "AY-3-8910"},
    Slot<i8255_t>    {0, 0, 0, "i8255 PPI"}
);

inline constexpr auto kSVI328Chips = make_chip_manifest(
    Slot<ROMChip>{0x0000, 32768, 0, "BASIC ROM"}.with_rom("svi328.rom|SVI328.ROM|svi.rom"),
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    // Non-bus chips
    Slot<ZilogZ80A>  {0, 0, 0, "Z80A"},
    Slot<TMS9918A>   {0, 0, 0, "TMS9918A"},
    Slot<AY_3_8910>  {0, 0, 0, "AY-3-8910"},
    Slot<i8255_t>    {0, 0, 0, "i8255 PPI"}
);

template<SVIVariant V> struct SVIBusTraits;

template<> struct SVIBusTraits<SVIVariant::SVI318> {
    static constexpr const auto& kManifest = kSVI318Chips;
    using Spec = ManifestBusSpec<kSVI318Chips, 16, 8>;
};

template<> struct SVIBusTraits<SVIVariant::SVI328> {
    static constexpr const auto& kManifest = kSVI328Chips;
    using Spec = ManifestBusSpec<kSVI328Chips, 16, 8>;
};

// ============================================================================
// SVI ChipSet — value-typed chips embedded in Board
// ============================================================================

struct SVIChips : StandardChips<ZilogZ80A, TMS9918A, AY_3_8910, i8255_t> {};

// ============================================================================
// Spectravideo System
// ============================================================================

template<SVIVariant V>
class SpectravideoSystem : public System {
    using Traits = SVIVariantTraits<V>;
    using BT     = SVIBusTraits<V>;
    using Chips  = SVIChips;

public:
    SpectravideoSystem();
    ~SpectravideoSystem() override;

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

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;

    // ── Audio ────────────────────────────────────────────────────────────
    uint32_t audio_sample_rate_    = svi_constants::DEFAULT_SAMPLE_RATE;
    uint32_t audio_sample_counter_ = 0;
    uint32_t audio_sample_period_  = 0;
    AudioRingBuffer audio_ring_buf_{8192};
    std::unique_ptr<AudioPort> audio_port_;

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[svi_constants::KEYBOARD_ROWS]{};

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_         = SVI_BUS_DEFAULT_STATE;
    uint32_t    frame_tstate_counter_ = 0;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
