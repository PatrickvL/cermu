#pragma once

#include "systems/chip8/chip8_constants.hpp"
#include "core/system.hpp"
#include "core/framebuffer_renderer.hpp"
#include <cstdint>
#include <cstring>
#include <vector>

/**
 * CHIP-8 / SCHIP / XO-CHIP Emulator System
 *
 * Supports three interpreter levels:
 *   CHIP8  –  Original 4KB RAM, 64×32 1-bit display, 35 opcodes
 *   SCHIP  –  Super-CHIP 1.1: 128×64 hi-res, scroll, 16×16 sprites, RPL flags
 *   XOCHIP –  XO-CHIP: 64KB RAM, dual-plane 4-color display, audio patterns
 *
 * The mode can be auto-detected from the ROM or set manually.
 * Display is always presented at 128×64; lo-res mode doubles each pixel.
 */

enum class Chip8Mode {
    CHIP8,   // Standard CHIP-8
    SCHIP,   // Super-CHIP 1.1
    XOCHIP   // XO-CHIP
};

class Chip8System : public System {
public:
    Chip8System();
    ~Chip8System() override = default;
    
    // System interface
    const SystemDescriptor& get_descriptor() const override;
    
    // Configuration management
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;
    
    // System lifecycle
    void reset() override;
    
    // Execution
    void tick() override;
    void run_frame() override;
    
    // File loading
    bool load_file(const char* filepath) override;
    
    // Display
    void get_display_dimensions(int* width, int* height) const override;
    
    // Input
    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    
    // GUI integration
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    
    // Emulation control
    void set_speed_multiplier(float multiplier) override;

    // Audio output — beeper or XO-CHIP audio pattern
    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;

private:
    // Main board (no ports, but required by System)
    BoardBase board_;

    // ── Interpreter mode ────────────────────────────────────────────────
    Chip8Mode mode_;                 // Active interpreter level

    // ── Memory ──────────────────────────────────────────────────────────
    std::vector<uint8_t> memory_;    // 4KB (CHIP8/SCHIP) or 64KB (XO-CHIP)

    // ── Registers ───────────────────────────────────────────────────────
    uint8_t V_[16];                  // V0-VF general-purpose
    uint16_t I_;                     // Index register
    uint16_t PC_;                    // Program counter
    uint16_t stack_[16];             // Call stack
    uint8_t SP_;                     // Stack pointer
    uint8_t delay_timer_;            // Delay timer (60 Hz)
    uint8_t sound_timer_;            // Sound timer (60 Hz)
    uint8_t rpl_flags_[16];         // RPL user flags (SCHIP FX75/FX85)

    // ── Display ─────────────────────────────────────────────────────────
    // Two 1-bit planes, each 128×64 pixels = 1024 bytes/plane.
    // Plane 0 and plane 1 are combined for XO-CHIP 4-color output.
    // In lo-res mode only the top-left 64×32 quadrant is active (doubled on output).
    static constexpr int PLANE_SIZE = chip8_constants::HIRES_WIDTH * chip8_constants::HIRES_HEIGHT / 8;  // 1024 bytes
    uint8_t planes_[2][PLANE_SIZE]; // plane 0, plane 1
    bool hires_;                     // true = 128×64, false = 64×32
    uint8_t active_plane_mask_;      // bitmask: bit0=plane0, bit1=plane1

    // GPU indexed rendering
    DisplaySurface display_;

    // ── Input ───────────────────────────────────────────────────────────
    uint8_t keys_[16];               // 16-key keypad state

    // ── Emulation state ─────────────────────────────────────────────────
    uint32_t cycles_per_frame_;
    bool display_dirty_;
    bool wait_for_key_;              // FX0A key-wait state
    uint8_t wait_key_reg_;           // Register to store key for FX0A

    // ── Quirks/settings ─────────────────────────────────────────────────
    bool shift_quirk_;               // SHR/SHL use Vy (COSMAC original)
    bool load_store_quirk_;          // FX55/FX65 increment I
    bool jump_quirk_;                // BNNN uses Vx instead of V0 (SCHIP)
    bool clip_quirk_;                // Sprites clip at screen edge (vs wrap)
    bool vf_reset_quirk_;            // 8XY1/2/3 reset VF

    // ── Audio ───────────────────────────────────────────────────────────
    uint32_t beeper_phase_;          // Phase accumulator for square wave
    uint8_t audio_pattern_[16];     // XO-CHIP 16-byte audio pattern buffer
    uint8_t pitch_register_;         // XO-CHIP pitch (F03A)
    bool has_audio_pattern_;         // True if F002 was used

    // ── Internal helpers ────────────────────────────────────────────────
    void execute_instruction(uint16_t opcode);
    void update_timers();

    // Display helpers
    int display_width() const  { return hires_ ? chip8_constants::HIRES_WIDTH  : 64; }
    int display_height() const { return hires_ ? chip8_constants::HIRES_HEIGHT : 32; }
    int bytes_per_row() const  { return display_width() / 8; }

    void scroll_down(int n);
    void scroll_up(int n);
    void scroll_right();
    void scroll_left();
    void draw_sprite(uint8_t x, uint8_t y, uint8_t n);

    // Key mapping (SDL keycode to CHIP-8 key)
    int map_sdl_key_to_chip8(int sdl_key);

    /// Register logical CHIP-8 chips into registered_chips_ for the Hardware menu.
    void register_chip8_chips();

};