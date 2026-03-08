#pragma once
/*
 * kc85_system.h — KC 85/2, /3, /4 system declaration
 *
 * Three variants, templated:
 *   KC85_2 (HC 900) — 16 KB RAM, no BASIC ROM, CAOS 2.2 (1984)
 *   KC85_3          — 16 KB RAM, built-in BASIC ROM, CAOS 3.1 (1986)
 *   KC85_4          — 64 KB RAM, extended video (2 planes), CAOS 4.2 (1989)
 */


#include "kc85_constants.h"
#include "../../../core/emulated_system.h"
#include "../../../core/system_lines.h"
#include "../../../chip/cpu/z80/u880.h"
#include "../../../chip/io/z80_pio.h"
#include "../../../chip/io/z80_ctc.h"
#include "../../../chip/io/kc85_module_system.h"
#include <cstdint>
#include <vector>

#define KC85_BUS_DEFAULT_STATE (U880::default_bus_state())

// ── Variants ─────────────────────────────────────────────────────────────
enum class KC85Variant { KC85_2, KC85_3, KC85_4 };

template<KC85Variant V> struct KC85VariantTraits;

template<> struct KC85VariantTraits<KC85Variant::KC85_2> {
    static constexpr const char* name            = "KC 85/2";
    static constexpr const char* short_name      = "KC85/2";
    static constexpr const char* description     = "VEB Mühlhausen KC 85/2 (HC 900) — U880 @ 1.77MHz, 16KB RAM, CAOS 2.2 (1984)";
    static constexpr uint32_t    ram_size        = kc85_constants::RAM_SIZE_16K;
    static constexpr bool        has_basic_rom   = false;
    static constexpr bool        has_extended_video = false;
    static constexpr const char* caos_version    = "2.2";
};

template<> struct KC85VariantTraits<KC85Variant::KC85_3> {
    static constexpr const char* name            = "KC 85/3";
    static constexpr const char* short_name      = "KC85/3";
    static constexpr const char* description     = "VEB Mühlhausen KC 85/3 — U880 @ 1.77MHz, 16KB RAM, BASIC, CAOS 3.1 (1986)";
    static constexpr uint32_t    ram_size        = kc85_constants::RAM_SIZE_16K;
    static constexpr bool        has_basic_rom   = true;
    static constexpr bool        has_extended_video = false;
    static constexpr const char* caos_version    = "3.1";
};

template<> struct KC85VariantTraits<KC85Variant::KC85_4> {
    static constexpr const char* name            = "KC 85/4";
    static constexpr const char* short_name      = "KC85/4";
    static constexpr const char* description     = "VEB Mühlhausen KC 85/4 — U880 @ 1.77MHz, 64KB RAM, dual-plane video, CAOS 4.2 (1989)";
    static constexpr uint32_t    ram_size        = kc85_constants::RAM_SIZE_64K;
    static constexpr bool        has_basic_rom   = true;
    static constexpr bool        has_extended_video = true;
    static constexpr const char* caos_version    = "4.2";
};

// ── System ───────────────────────────────────────────────────────────────
template<KC85Variant V>
class KC85System : public EmulatedSystem {
    using Traits = KC85VariantTraits<V>;
public:
    KC85System();
    ~KC85System() override;

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
    U880*                cpu_  = nullptr;     // U880 @ 1.7734 MHz
    z80_pio_t            pio1_;               // U855 PIO (system + keyboard)
    z80_pio_t            pio2_;               // U855 PIO (module system)
    z80_ctc_t            ctc_;                // U857 CTC (timing + sound + tape)
    kc85_module_system_t modules_;            // Expansion module slot controller

    // ── Memory ───────────────────────────────────────────────────────────
    std::vector<uint8_t> ram_;       // 16 KB (KC85/2,/3) or 64 KB (KC85/4)
    std::vector<uint8_t> os_rom_;    // 8 KB CAOS ROM
    std::vector<uint8_t> basic_rom_; // 8 KB BASIC ROM (KC85/3, /4)

    // ── Video RAM ────────────────────────────────────────────────────────
    // KC85/2 and /3: single plane, 16 KB pixel RAM (interleaved with color)
    std::vector<uint8_t> pixel_ram_;     // 16 KB pixel data
    std::vector<uint8_t> color_ram_;     // Color attributes (interleaved or separate)

    // KC85/4: second screen plane
    std::vector<uint8_t> pixel_ram_2_;   // 16 KB pixel data (plane 2)
    std::vector<uint8_t> color_ram_2_;   // Color data (plane 2)

    // ── Display ──────────────────────────────────────────────────────────
    uint32_t framebuffer_[kc85_constants::FB_WIDTH *
                          kc85_constants::FB_HEIGHT] = {};

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[kc85_constants::KEYBOARD_ROWS] = {};

    // ── Banking state (KC85/4 only) ──────────────────────────────────────
    uint8_t bank_ctrl_     = 0;      // Port $84 value
    uint8_t bank_ctrl2_    = 0;      // Port $86 value
    bool    irm_enabled_   = false;  // Video RAM access enabled
    bool    caos_rom_on_   = true;   // CAOS ROM bank enabled
    bool    basic_rom_on_  = false;  // BASIC ROM bank enabled
    uint8_t active_plane_  = 0;      // Display plane (KC85/4: 0 or 1)

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_       = KC85_BUS_DEFAULT_STATE;
    bool        system_ready_ = false;
    uint64_t total_cycles_  = 0;
    int audio_sample_rate_  = kc85_constants::DEFAULT_SAMPLE_RATE;
    float speed_multiplier_ = 1.0f;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t mem_tick(bus_state_t pins);
    bus_state_t io_tick(bus_state_t pins);
    void update_bank_state();        // Decode PIO B + $84/$86 → memory mapping
    bool load_roms();
};
