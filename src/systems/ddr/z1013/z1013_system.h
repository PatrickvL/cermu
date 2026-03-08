#pragma once
/*
 * z1013_system.h — Robotron Z1013 system declaration
 *
 * Simple DDR home computer with character display and membrane keyboard.
 * Three known variants: Z1013.01 (original), Z1013.12, Z1013.64 (64KB).
 */


#include "z1013_constants.h"
#include "../../../core/emulated_system.h"
#include "../../../core/system_lines.h"
#include "../../../chip/cpu/z80/u880.h"
#include "../../../chip/cpu/z80/z80.hpp"   // Z80_MREQ_BIT / Z80_IORQ_BIT
#include "../../../chip/io/z80_pio.h"
#include <cstdint>
#include <vector>

#define Z1013_BUS_DEFAULT_STATE (U880::default_bus_state())

// ── Variants ─────────────────────────────────────────────────────────────
enum class Z1013Variant { Z1013_01, Z1013_16, Z1013_64 };

template<Z1013Variant V> struct Z1013VariantTraits;

template<> struct Z1013VariantTraits<Z1013Variant::Z1013_01> {
    static constexpr const char* name            = "Robotron Z1013.01";
    static constexpr const char* short_name      = "Z1013.01";
    static constexpr const char* description     = "Robotron Z1013.01 — U880 @ 2MHz, 16KB RAM, 32×32 text (1985)";
    static constexpr uint32_t    ram_size        = z1013_constants::RAM_SIZE_16K;
    static constexpr bool        has_basic_rom   = false;
};

template<> struct Z1013VariantTraits<Z1013Variant::Z1013_16> {
    static constexpr const char* name            = "Robotron Z1013.16";
    static constexpr const char* short_name      = "Z1013.16";
    static constexpr const char* description     = "Robotron Z1013.16 — U880 @ 2MHz, 16KB RAM, membrane keyboard (1987)";
    static constexpr uint32_t    ram_size        = z1013_constants::RAM_SIZE_16K;
    static constexpr bool        has_basic_rom   = false;
};

template<> struct Z1013VariantTraits<Z1013Variant::Z1013_64> {
    static constexpr const char* name            = "Robotron Z1013.64";
    static constexpr const char* short_name      = "Z1013.64";
    static constexpr const char* description     = "Robotron Z1013.64 — U880 @ 2MHz, 64KB RAM, ROM BASIC (1988)";
    static constexpr uint32_t    ram_size        = z1013_constants::RAM_SIZE_64K;
    static constexpr bool        has_basic_rom   = true;
};

// ── System ───────────────────────────────────────────────────────────────
template<Z1013Variant V>
class Z1013System : public EmulatedSystem {
    using Traits = Z1013VariantTraits<V>;
public:
    Z1013System();
    ~Z1013System() override;

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
    U880*       cpu_  = nullptr;     // U880 (Z80A clone) @ 2 MHz
    z80_pio_t   pio_;                // U855 PIO (keyboard + cassette)

    // ── Memory ───────────────────────────────────────────────────────────
    std::vector<uint8_t> ram_;       // 16 KB or 64 KB
    std::vector<uint8_t> monitor_rom_;  // 2 KB monitor
    std::vector<uint8_t> basic_rom_;    // 10 KB ROM BASIC (Z1013.64 only)
    std::vector<uint8_t> video_ram_;    // 1 KB screen buffer ($EC00–$EFFF)
    std::vector<uint8_t> char_rom_;     // 2 KB character generator

    // ── Display ──────────────────────────────────────────────────────────
    uint32_t framebuffer_[z1013_constants::FB_WIDTH *
                          z1013_constants::FB_HEIGHT] = {};

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[z1013_constants::KEYBOARD_ROWS] = {};
    uint8_t keyboard_column_select_ = 0xFF;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = Z1013_BUS_DEFAULT_STATE;
    bool        system_ready_ = false;
    uint64_t total_cycles_  = 0;
    int audio_sample_rate_  = z1013_constants::DEFAULT_SAMPLE_RATE;
    float speed_multiplier_ = 1.0f;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t mem_tick(bus_state_t pins);
    bus_state_t io_tick(bus_state_t pins);
    void        render_frame();   // Render one complete video frame to framebuffer_
    bool        load_roms();
};
