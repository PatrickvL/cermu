#pragma once
/*
 * nes_standard_controller.h — NES Standard Controller Peripheral
 *
 * Models the standard NES gamepad (NES-004) as a PeripheralDevice.
 * Internally composes a CD4021 shift register for the serial
 * LATCH/CLK/D0 protocol used on the NES controller port.
 *
 * Lives in src/devices/input/ because NES controller ports appear on
 * NES, Famicom, VS. System, and PlayChoice-10.
 *
 * Button bitmask layout (MSB → LSB, matching hardware shift order):
 *   A  B  SELECT  START  UP  DOWN  LEFT  RIGHT
 *   7  6    5       4     3    2     1      0
 */

#include "core/peripherals/input_peripheral_device.h"
#include "chip/input/cd4021.h"

#include <cstdint>
#include <SDL_scancode.h>

// ============================================================================
// NES KEYBOARD MAP
// ============================================================================

/**
 * Maps host keyboard scancodes to NES controller buttons.
 * Uses scancodes (layout-independent) like JoystickKeyMap.
 */
struct NesKeyMap {
    SDL_Scancode up     = SDL_SCANCODE_UP;
    SDL_Scancode down   = SDL_SCANCODE_DOWN;
    SDL_Scancode left   = SDL_SCANCODE_LEFT;
    SDL_Scancode right  = SDL_SCANCODE_RIGHT;
    SDL_Scancode a      = SDL_SCANCODE_X;
    SDL_Scancode b      = SDL_SCANCODE_Z;
    SDL_Scancode start  = SDL_SCANCODE_RETURN;
    SDL_Scancode select = SDL_SCANCODE_RSHIFT;
};

/// Preset: WASD + Space/LShift/Enter/Tab (Player 1).
inline NesKeyMap nes_keymap_wasd() {
    NesKeyMap m;
    m.up     = SDL_SCANCODE_W;
    m.down   = SDL_SCANCODE_S;
    m.left   = SDL_SCANCODE_A;
    m.right  = SDL_SCANCODE_D;
    m.a      = SDL_SCANCODE_SPACE;
    m.b      = SDL_SCANCODE_LSHIFT;
    m.start  = SDL_SCANCODE_RETURN;
    m.select = SDL_SCANCODE_TAB;
    return m;
}

/// Preset: IJKL + Period/Comma/Semicolon/O (Player 2).
inline NesKeyMap nes_keymap_ijkl() {
    NesKeyMap m;
    m.up     = SDL_SCANCODE_I;
    m.down   = SDL_SCANCODE_K;
    m.left   = SDL_SCANCODE_J;
    m.right  = SDL_SCANCODE_L;
    m.a      = SDL_SCANCODE_PERIOD;
    m.b      = SDL_SCANCODE_COMMA;
    m.start  = SDL_SCANCODE_SEMICOLON;
    m.select = SDL_SCANCODE_O;
    return m;
}

/// Preset: Arrow keys + Z/X/Enter/RShift (legacy default).
inline NesKeyMap nes_keymap_arrows() {
    return NesKeyMap{};  // defaults are arrows
}

class NesStandardController : public InputPeripheralDevice {
public:
    // Button bitmask — matches CD4021 parallel load bit order
    enum Button : uint8_t {
        RIGHT  = 0x01,
        LEFT   = 0x02,
        DOWN   = 0x04,
        UP     = 0x08,
        START  = 0x10,
        SELECT = 0x20,
        B      = 0x40,
        A      = 0x80,
    };

    NesStandardController();
    ~NesStandardController() override = default;

    // --- PeripheralDevice interface ------------------------------------
    const char* get_name() const override { return "NES Standard Controller"; }
    const char* get_id()   const override { return "nes_gamepad"; }
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
    uint8_t get_button_state() const { return button_state_; }

    /// Set the keyboard-to-button mapping (default: arrows + Z/X/Enter/RShift).
    void set_key_map(const NesKeyMap& map) { keymap_ = map; }
    const NesKeyMap& get_key_map() const { return keymap_; }

private:
    bool process_keyboard_event(const SDL_Event& event);
    bool process_gamepad_event(const SDL_Event& event);

    void on_input_source_will_change() override;
    const ControllerKeyMapPreset* get_keymap_presets_table() const override;
    int get_keymap_presets_table_size() const override;

    CD4021 cd4021_;                         // Internal shift register
    uint8_t button_state_ = 0;             // Current button bitmask (active-high)
    bool latch_was_high_ = false;          // Edge detection for LATCH signal
    uint32_t output_signals_ = 0xFFFFFFFF; // Active-low output (all released)
    NesKeyMap keymap_{};                   // Keyboard scancode → button mapping
};
