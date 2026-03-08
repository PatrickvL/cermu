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

#include "../../core/emulated_system.h"
#include "../../core/system_lines.h"
#include "../../chip/cpu/z80/zilog_z80a.h"
#include "../../chip/video/spectrum_ula/ferranti_ula.h"
#include "../../chip/sound/ay_3_8910.h"
#include "spectrum_constants.h"
#include <cstdint>
#include <memory>
#include <vector>

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

// ============================================================================
// ZX Spectrum System
// ============================================================================

template<SpectrumVariant V>
class SpectrumSystem : public EmulatedSystem {
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
    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

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

    ZilogZ80A*      cpu_ = nullptr;   // Z80A CPU @ 3.5 MHz
    ferranti_ula_t  ula_;             // Ferranti ULA (video, keyboard, tape, contention)
    ay_3_8910_t     ay_;              // AY-3-8912 sound (128K only, but always present for simplicity)

    // ========================================================================
    // MEMORY
    // ========================================================================

    std::vector<uint8_t> ram_;        // 48KB or 128KB
    std::vector<uint8_t> rom_;        // 16KB or 32KB

    // 128K banking state
    uint8_t  bank_select_ = 0;       // Port $7FFD latch
    bool     bank_locked_ = false;   // Bit 5 of $7FFD: locks banking until reset

    // ========================================================================
    // SYSTEM STATE
    // ========================================================================

    bus_state_t pins_;
    bool        system_ready_ = false;
    uint32_t    frame_tstate_counter_ = 0;

    // ========================================================================
    // DISPLAY
    // ========================================================================

    uint32_t framebuffer_[spectrum_constants::TOTAL_WIDTH * spectrum_constants::TOTAL_HEIGHT]{};

    // ========================================================================
    // AUDIO
    // ========================================================================

    uint32_t audio_sample_rate_ = spectrum_constants::DEFAULT_SAMPLE_RATE;
    uint32_t audio_sample_counter_ = 0;
    uint32_t audio_sample_period_ = 0;
    std::vector<float> audio_buffer_;

    // ========================================================================
    // HELPERS
    // ========================================================================

    bus_state_t mem_tick(bus_state_t pins);
    bus_state_t io_tick(bus_state_t pins);
    void update_framebuffer();
    bool load_roms();
};
