/**
 * joystick_device.cpp - Digital Joystick Peripheral Implementation
 */

#include "joystick_device.h"
#include "../../core/device_registry.h"
#include <cstdio>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>

#ifdef CERMU_HAS_GUI
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

int JoystickDevice::get_keymap_preset_count() const {
    return JOYSTICK_PRESET_COUNT;
}

const ControllerKeyMapPreset& JoystickDevice::get_keymap_preset(int index) const {
    if (index >= 0 && index < JOYSTICK_PRESET_COUNT) return joystick_presets[index];
    static const ControllerKeyMapPreset empty{"None", {}, 0, false};
    return empty;
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

    // Input source info
    if (binding_.type == HostInputType::KEYBOARD) {
        ImGui::SameLine();
        ImGui::TextDisabled("[Keys]");

        // Keymap preset selector with collision info
        int active = get_active_keymap_preset();
        const char* preview = (active >= 0) ? get_keymap_preset(active).name : "Custom";

        if (ImGui::BeginCombo("Key Map", preview)) {
            for (int i = 0; i < get_keymap_preset_count(); i++) {
                const auto& preset = get_keymap_preset(i);
                int collisions = count_keymap_collisions(preset, guest_keyboard_scancodes_);

                char label[128];
                if (collisions > 0) {
                    snprintf(label, sizeof(label), "%s  (%d collision%s)",
                             preset.name, collisions, collisions > 1 ? "s" : "");
                } else {
                    snprintf(label, sizeof(label), "%s", preset.name);
                }

                bool selected = (i == active);
                if (ImGui::Selectable(label, selected)) {
                    apply_keymap_preset(i);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
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
