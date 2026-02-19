#pragma once
/**
 * host_input.h - Host Input Provider Framework
 *
 * Defines how emulated peripheral devices receive input from the host machine.
 * Each input-accepting device (joystick, mouse, lightpen, paddles) can be bound
 * to a host input source:
 *
 *   HostInputType::KEYBOARD     — Host keyboard keys (configurable mapping)
 *   HostInputType::SDL_GAMEPAD  — Physical game controller via SDL GameController
 *   HostInputType::HOST_MOUSE   — Host mouse position + buttons
 *
 * The GUI layer routes SDL events to devices via PeripheralDevice::process_sdl_event().
 * Each device interprets events according to its current binding.
 *
 * Joystick keyboard mappings default to cursor keys + Right Ctrl (fire),
 * matching the common C64 configuration.  These can be customised per-device
 * through render_device_ui().
 */

#include <SDL_keycode.h>
#include <SDL_scancode.h>
#include <cstdint>
#include <string>

// ============================================================================
// HOST INPUT TYPE
// ============================================================================

/// What kind of host hardware drives an emulated input device.
enum class HostInputType {
    NONE,           ///< No host input — device state is set programmatically
    KEYBOARD,       ///< Host keyboard keys (configurable key map)
    SDL_GAMEPAD,    ///< SDL GameController (USB/BT gamepad, joystick)
    HOST_MOUSE,     ///< Host mouse (position + buttons)
};

/// Human-readable label for a HostInputType.
inline const char* host_input_type_name(HostInputType type) {
    switch (type) {
        case HostInputType::NONE:        return "None";
        case HostInputType::KEYBOARD:    return "Keyboard";
        case HostInputType::SDL_GAMEPAD: return "Game Controller";
        case HostInputType::HOST_MOUSE:  return "Host Mouse";
        default:                         return "Unknown";
    }
}

// ============================================================================
// HOST INPUT BINDING
// ============================================================================

/**
 * Describes which host input source is currently driving an emulated device.
 */
struct HostInputBinding {
    HostInputType type = HostInputType::NONE;

    /// SDL GameController/joystick instance ID (from SDL_CONTROLLERDEVICEADDED).
    /// Only meaningful when type == SDL_GAMEPAD.  -1 = any/auto.
    int gamepad_instance_id = -1;

    /// Human-readable label for current binding (e.g. "Keyboard (Arrows+RCtrl)").
    std::string label = "None";
};

// ============================================================================
// JOYSTICK KEYBOARD MAP
// ============================================================================

/**
 * Maps host keyboard scancodes to joystick directions + fire.
 * Uses scancodes (not keycodes) so the mapping is layout-independent.
 */
struct JoystickKeyMap {
    SDL_Scancode up    = SDL_SCANCODE_UP;
    SDL_Scancode down  = SDL_SCANCODE_DOWN;
    SDL_Scancode left  = SDL_SCANCODE_LEFT;
    SDL_Scancode right = SDL_SCANCODE_RIGHT;
    SDL_Scancode fire  = SDL_SCANCODE_RCTRL;

    /// Optional second fire button.
    SDL_Scancode fire2 = SDL_SCANCODE_RSHIFT;

    /// Human-readable description of this mapping.
    const char* description() const {
        // Static — only one mapping description shown at a time
        static char buf[128];
        snprintf(buf, sizeof(buf), "Arrows + RCtrl/RShift");
        return buf;
    }
};

/// Preset: WASD + Space fire.
inline JoystickKeyMap joystick_keymap_wasd() {
    JoystickKeyMap m;
    m.up    = SDL_SCANCODE_W;
    m.down  = SDL_SCANCODE_S;
    m.left  = SDL_SCANCODE_A;
    m.right = SDL_SCANCODE_D;
    m.fire  = SDL_SCANCODE_SPACE;
    m.fire2 = SDL_SCANCODE_LCTRL;
    return m;
}

/// Preset: Numpad (8/2/4/6 + 0).
inline JoystickKeyMap joystick_keymap_numpad() {
    JoystickKeyMap m;
    m.up    = SDL_SCANCODE_KP_8;
    m.down  = SDL_SCANCODE_KP_2;
    m.left  = SDL_SCANCODE_KP_4;
    m.right = SDL_SCANCODE_KP_6;
    m.fire  = SDL_SCANCODE_KP_0;
    m.fire2 = SDL_SCANCODE_KP_ENTER;
    return m;
}

