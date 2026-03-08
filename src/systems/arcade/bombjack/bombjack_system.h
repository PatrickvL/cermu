#pragma once
/*
 * bombjack_system.h — Bomb Jack arcade system declaration
 *
 * Tehkan Bomb Jack (1984) — dual Z80 arcade board.
 *   Main CPU:   Z80A @ 4 MHz (gameplay, video)
 *   Sound CPU:  Z80A @ 3 MHz (driven by sound latch commands)
 *   Sound:      3× AY-3-8910 PSG
 *   Video:      256×224, background + tilemap + sprites
 *   Inputs:     2 players (joystick + 2 buttons), coins, DIP switches
 */


#include "bombjack_constants.h"
#include "../../../core/emulated_system.h"
#include "../../../core/system_lines.h"
#include "../../../chip/cpu/z80/zilog_z80a.h"
#include "../../../chip/sound/ay_3_8910.h"
#include <cstdint>
#include <vector>

#define BOMBJACK_BUS_DEFAULT_STATE (ZilogZ80A::default_bus_state())

class BombJackSystem : public EmulatedSystem {
public:
    BombJackSystem();
    ~BombJackSystem() override;

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

    bool is_system_ready() const override { return system_ready_; }

private:
    // ── CPUs ─────────────────────────────────────────────────────────────
    ZilogZ80A*  main_cpu_  = nullptr;    // Z80A @ 4 MHz (main)
    ZilogZ80A*  sound_cpu_ = nullptr;    // Z80A @ 3 MHz (sound)

    // ── Sound ────────────────────────────────────────────────────────────
    ay_3_8910_t ay_[3];                  // 3× AY-3-8910 PSG

    // ── Memory (main CPU) ────────────────────────────────────────────────
    std::vector<uint8_t> main_rom_;      // 32 KB program ROM
    std::vector<uint8_t> main_ram_;      // 4 KB work RAM
    std::vector<uint8_t> fg_tilemap_;    // 1 KB foreground tilemap
    std::vector<uint8_t> fg_attr_;       // 1 KB foreground attributes
    std::vector<uint8_t> sprite_ram_;    // Sprite attribute table (96 bytes)
    std::vector<uint8_t> palette_ram_;   // 256 bytes palette RAM

    // ── Memory (sound CPU) ───────────────────────────────────────────────
    std::vector<uint8_t> sound_rom_;     // 8 KB sound program ROM
    std::vector<uint8_t> sound_ram_;     // 1 KB sound work RAM

    // ── Graphics ROM ─────────────────────────────────────────────────────
    std::vector<uint8_t> char_rom_;      // Character/tile ROM
    std::vector<uint8_t> sprite_rom_;    // Sprite graphics ROM
    std::vector<uint8_t> bg_rom_;        // Background image ROM

    // ── Inter-CPU communication ──────────────────────────────────────────
    uint8_t sound_latch_ = 0;           // Main → Sound command latch
    bool    sound_nmi_   = false;       // NMI to sound CPU on latch write

    // ── Display ──────────────────────────────────────────────────────────
    uint32_t framebuffer_[bombjack_constants::FB_WIDTH *
                          bombjack_constants::FB_HEIGHT] = {};
    uint8_t bg_image_select_ = 0;       // Active background (0–4)

    // ── Inputs ───────────────────────────────────────────────────────────
    uint8_t input_p1_     = 0xFF;       // Player 1 (active low)
    uint8_t input_p2_     = 0xFF;       // Player 2 (active low)
    uint8_t input_system_ = 0xFF;       // Coin/start (active low)
    uint8_t dsw1_         = 0xFF;       // DIP switch bank 1
    uint8_t dsw2_         = 0xFF;       // DIP switch bank 2

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t main_pins_  = BOMBJACK_BUS_DEFAULT_STATE;
    bus_state_t sound_pins_ = BOMBJACK_BUS_DEFAULT_STATE;
    bool        system_ready_ = false;
    uint64_t total_cycles_  = 0;
    int audio_sample_rate_  = bombjack_constants::DEFAULT_SAMPLE_RATE;
    float speed_multiplier_ = 1.0f;

    // ── Internal helpers ─────────────────────────────────────────────────
    bus_state_t main_mem_tick(bus_state_t pins);
    bus_state_t main_io_tick(bus_state_t pins);
    bus_state_t sound_mem_tick(bus_state_t pins);
    bus_state_t sound_io_tick(bus_state_t pins);
    bool load_roms();
};
