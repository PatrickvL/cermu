#pragma once
/**
 * joystick_device.h - Digital Joystick Peripheral
 *
 * Emulates a standard Atari-compatible digital joystick that plugs into a
 * Control Port (DB-9).  Supports 4 directions + 1 fire button.  This covers
 * all Atari-style digital joysticks (Competition Pro, TAC-2, Quickshot, etc.)
 *
 * The device can be driven by:
 *   - Programmatic API (set_up/down/left/right/fire)
 *   - Host keyboard keys (configurable via JoystickKeyMap)
 *   - SDL GameController (physical gamepad/joystick via USB/BT)
 */

#include "../../core/peripherals/control_port_device.h"

/**
 * Digital joystick device (Atari-compatible).
 *
 * DirectionBit layout matches ConnectorSignals::ControlPortBit:
 *   bit 0 = UP, bit 1 = DOWN, bit 2 = LEFT, bit 3 = RIGHT, bit 6 = FIRE
 */
class JoystickDevice : public ControlPortInputDevice {
public:
    JoystickDevice();
    ~JoystickDevice() override = default;

    // --- PeripheralDevice interface ------------------------------------
    const char* get_name() const override { return "Digital Joystick"; }
    const char* get_id() const override   { return "joystick"; }
    void reset() override;

    // --- Host Input support --------------------------------------------
    int get_supported_input_type_count() const override { return 2; }
    HostInputType get_supported_input_type(int index) const override {
        if (index == 0) return HostInputType::KEYBOARD;
        if (index == 1) return HostInputType::SDL_GAMEPAD;
        return HostInputType::NONE;
    }
    const HostInputBinding& get_host_input_binding() const override { return binding_; }
    void set_host_input_binding(const HostInputBinding& binding) override;
    bool process_sdl_event(const SDL_Event& event) override;

    // --- Keymap preset support -----------------------------------------
    int get_keymap_preset_count() const override;
    const ControllerKeyMapPreset& get_keymap_preset(int index) const override;
    void apply_keymap_preset(int index) override;
    int get_active_keymap_preset() const override;

#ifdef CERMU_HAS_GUI
    void render_device_ui() override;
    void render_input_source_settings_ui() override;
#endif

    // --- Joystick-specific API -----------------------------------------

    /// Set a direction/button state.  `pressed` = true means the switch is closed (line pulled LOW).
    void set_direction(uint8_t bit_index, bool pressed) { set_signal(bit_index, pressed); }

    /// Convenience setters matching common controller events.
    void set_up(bool pressed)    { set_signal(ConnectorSignals::JOY_UP,    pressed); }
    void set_down(bool pressed)  { set_signal(ConnectorSignals::JOY_DOWN,  pressed); }
    void set_left(bool pressed)  { set_signal(ConnectorSignals::JOY_LEFT,  pressed); }
    void set_right(bool pressed) { set_signal(ConnectorSignals::JOY_RIGHT, pressed); }
    void set_fire(bool pressed)  { set_signal(ConnectorSignals::JOY_FIRE,  pressed); }

    /// Get raw button state (active-low: 0 = pressed).
    uint32_t get_state() const { return signal_state_; }

    /// Get/set the keyboard mapping (only used when binding type == KEYBOARD).
    const JoystickKeyMap& get_key_map() const { return key_map_; }
    void set_key_map(const JoystickKeyMap& map) { key_map_ = map; }

private:
    HostInputBinding binding_;    ///< Current host input binding
    JoystickKeyMap key_map_;      ///< Keyboard scancode mapping

    // SDL event handlers for each input type
    bool process_keyboard_event(const SDL_Event& event);
    bool process_gamepad_event(const SDL_Event& event);
};
