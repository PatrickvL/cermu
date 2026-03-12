/**
 * joystick_device.cpp - Digital Joystick Peripheral Implementation
 */

#include "devices/input/joystick_device.hpp"
#include "core/device_registry.hpp"
#include <cstdio>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>

// ============================================================================
// CONSTRUCTION / RESET
// ============================================================================

JoystickDevice::JoystickDevice() {
    // Default binding: Keyboard (arrows + right ctrl)
    binding_.type = HostInputType::KEYBOARD;
    binding_.label = "Keyboard";
}

void JoystickDevice::reset() {
    ControlPortInputDevice::reset();
}

// ============================================================================
// SDL EVENT PROCESSING
// ============================================================================

bool JoystickDevice::process_sdl_event(const SDL_Event& event) {
    switch (binding_.type) {
        case HostInputType::KEYBOARD:  return process_keyboard_event(event);
        case HostInputType::SDL_GAMEPAD: return process_gamepad_event(event);
        default: return false;
    }
}

bool JoystickDevice::process_keyboard_event(const SDL_Event& event) {
    SDL_Scancode sc;
    bool pressed;
    if (!extract_key_event(event, sc, pressed)) return false;

    if (sc == key_map_.up)         { set_up(pressed);   return true; }
    if (sc == key_map_.down)       { set_down(pressed);  return true; }
    if (sc == key_map_.left)       { set_left(pressed);  return true; }
    if (sc == key_map_.right)      { set_right(pressed); return true; }
    if (sc == key_map_.fire)       { set_fire(pressed);  return true; }
    if (sc == key_map_.fire2)      { set_fire(pressed);  return true; }

    return false;
}

bool JoystickDevice::process_gamepad_event(const SDL_Event& event) {
    if (!should_accept_gamepad_event(event)) return false;

    // Button events
    if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
        bool pressed = (event.type == SDL_CONTROLLERBUTTONDOWN);
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    set_up(pressed);   return true;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  set_down(pressed); return true;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  set_left(pressed); return true;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: set_right(pressed);return true;
            case SDL_CONTROLLER_BUTTON_A:          set_fire(pressed); return true;
            case SDL_CONTROLLER_BUTTON_B:          set_fire(pressed); return true;
            default: return false;
        }
    }

    // Axis events (left stick → digital directions, threshold 50%)
    DigitalAxes axes;
    if (axis_to_digital(event, axes)) {
        set_left(axes.left);
        set_right(axes.right);
        set_up(axes.up);
        set_down(axes.down);
        return true;
    }

    return false;
}

// ============================================================================
// KEYMAP PRESETS
// ============================================================================

static const ControllerKeyMapPreset joystick_presets[] = {
    { "Numpad",
      { SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_4,
        SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_ENTER },
      6, true },
    { "Arrows + RCtrl",
      { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT, SDL_SCANCODE_RCTRL, SDL_SCANCODE_RSHIFT },
      6, false },
    { "WASD + Space",
      { SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A,
        SDL_SCANCODE_D, SDL_SCANCODE_SPACE, SDL_SCANCODE_LCTRL },
      6, false },
};

static constexpr int JOYSTICK_PRESET_COUNT = static_cast<int>(
    sizeof(joystick_presets) / sizeof(joystick_presets[0]));

const ControllerKeyMapPreset* JoystickDevice::get_keymap_presets_table() const {
    return joystick_presets;
}

int JoystickDevice::get_keymap_presets_table_size() const {
    return JOYSTICK_PRESET_COUNT;
}

void JoystickDevice::apply_keymap_preset(int index) {
    switch (index) {
        case 0:  key_map_ = joystick_keymap_numpad(); break;
        case 1:  key_map_ = JoystickKeyMap();         break;  // Arrows+RCtrl (default)
        case 2:  key_map_ = joystick_keymap_wasd();   break;
        default: break;
    }
}

int JoystickDevice::get_active_keymap_preset() const {
    if (key_map_.up == SDL_SCANCODE_KP_8) return 0;
    if (key_map_.up == SDL_SCANCODE_UP)   return 1;
    if (key_map_.up == SDL_SCANCODE_W)    return 2;
    return -1;
}

// ============================================================================
// GUI
// ============================================================================

#ifdef CERMU_HAS_GUI
void JoystickDevice::render_device_ui() {
    // Joystick state display
    bool up    = is_signal_asserted(ConnectorSignals::JOY_UP);
    bool down  = is_signal_asserted(ConnectorSignals::JOY_DOWN);
    bool left  = is_signal_asserted(ConnectorSignals::JOY_LEFT);
    bool right = is_signal_asserted(ConnectorSignals::JOY_RIGHT);
    bool fire  = is_signal_asserted(ConnectorSignals::JOY_FIRE);

    ImGui::Text("  %s %s %s %s %s",
                up ? "U" : ".", down ? "D" : ".", left ? "L" : ".",
                right ? "R" : ".", fire ? "F" : ".");

    render_input_source_badge();
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor joystick_descriptor = {
    "joystick",
    "Digital Joystick",
    "Atari-compatible digital joystick (Competition Pro, TAC-2, etc.)",
    ConnectorType::CONTROL_PORT_DB9,
    false  // Not a bus device
};

REGISTER_DEVICE(joystick_descriptor, []() {
    return std::make_unique<JoystickDevice>();
})
