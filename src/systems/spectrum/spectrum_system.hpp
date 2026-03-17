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
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/spectrum_ula/ferranti_ula.hpp"
#include "chip/sound/ay_3_8910.hpp"
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
//   Slot 0: RAM — 64 KB at $0000 (only $4000–$FFFF used; ROM overlays reads)
//   Slot 1: ROM — 16 KB at $0000 (read overlay)
//
// 128K:
//   Slot 0: RAM — 128 KB at $0000 (8 × 16 KB banks; $4000=$bank5, $8000=$bank2,
//                                  $C000=switchable via port $7FFD)
//   Slot 1: ROM —  32 KB at $0000 (2 × 16 KB banks; selected by $7FFD bit 4)
//
inline constexpr auto kSpectrum48KChips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>{0x0000, 16384, 0, "ROM"}.with_rom("spectrum48k.rom|48.rom|spectrum.rom|zx48.rom"),
    // Non-bus chips — factory-created or pre-bound, not address-decoded
    Slot<ZilogZ80A>     {0, 0, 0, "Z80A"},
    Slot<ferranti_ula_t>{0, 0, 0, "Ferranti ULA"},
    Slot<ay_3_8910_t>   {0, 0, 0, "AY-3-8912"}
);

inline constexpr auto kSpectrum128KChips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 131072, 0, "RAM", 0, 16384},
    Slot<ROMChip>{0x0000,  32768, 0, "ROM", 0, 16384}.with_rom("spectrum128k.rom|128.rom|128-0.rom"),
    // Non-bus chips — factory-created or pre-bound, not address-decoded
    Slot<ZilogZ80A>     {0, 0, 0, "Z80A"},
    Slot<ferranti_ula_t>{0, 0, 0, "Ferranti ULA"},
    Slot<ay_3_8910_t>   {0, 0, 0, "AY-3-8912"}
);


// BusTraits — selects the correct manifest per variant
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

    // Display
    void get_display_dimensions(int* width, int* height) const override;

    // Audio
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    // GUI
    void render_system_menu_items() override;
    void render_configuration_ui() override;

    // Emulation control
    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ========================================================================
    // CHIPS
    // ========================================================================

    ZilogZ80A*      cpu_ = nullptr;   // Z80A CPU @ 3.5 MHz — owned by board_
    ferranti_ula_t  ula_;             // Ferranti ULA (video, keyboard, tape, contention)
    ay_3_8910_t     ay_;              // AY-3-8912 sound (128K only, but always present for simplicity)

    // ========================================================================
    // MEMORY — owned by Board
    // ========================================================================

    // Direct pointer into flat mem for screen rendering
    uint8_t* screen_ram_ptr_ = nullptr;

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using BT  = SpectrumBusTraits<V>;
    using Bus = MemoryBus<typename BT::Spec>;
    using PT  = PackingTraits<typename BT::Spec>;
    using MainBoard = Board<typename BT::Spec>;
    Bus bus_;
    MainBoard board_{BT::kManifest};

    // 128K banking state
    uint8_t  bank_select_ = 0;       // Port $7FFD latch
    bool     bank_locked_ = false;   // Bit 5 of $7FFD: locks banking until reset

    // ========================================================================
    // SYSTEM STATE
    // ========================================================================

    bus_state_t pins_;
    bool        system_ready_ = false;
    uint32_t    frame_tstate_counter_ = 0;
    int         int_counter_ = 0;      // T-states remaining for INT assertion (0 = deasserted)
    uint8_t     keyboard_rows_[8]{};   // Shadow of ULA keyboard matrix (active-low, 0xFF = no keys)

    // ========================================================================
    // DISPLAY
    // ========================================================================

    // Display output — IndexedFrameBuffer owns palette + RGBA fallback.
    // ULA chip's pixel unit writes scanlines; display_ handles GPU routing.
    IndexedFrameBuffer display_;

    // ========================================================================
    // AUDIO
    // ========================================================================

    uint32_t audio_sample_rate_ = spectrum_constants::DEFAULT_SAMPLE_RATE;
    uint32_t audio_sample_counter_ = 0;
    uint32_t audio_sample_period_ = 0;
    AudioRingBuffer audio_ring_buf_{8192};

    // ========================================================================
    // HELPERS
    // ========================================================================

    void configure_bus_memory_map();
    void update_banking();           // 128K: remap pages after $7FFD write
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
