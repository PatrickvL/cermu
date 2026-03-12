#pragma once
/*
 * snes_standard_controller.h — SNES Standard Controller Peripheral
 *
 * Models the standard SNES gamepad (SNS-005) as a PeripheralDevice.
 * Internally uses a 16-bit shift register for the serial LATCH/CLK/D0
 * protocol — equivalent to two daisy-chained CD4021 shift registers
 * as used in the real hardware.
 *
 * Lives in src/devices/input/ because the SNES controller protocol is
 * compatible with NES controller ports (same LATCH/CLK/D0 signals),
 * enabling plug-in to NES, Famicom, and future SNES systems.
 *
 * Button bitmask layout (MSB → LSB, matching hardware shift order):
 *   B  Y  SELECT  START  UP  DOWN  LEFT  RIGHT  A  X  L  R  [0  0  0  0]
 *  15 14    13      12   11   10    9      8    7  6  5  4   3  2  1  0
 *
 * Bits 3-0 are always 0 (no button), producing D0=1 (released) on the bus
 * after all 12 buttons have been shifted out.
 */

#include "core/peripherals/input_peripheral_device.h"

#include <cstdint>
#include <SDL_scancode.h>

// ============================================================================
// SNES KEYBOARD MAP
// ============================================================================

/**
 * Maps host keyboard scancodes to SNES controller buttons.
 * Uses scancodes (layout-independent) like NesKeyMap / JoystickKeyMap.
 */
struct SnesKeyMap {
    SDL_Scancode up     = SDL_SCANCODE_UP;
    SDL_Scancode down   = SDL_SCANCODE_DOWN;
    SDL_Scancode left   = SDL_SCANCODE_LEFT;
    SDL_Scancode right  = SDL_SCANCODE_RIGHT;
    SDL_Scancode a      = SDL_SCANCODE_X;
    SDL_Scancode b      = SDL_SCANCODE_Z;
    SDL_Scancode x      = SDL_SCANCODE_S;
    SDL_Scancode y      = SDL_SCANCODE_A;
    SDL_Scancode l      = SDL_SCANCODE_Q;
    SDL_Scancode r      = SDL_SCANCODE_W;
    SDL_Scancode start  = SDL_SCANCODE_RETURN;
    SDL_Scancode select = SDL_SCANCODE_RSHIFT;
};

/// Preset: WASD directions + nearby keys for face/shoulder buttons (Player 1).
inline SnesKeyMap snes_keymap_wasd() {
    SnesKeyMap m;
    m.up     = SDL_SCANCODE_W;
    m.down   = SDL_SCANCODE_S;
    m.left   = SDL_SCANCODE_A;
    m.right  = SDL_SCANCODE_D;
    m.a      = SDL_SCANCODE_PERIOD;
    m.b      = SDL_SCANCODE_COMMA;
    m.x      = SDL_SCANCODE_L;
    m.y      = SDL_SCANCODE_K;
    m.l      = SDL_SCANCODE_I;
    m.r      = SDL_SCANCODE_O;
    m.start  = SDL_SCANCODE_RETURN;
    m.select = SDL_SCANCODE_TAB;
    return m;
}

/// Preset: Arrow keys + Z/X/A/S/Q/W/Enter/RShift (default layout).
inline SnesKeyMap snes_keymap_arrows() {
    return SnesKeyMap{};  // defaults are arrows
}

class SnesStandardController : public InputPeripheralDevice {
public:
    // Button bitmask — matches 16-bit parallel load bit order (MSB first)
    enum Button : uint16_t {
        RIGHT  = 0x0100,
        LEFT   = 0x0200,
        DOWN   = 0x0400,
        UP     = 0x0800,
        START  = 0x1000,
        SELECT = 0x2000,
        Y      = 0x4000,
        B      = 0x8000,
        R      = 0x0010,
        L      = 0x0020,
        X      = 0x0040,
        A      = 0x0080,
    };

    SnesStandardController();
    ~SnesStandardController() override = default;

    // --- PeripheralDevice interface ------------------------------------
    const char* get_name() const override { return "SNES Standard Controller"; }
    const char* get_id()   const override { return "snes_gamepad"; }
    ConnectorType get_connector_type() const override { return ConnectorType::CONTROLLER_NES; }
    void reset() override;

    // --- Signal protocol -----------------------------------------------
    void on_signal_change(uint32_t signal_state) override;
    uint32_t get_output_signals() const override { return output_signals_; }

    // --- Host Input (keyboard / gamepad) -------------------------------
    int get_supported_input_type_count() const override { return 2; }
    HostInputType get_supported_input_type(int index) const override {
        if (index == 0) return HostInputType::KEYBOARD;
        if (index == 1) return HostInputType::SDL_GAMEPAD;
        return HostInputType::NONE;
    }
    void set_host_input_binding(const HostInputBinding& binding) override;
    bool process_sdl_event(const SDL_Event& event) override;

    // --- Keymap preset support -----------------------------------------
    void apply_keymap_preset(int index) override;
    int get_active_keymap_preset() const override;

#ifdef CERMU_HAS_GUI
    void render_device_ui() override;
#endif

    // --- Controller-specific API ---------------------------------------
    void set_button_state(Button button, bool pressed);
    uint16_t get_button_state() const { return button_state_; }

    /// Set the keyboard-to-button mapping.
    void set_key_map(const SnesKeyMap& map) { keymap_ = map; }
    const SnesKeyMap& get_key_map() const { return keymap_; }

private:
    bool process_keyboard_event(const SDL_Event& event);
    bool process_gamepad_event(const SDL_Event& event);

    void on_input_source_will_change() override;
    const ControllerKeyMapPreset* get_keymap_presets_table() const override;
    int get_keymap_presets_table_size() const override;

    // 16-bit shift register (models two daisy-chained CD4021 chips)
    uint16_t shift_register_ = 0;
    uint16_t button_state_ = 0;           // Current button bitmask (active-high)
    bool latch_was_high_ = false;          // Edge detection for LATCH signal
    uint32_t output_signals_ = 0xFFFFFFFF; // Active-low output (all released)
    SnesKeyMap keymap_{};                  // Keyboard scancode → button mapping
};
