/**
 * nes_standard_controller.cpp — NES Standard Controller Implementation
 *
 * Implements the NES-004 gamepad as a PeripheralDevice, using a CD4021
 * shift register for the serial LATCH/CLK/D0 protocol.
 *
 * Signal protocol (active-low on the bus):
 *   LATCH rising edge → latch button_state_ into CD4021
 *   CLK rising edge   → shift out one bit; drive D0 output
 *   D0 output         → active-low (bit CLEAR = button pressed)
 */

#include "nes_standard_controller.h"
#include "../../core/device_registry.h"

#include <cstdio>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>

using ConnectorSignals::NESControllerBit;

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

// ============================================================================
// CONSTRUCTION / RESET
// ============================================================================

NesStandardController::NesStandardController() {
    // Default binding: keyboard
    binding_.type  = HostInputType::KEYBOARD;
    binding_.label = "Keyboard";
}

void NesStandardController::reset() {
    button_state_   = 0;
    latch_was_high_ = false;
    output_signals_ = 0xFFFFFFFF;  // All signals released (active-low)
    cd4021_.reset();
}

// ============================================================================
// SIGNAL PROTOCOL
// ============================================================================

void NesStandardController::on_signal_change(uint32_t signal_state) {
    // Detect LATCH edge
    bool latch_now = (signal_state & (1u << NESControllerBit::NES_LATCH)) != 0;

    if (latch_now && !latch_was_high_) {
        // Rising edge of LATCH — capture button state into shift register
        // NES buttons are active-HIGH in our internal state, CD4021 latches directly
        cd4021_.latch(button_state_);
    }

    if (!latch_now && latch_was_high_) {
        // Falling edge of LATCH — first bit is already on D0 after latch
        // (hardware behavior: Q7 updates on latch)
    }

    latch_was_high_ = latch_now;

    // Detect CLK — when LATCH is LOW, each CLK high shifts out next bit
    bool clk_now = (signal_state & (1u << NESControllerBit::NES_CLK)) != 0;

    if (clk_now && !latch_now) {
        // Shift out next bit — discard (the read already happened on the bus side)
        cd4021_.shift_out();
    }

    // Drive D0 with the current MSB of the shift register
    // NES convention: bit 1 = button pressed, but D0 is active-low on the bus
    uint8_t current_bit = (cd4021_.get_shift_register() & 0x80) >> 7;

    if (current_bit) {
        // Button pressed → assert D0 (clear the bit = active-low)
        output_signals_ &= ~(1u << NESControllerBit::NES_D0);
    } else {
        // Button released → release D0 (set the bit)
        output_signals_ |= (1u << NESControllerBit::NES_D0);
    }
}

// ============================================================================
// BUTTON STATE
// ============================================================================

void NesStandardController::set_button_state(Button button, bool pressed) {
    if (pressed) {
        button_state_ |= static_cast<uint8_t>(button);
    } else {
        button_state_ &= ~static_cast<uint8_t>(button);
    }
}

// ============================================================================
// SDL EVENT PROCESSING
// ============================================================================

bool NesStandardController::process_sdl_event(const SDL_Event& event) {
    switch (binding_.type) {
        case HostInputType::KEYBOARD:    return process_keyboard_event(event);
        case HostInputType::SDL_GAMEPAD: return process_gamepad_event(event);
        default: return false;
    }
}

bool NesStandardController::process_keyboard_event(const SDL_Event& event) {
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) return false;
    if (event.type == SDL_KEYDOWN && event.key.repeat) return false;

    bool pressed = (event.type == SDL_KEYDOWN);
    SDL_Scancode sc = event.key.keysym.scancode;

    // Default mapping: Arrows + Z=B, X=A, Enter=START, RShift=SELECT
    if (sc == SDL_SCANCODE_UP)     { set_button_state(UP, pressed);     return true; }
    if (sc == SDL_SCANCODE_DOWN)   { set_button_state(DOWN, pressed);   return true; }
    if (sc == SDL_SCANCODE_LEFT)   { set_button_state(LEFT, pressed);   return true; }
    if (sc == SDL_SCANCODE_RIGHT)  { set_button_state(RIGHT, pressed);  return true; }
    if (sc == SDL_SCANCODE_Z)      { set_button_state(B, pressed);      return true; }
    if (sc == SDL_SCANCODE_X)      { set_button_state(A, pressed);      return true; }
    if (sc == SDL_SCANCODE_RETURN) { set_button_state(START, pressed);  return true; }
    if (sc == SDL_SCANCODE_RSHIFT) { set_button_state(SELECT, pressed); return true; }

    return false;
}

bool NesStandardController::process_gamepad_event(const SDL_Event& event) {
    auto get_instance_id = [&]() -> SDL_JoystickID {
        if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP)
            return event.cbutton.which;
        if (event.type == SDL_CONTROLLERAXISMOTION)
            return event.caxis.which;
        return -1;
    };

    SDL_JoystickID eid = get_instance_id();
    if (eid < 0) return false;

    if (binding_.gamepad_instance_id >= 0 && eid != binding_.gamepad_instance_id)
        return false;

    // Button events
    if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
        bool pressed = (event.type == SDL_CONTROLLERBUTTONDOWN);
        switch (event.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    set_button_state(UP, pressed);     return true;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  set_button_state(DOWN, pressed);   return true;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  set_button_state(LEFT, pressed);   return true;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: set_button_state(RIGHT, pressed);  return true;
            case SDL_CONTROLLER_BUTTON_A:          set_button_state(A, pressed);      return true;
            case SDL_CONTROLLER_BUTTON_B:          set_button_state(B, pressed);      return true;
            case SDL_CONTROLLER_BUTTON_START:      set_button_state(START, pressed);  return true;
            case SDL_CONTROLLER_BUTTON_BACK:       set_button_state(SELECT, pressed); return true;
            default: return false;
        }
    }

    // Axis events (left stick → digital directions)
    if (event.type == SDL_CONTROLLERAXISMOTION) {
        const int16_t THRESHOLD = 16384;
        int16_t value = event.caxis.value;

        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
            set_button_state(LEFT,  value < -THRESHOLD);
            set_button_state(RIGHT, value > THRESHOLD);
            return true;
        }
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            set_button_state(UP,   value < -THRESHOLD);
            set_button_state(DOWN, value > THRESHOLD);
            return true;
        }
    }

    return false;
}

// ============================================================================
// GUI
// ============================================================================

#ifdef IMGUI_VERSION
void NesStandardController::render_device_ui() {
    // Button state visualization
    bool a      = (button_state_ & A)      != 0;
    bool b      = (button_state_ & B)      != 0;
    bool sel    = (button_state_ & SELECT)  != 0;
    bool start  = (button_state_ & START)   != 0;
    bool up     = (button_state_ & UP)      != 0;
    bool down   = (button_state_ & DOWN)    != 0;
    bool left   = (button_state_ & LEFT)    != 0;
    bool right  = (button_state_ & RIGHT)   != 0;

    ImGui::Text("%s %s %s %s  %s %s  %s %s",
                up    ? "U" : ".", down  ? "D" : ".",
                left  ? "L" : ".", right ? "R" : ".",
                sel   ? "Se" : "..", start ? "St" : "..",
                b     ? "B" : ".", a     ? "A" : ".");

    if (binding_.type == HostInputType::KEYBOARD) {
        ImGui::SameLine();
        ImGui::TextDisabled("[Keys]");
    } else if (binding_.type == HostInputType::SDL_GAMEPAD) {
        ImGui::SameLine();
        ImGui::TextDisabled("[Pad]");
    }
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor nes_gamepad_descriptor = {
    "nes_gamepad",
    "NES Standard Controller",
    "Standard NES-004 gamepad with D-pad, Select, Start, B, and A buttons",
    ConnectorType::CONTROLLER_NES,
    false  // Not a bus device
};

REGISTER_DEVICE(nes_gamepad_descriptor, []() {
    return std::make_unique<NesStandardController>();
})
