#pragma once
/*
 * spectrum_system.h — ZX Spectrum 48K / 128K Emulated System
 *
 * The ZX Spectrum (1982) was Sinclair Research's mass-market 8-bit home
 * computer, hugely successful in the UK and Europe.  The 128K (1985/1986)
 * added bank-switched RAM, AY-3-8912 sound, and a second ROM.
 *
 * Templated on SpectrumVariant to share code between 48K and 128K.
 *
 * ZX Spectrum 48K:
 *   - Zilog Z80A CPU @ 3.5 MHz
 *   - Ferranti ULA (video, keyboard, tape, memory contention)
 *   - 48KB RAM + 16KB ROM
 *   - 256×192 pixel display, 15 colors, attribute-based coloring
 *   - Audio: 1-bit beeper via ULA
 *
 * ZX Spectrum 128K:
 *   - Same Z80A + ULA
 *   - 128KB RAM (8 × 16KB banks, paged at $C000-$FFFF)
 *   - 2 × 16KB ROM (ROM 0 = 128K editor, ROM 1 = 48K BASIC)
 *   - AY-3-8912 sound chip (3 channels)
 *   - Port $7FFD controls RAM/ROM banking + display bank
 */

#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/system_chip_visitors.hpp"
#include "core/signal/video_port.hpp"
#include "core/signal/audio_port.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/spectrum_ula/ferranti_ula.hpp"
#include "chip/sound/ay_psg/ay_3_8912.hpp"
#include "chip/memory/memory_chip.hpp"
#include "systems/spectrum/spectrum_constants.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <memory>

// ============================================================================
// Spectrum Variant Template
// ============================================================================

enum class SpectrumVariant { ZX48K, ZX128K };

template<SpectrumVariant V> struct SpectrumVariantTraits;

template<> struct SpectrumVariantTraits<SpectrumVariant::ZX48K> {
    static constexpr const char* name         = "ZX Spectrum 48K";
    static constexpr const char* short_name   = "Spectrum48K";
    static constexpr const char* description  = "Sinclair ZX Spectrum 48K (1982)";
    static constexpr const char* data_folder  = "spectrum";
    static constexpr bool has_ay_sound        = false;
    static constexpr bool has_banking         = false;
    static constexpr int  ram_size_kb         = 48;
    static constexpr int  rom_count           = 1;
    static std::vector<const char*> get_aliases() {
        return {"Spectrum", "Spectrum48K", "ZXSpectrum", "ZX48K", "Speccy"};
    }
};

template<> struct SpectrumVariantTraits<SpectrumVariant::ZX128K> {
    static constexpr const char* name         = "ZX Spectrum 128K";
    static constexpr const char* short_name   = "Spectrum128K";
    static constexpr const char* description  = "Sinclair ZX Spectrum 128K (1985)";
    static constexpr const char* data_folder  = "spectrum";
    static constexpr bool has_ay_sound        = true;
    static constexpr bool has_banking         = true;
    static constexpr int  ram_size_kb         = 128;
    static constexpr int  rom_count           = 2;
    static std::vector<const char*> get_aliases() {
        return {"Spectrum128K", "ZX128K", "Spectrum128"};
    }
};

// ============================================================================
// ZX Spectrum default bus state
// ============================================================================

#define SPECTRUM_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

// =============================================================================
// ZX Spectrum chip manifests — declarative memory layout
// =============================================================================
//
// 48K:
//   RAM — 64 KB at $0000 (only $4000–$FFFF used; ROM overlays reads)
//   ROM — 16 KB at $0000 (read overlay)
//
// 128K:
//   RAM — 128 KB at $0000 (8 × 16 KB banks; $4000=$bank5, $8000=$bank2,
//                          $C000=switchable via port $7FFD)
//   ROM —  32 KB at $0000 (2 × 16 KB banks; selected by $7FFD bit 4)
//

// ── 48K manifest ─────────────────────────────────────────────────────────
#define SPECTRUM48K_FOR_EACH_SYSTEM_CHIP(X, ctx)                                                                \
    X(ctx, ZilogZ80A,      z80,  0x0000,      0, 0, 0, "Z80A",         nullptr)                                 \
    X(ctx, RAMChip,        ram,  0x0000,  65536, 0, 0, "RAM",          nullptr)                                 \
    X(ctx, ROMChip,        rom,  0x0000,  16384, 0, 1, "ROM",          "spectrum48k.rom|48.rom|spectrum.rom|zx48.rom") \
    X(ctx, ferranti_ula_t, ula,  0x0000,      0, 0, 0, "Ferranti ULA", nullptr)                                 \
    X(ctx, AY_3_8912,      psg,  0x0000,      0, 0, 0, "AY-3-8912",    nullptr)

static constexpr size_t kSpectrum48KChipCount = 0 SPECTRUM48K_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kSpectrum48KChipCount> kSpectrum48KChips = ChipManifest<kSpectrum48KChipCount>{{
    SPECTRUM48K_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

// ── 128K manifest ────────────────────────────────────────────────────────
#define SPECTRUM128K_FOR_EACH_SYSTEM_CHIP(X, ctx)                                                               \
    X(ctx, ZilogZ80A,      z80,  0x0000,       0, 0, 0, "Z80A",         nullptr)                                \
    X(ctx, RAMChip,        ram,  0x0000,  131072, 0, 0, "RAM",          nullptr)                                \
    X(ctx, ROMChip,        rom,  0x0000,   32768, 0, 1, "ROM",          "spectrum128k.rom|128.rom|128-0.rom")   \
    X(ctx, ferranti_ula_t, ula,  0x0000,       0, 0, 0, "Ferranti ULA", nullptr)                                \
    X(ctx, AY_3_8912,      psg,  0x0000,       0, 0, 0, "AY-3-8912",    nullptr)

static constexpr size_t kSpectrum128KChipCount = 0 SPECTRUM128K_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

// Helper: set bank_size on RAM and ROM slots for 128K banked memory.
template<size_t N>
constexpr ChipManifest<N> with_bank_size_on_memory(ChipManifest<N> m, size_t bs) {
    for (size_t i = 0; i < N; ++i)
        if (m.chips[i].size_bytes > 0) m.chips[i].bank_size = bs;
    return m;
}

inline constexpr ChipManifest<kSpectrum128KChipCount> kSpectrum128KChips =
    with_bank_size_on_memory(ChipManifest<kSpectrum128KChipCount>{{
        SPECTRUM128K_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
    }}, 16384);

// ── BusTraits — selects the correct manifest per variant ─────────────────
template<SpectrumVariant V> struct SpectrumBusTraits;

template<> struct SpectrumBusTraits<SpectrumVariant::ZX48K> {
    static constexpr const auto& kManifest = kSpectrum48KChips;
    using Spec = ManifestBusSpec<kSpectrum48KChips, 16, 8>;
};

template<> struct SpectrumBusTraits<SpectrumVariant::ZX128K> {
    static constexpr const auto& kManifest = kSpectrum128KChips;
    using Spec = ManifestBusSpec<kSpectrum128KChips, 16, 8>;
};

// ============================================================================
// Spectrum Chips — value-typed chips owned by Board
// ============================================================================
// Both 48K and 128K share the same chip types, so one struct suffices.
// (The X-macros are identical in field layout, only ROM/RAM sizes differ.)

struct SpectrumChips {
    SPECTRUM48K_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
};

// ============================================================================
// ZX Spectrum System
// ============================================================================

template<SpectrumVariant V>
class SpectrumSystem : public System {
    using Traits = SpectrumVariantTraits<V>;

public:
    SpectrumSystem();
    ~SpectrumSystem() override;

    // System identification
    const SystemDescriptor& get_descriptor() const override;

    // Configuration
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    // Lifecycle
    bool initialize() override;
    void shutdown() override;
    void reset() override;

    // Execution
    void tick() override;
    void run_frame() override;

    // File loading
    bool load_file(const char* filepath) override;

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI
    void render_configuration_ui() override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ========================================================================
    // MEMORY — owned by Board
    // ========================================================================

    // Direct pointer into flat mem for screen rendering
    uint8_t* screen_ram_ptr_ = nullptr;

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = SpectrumBusTraits<V>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec, SpectrumChips>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // 128K banking state
    uint8_t  bank_select_ = 0;       // Port $7FFD latch
    bool     bank_locked_ = false;   // Bit 5 of $7FFD: locks banking until reset

    // ========================================================================
    // SYSTEM STATE
    // ========================================================================

    bus_state_t pins_;
    uint32_t    frame_tstate_counter_ = 0;
    int         int_counter_ = 0;      // T-states remaining for INT assertion (0 = deasserted)
    uint8_t     keyboard_rows_[8]{};   // Shadow of ULA keyboard matrix (active-low, 0xFF = no keys)

    // ========================================================================
    // DISPLAY
    // ========================================================================

    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output

    // ========================================================================
    // AUDIO
    // ========================================================================

    uint32_t audio_sample_rate_ = spectrum_constants::DEFAULT_SAMPLE_RATE;
    uint32_t audio_sample_counter_ = 0;
    uint32_t audio_sample_period_ = 0;
    AudioRingBuffer audio_ring_buf_{8192};
    std::unique_ptr<AudioPort> audio_port_;  // Audio signal output

    // ========================================================================
    // HELPERS
    // ========================================================================

    void configure_bus_memory_map();
    void update_banking();           // 128K: remap pages after $7FFD write
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
