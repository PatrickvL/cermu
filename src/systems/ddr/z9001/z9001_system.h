#pragma once
/*
 * z9001_system.h — Robotron Z9001 / KC 87 system declaration
 *
 * Two variants:
 *   Z9001  — 16 KB RAM, no color, optional BASIC (1984)
 *   KC 87  — 48 KB RAM, color attribute RAM, built-in BASIC (1987)
 */


#include "z9001_constants.h"
#include "../../../core/emulated_system.h"
#include "../../../core/system_lines.h"
#include "../../../chip/cpu/z80/u880.h"
#include "../../../chip/cpu/z80/z80.hpp"   // Z80_MREQ_BIT / Z80_IORQ_BIT
#include "../../../chip/io/z80_pio.h"
#include "../../../chip/io/z80_ctc.h"
#include <cstdint>
#include <vector>

#define Z9001_BUS_DEFAULT_STATE (U880::default_bus_state())

// ── Variants ─────────────────────────────────────────────────────────────
enum class Z9001Variant { Z9001, KC87 };

template<Z9001Variant V> struct Z9001VariantTraits;

template<> struct Z9001VariantTraits<Z9001Variant::Z9001> {
    static constexpr const char* name            = "Robotron Z9001";
    static constexpr const char* short_name      = "Z9001";
    static constexpr const char* description     = "Robotron Z9001 — U880 @ 2.4576MHz, 16KB RAM, 40×24 text (1984)";
    static constexpr uint32_t    ram_size        = z9001_constants::RAM_SIZE_Z9001;
    static constexpr bool        has_color_ram   = false;
    static constexpr bool        has_basic_rom   = false;
};

template<> struct Z9001VariantTraits<Z9001Variant::KC87> {
    static constexpr const char* name            = "Robotron KC 87";
    static constexpr const char* short_name      = "KC87";
    static constexpr const char* description     = "Robotron KC 87 — U880 @ 2.4576MHz, 48KB RAM, color text, BASIC (1987)";
    static constexpr uint32_t    ram_size        = z9001_constants::RAM_SIZE_KC87;
    static constexpr bool        has_color_ram   = true;
    static constexpr bool        has_basic_rom   = true;
};

// ── System ───────────────────────────────────────────────────────────────
template<Z9001Variant V>
class Z9001System : public EmulatedSystem {
    using Traits = Z9001VariantTraits<V>;
public:
    Z9001System();
    ~Z9001System() override;

    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    bool load_file(const char* filepath) override;

    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ── Chips ────────────────────────────────────────────────────────────
    U880*       cpu_  = nullptr;     // U880 @ 2.4576 MHz
    z80_pio_t   pio1_;               // U855 PIO #1 (keyboard + system control)
    z80_pio_t   pio2_;               // U855 PIO #2 (keyboard + cassette)
    z80_ctc_t   ctc_;                // U857 CTC (timing + sound)

    // ── Memory ───────────────────────────────────────────────────────────
    std::vector<uint8_t> ram_;       // 16 KB (Z9001) or 48 KB (KC 87)
    std::vector<uint8_t> os_rom_;    // 4 KB OS ROM
    std::vector<uint8_t> basic_rom_; // 10 KB BASIC ROM (KC 87 only)
    std::vector<uint8_t> video_ram_; // 1 KB screen buffer
    std::vector<uint8_t> color_ram_; // 1 KB color attributes (KC 87 only)
    std::vector<uint8_t> char_rom_;  // 2 KB character generator

    // ── Display ──────────────────────────────────────────────────────────
    uint32_t framebuffer_[z9001_constants::FB_WIDTH *
                          z9001_constants::FB_HEIGHT] = {};

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[z9001_constants::KEYBOARD_ROWS] = {};

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = Z9001_BUS_DEFAULT_STATE;
    bool        system_ready_ = false;
    uint64_t total_cycles_  = 0;
    int audio_sample_rate_  = z9001_constants::DEFAULT_SAMPLE_RATE;
    float speed_multiplier_ = 1.0f;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t mem_tick(bus_state_t pins);
    bus_state_t io_tick(bus_state_t pins);
    void        render_frame();   // Render one complete video frame to framebuffer_
    bool        load_roms();
};
