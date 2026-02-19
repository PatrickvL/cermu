/**
 * joystick_device.cpp - Digital Joystick Peripheral Implementation
 */

#include "joystick_device.h"
#include "../../core/device_registry.h"
#include <cstdio>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

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
// HOST INPUT BINDING
// ============================================================================

void JoystickDevice::set_host_input_binding(const HostInputBinding& binding) {
    // Release all directions when switching input source
    release_all_signals();
    notify_port();

    binding_ = binding;
    printf("Joystick: Input source changed to %s\n", binding_.label.c_str());
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
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) return false;

    // Ignore key repeats
    if (event.type == SDL_KEYDOWN && event.key.repeat) return false;

    bool pressed = (event.type == SDL_KEYDOWN);
    SDL_Scancode sc = event.key.keysym.scancode;

    if (sc == key_map_.up)         { set_up(pressed);   return true; }
    if (sc == key_map_.down)       { set_down(pressed);  return true; }
    if (sc == key_map_.left)       { set_left(pressed);  return true; }
    if (sc == key_map_.right)      { set_right(pressed); return true; }
    if (sc == key_map_.fire)       { set_fire(pressed);  return true; }
    if (sc == key_map_.fire2)      { set_fire(pressed);  return true; }

    return false;
}

bool JoystickDevice::process_gamepad_event(const SDL_Event& event) {
    // Filter by gamepad instance ID if a specific one is bound
    auto get_instance_id = [&]() -> SDL_JoystickID {
        if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP)
            return event.cbutton.which;
        if (event.type == SDL_CONTROLLERAXISMOTION)
            return event.caxis.which;
        return -1;
    };

    SDL_JoystickID eid = get_instance_id();
    if (eid < 0) return false;

    // If bound to a specific gamepad, only accept events from it
    if (binding_.gamepad_instance_id >= 0 && eid != binding_.gamepad_instance_id)
        return false;

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
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        const int16_t THRESHOLD = 16384;  // 50% of max
        int16_t value = event.caxis.value;

        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
            set_left(value < -THRESHOLD);
            set_right(value > THRESHOLD);
            return true;
        }
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            set_up(value < -THRESHOLD);
            set_down(value > THRESHOLD);
            return true;
        }
    }

    return false;
}



// ============================================================================
// GUI
// ============================================================================

#ifdef IMGUI_VERSION
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

    // Input source info
    if (binding_.type == HostInputType::KEYBOARD) {
        ImGui::SameLine();
        ImGui::TextDisabled("[Keys]");

        // Key mapping preset selector
        static const char* presets[] = { "Arrows + RCtrl", "WASD + Space", "Numpad" };
        int current_preset = -1;
        // Detect current preset by checking up scancode
        if (key_map_.up == SDL_SCANCODE_UP)     current_preset = 0;
        else if (key_map_.up == SDL_SCANCODE_W)  current_preset = 1;
        else if (key_map_.up == SDL_SCANCODE_KP_8) current_preset = 2;

        int sel = current_preset;
        if (ImGui::Combo("Key Map", &sel, presets, IM_ARRAYSIZE(presets))) {
            switch (sel) {
                case 0: key_map_ = JoystickKeyMap(); break;          // Arrows+RCtrl
                case 1: key_map_ = joystick_keymap_wasd(); break;    // WASD+Space
                case 2: key_map_ = joystick_keymap_numpad(); break;  // Numpad
            }
        }
    } else if (binding_.type == HostInputType::SDL_GAMEPAD) {
        ImGui::SameLine();
        ImGui::TextDisabled("[Pad]");
    }
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
