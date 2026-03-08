#pragma once
/*
 * lc80_system.h — LC 80 learning computer system declaration
 *
 * The LC 80 has no video output; it uses multiplexed 7-segment LEDs.
 * We render the LED display into a small framebuffer for the GUI.
 */


#include "lc80_constants.h"
#include "../../core/emulated_system.h"
#include "../../core/system_lines.h"
#include "../../chip/cpu/z80/u880.h"
#include "../../chip/io/z80_pio.h"
#include "../../chip/io/z80_ctc.h"
#include <cstdint>
#include <vector>

// LC80 bus — Z80 bus defaults
#define LC80_BUS_DEFAULT_STATE (U880::default_bus_state())

class LC80System : public EmulatedSystem {
public:
    LC80System();
    ~LC80System() override;

    // ── EmulatedSystem interface ─────────────────────────────────────────
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

private:
    // ── Chips ────────────────────────────────────────────────────────────
    U880*       cpu_  = nullptr;     // U880 (Z80A clone) @ 900 kHz
    z80_pio_t   pio1_;               // U855 PIO #1 (LED display + keyboard)
    z80_pio_t   pio2_;               // U855 PIO #2 (keyboard scan + cassette)
    z80_ctc_t   ctc_;                // U857 CTC (speaker on channel 2)

    // ── Memory ───────────────────────────────────────────────────────────
    std::vector<uint8_t> rom_;       // 2 KB monitor ROM
    std::vector<uint8_t> ram_;       // 1–2 KB RAM

    // ── LED display ──────────────────────────────────────────────────────
    // Segment data for each of the 6 digits (bit 0..6 = a..g, bit 7 = dp)
    uint8_t led_segments_[lc80_constants::LED_DIGIT_COUNT] = {};

    // Render the LED display into a small framebuffer for visualization
    static constexpr int FB_WIDTH  = 192;   // 6 digits × 32 px each
    static constexpr int FB_HEIGHT = 48;
    uint32_t framebuffer_[FB_WIDTH * FB_HEIGHT] = {};

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint32_t key_state_ = 0;        // Bit field for 25 keys

    // ── Speaker ──────────────────────────────────────────────────────────
    bool speaker_state_ = false;

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = LC80_BUS_DEFAULT_STATE;
    uint64_t total_cycles_  = 0;
    int audio_sample_rate_  = lc80_constants::DEFAULT_SAMPLE_RATE;
    float speed_multiplier_ = 1.0f;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t mem_tick(bus_state_t pins);
    bus_state_t io_tick(bus_state_t pins);
    bool load_roms();
};
