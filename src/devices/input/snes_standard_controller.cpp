/**
 * snes_standard_controller.cpp — SNES Standard Controller Implementation
 *
 * Implements the SNS-005 gamepad as a PeripheralDevice, using a 16-bit
 * shift register for the serial LATCH/CLK/D0 protocol.  Equivalent to
 * two daisy-chained CD4021 shift registers as in the real hardware.
 *
 * Signal protocol (active-low on the bus):
 *   LATCH rising edge → latch button_state_ into shift register
 *   CLK rising edge   → shift out one bit; drive D0 output
 *   D0 output         → active-low (bit CLEAR = button pressed)
 *
 * Compatible with NES controller ports — the SNES controller uses the
 * same LATCH/CLK/D0 signals but shifts out 16 bits instead of 8.
 */

#include "core/cermu.hpp"
#include "devices/input/snes_standard_controller.hpp"
#include "core/device_registry.hpp"

#include <cstdio>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>

using PortSignals::NESControllerBit;

// ============================================================================
// CONSTRUCTION / RESET
// ============================================================================

SnesStandardController::SnesStandardController() {
    // Default binding: keyboard
    binding_.type  = HostInputType::KEYBOARD;
    binding_.label = "Keyboard";
}

void SnesStandardController::reset() {
    button_state_   = 0;
    shift_register_ = 0;
    latch_was_high_ = false;
    output_signals_ = 0xFFFFFFFF;  // All signals released (active-low)
}

// ============================================================================
// HOST INPUT BINDING
// ============================================================================

void SnesStandardController::set_host_input_binding(const HostInputBinding& binding) {
    on_input_source_will_change();
    binding_ = binding;
    log_info("SNES Controller: Input source changed to %s\n", binding_.label.c_str());
}

void SnesStandardController::on_input_source_will_change() {
    button_state_ = 0;
    output_signals_ = 0xFFFFFFFF;
}

// ============================================================================
// SIGNAL PROTOCOL
// ============================================================================

void SnesStandardController::on_signal_change(uint32_t signal_state) {
    // Detect LATCH edge
    bool latch_now = (signal_state & (1u << NESControllerBit::NES_LATCH)) != 0;

    if (latch_now && !latch_was_high_) {
        // Rising edge of LATCH — capture button state into 16-bit shift register
        // Bits 3-0 are always 0 (unused signature bits)
        shift_register_ = button_state_ & 0xFFF0;
    }

    if (!latch_now && latch_was_high_) {
        // Falling edge of LATCH — first bit is already on D0 after latch
    }

    latch_was_high_ = latch_now;

    // Detect CLK — when LATCH is LOW, each CLK high shifts out next bit
    bool clk_now = (signal_state & (1u << NESControllerBit::NES_CLK)) != 0;

    if (clk_now && !latch_now) {
        // Shift out MSB, shift in 0 from the bottom
        shift_register_ <<= 1;
    }

    // Drive D0 with the current MSB of the shift register
    // SNES convention: bit 1 = button pressed, but D0 is active-low on the bus
    uint8_t current_bit = (shift_register_ & 0x8000) >> 15;

    if (current_bit) {
        // Button pressed → assert D0 (clear the bit = active-low)
        output_signals_ &= ~(1u << NESControllerBit::NES_D0);
    } else {
        // Button released → release D0 (set the bit)
        output_signals_ |= (1u << NESControllerBit::NES_D0);
    }

    // Keep Port cache in sync so read_signals() returns fresh D0
    if (port_) port_->notify_device_output_changed(output_signals_);
}

// ============================================================================
// BUTTON STATE
// ============================================================================

void SnesStandardController::set_button_state(Button button, bool pressed) {
    if (pressed) {
        button_state_ |= static_cast<uint16_t>(button);
    } else {
        button_state_ &= ~static_cast<uint16_t>(button);
    }
}

// ============================================================================
// SDL EVENT PROCESSING
// ============================================================================

bool SnesStandardController::process_sdl_event(const SDL_Event& event) {
    switch (binding_.type) {
        case HostInputType::KEYBOARD:    return process_keyboard_event(event);
        case HostInputType::SDL_GAMEPAD: return process_gamepad_event(event);
        default: return false;
    }
}

bool SnesStandardController::process_keyboard_event(const SDL_Event& event) {
    SDL_Scancode sc;
    bool pressed;
    if (!extract_key_event(event, sc, pressed)) return false;

    // Configurable mapping via SnesKeyMap
    if (sc == keymap_.up)     { set_button_state(UP, pressed);     return true; }
    if (sc == keymap_.down)   { set_button_state(DOWN, pressed);   return true; }
    if (sc == keymap_.left)   { set_button_state(LEFT, pressed);   return true; }
    if (sc == keymap_.right)  { set_button_state(RIGHT, pressed);  return true; }
    if (sc == keymap_.b)      { set_button_state(B, pressed);      return true; }
    if (sc == keymap_.a)      { set_button_state(A, pressed);      return true; }
    if (sc == keymap_.x)      { set_button_state(X, pressed);      return true; }
    if (sc == keymap_.y)      { set_button_state(Y, pressed);      return true; }
    if (sc == keymap_.l)      { set_button_state(L, pressed);      return true; }
    if (sc == keymap_.r)      { set_button_state(R, pressed);      return true; }
    if (sc == keymap_.start)  { set_button_state(START, pressed);  return true; }
    if (sc == keymap_.select) { set_button_state(SELECT, pressed); return true; }

    return false;
}

bool SnesStandardController::process_gamepad_event(const SDL_Event& event) {
    if (!should_accept_gamepad_event(event)) return false;

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
            case SDL_CONTROLLER_BUTTON_X:          set_button_state(X, pressed);      return true;
            case SDL_CONTROLLER_BUTTON_Y:          set_button_state(Y, pressed);      return true;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  set_button_state(L, pressed);   return true;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: set_button_state(R, pressed);   return true;
            case SDL_CONTROLLER_BUTTON_START:      set_button_state(START, pressed);  return true;
            case SDL_CONTROLLER_BUTTON_BACK:       set_button_state(SELECT, pressed); return true;
            default: return false;
        }
    }

    // Axis events (left stick → digital directions)
    DigitalAxes axes;
    if (axis_to_digital(event, axes)) {
        set_button_state(LEFT,  axes.left);
        set_button_state(RIGHT, axes.right);
        set_button_state(UP,    axes.up);
        set_button_state(DOWN,  axes.down);
        return true;
    }

    return false;
}

// ============================================================================
// KEYMAP PRESETS
// ============================================================================

static const ControllerKeyMapPreset snes_presets[] = {
    { "WASD + K/,/L/./I/O/Enter/Tab",
      { SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D,
        SDL_SCANCODE_PERIOD, SDL_SCANCODE_COMMA, SDL_SCANCODE_L, SDL_SCANCODE_K,
        SDL_SCANCODE_I, SDL_SCANCODE_O, SDL_SCANCODE_RETURN, SDL_SCANCODE_TAB },
      12, false },
    { "Arrows + X/Z/S/A/Q/W/Enter/RShift",
      { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_X, SDL_SCANCODE_Z, SDL_SCANCODE_S, SDL_SCANCODE_A,
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_RETURN, SDL_SCANCODE_RSHIFT },
      12, false },
};

static constexpr int SNES_PRESET_COUNT = static_cast<int>(
    sizeof(snes_presets) / sizeof(snes_presets[0]));

const ControllerKeyMapPreset* SnesStandardController::get_keymap_presets_table() const {
    return snes_presets;
}

int SnesStandardController::get_keymap_presets_table_size() const {
    return SNES_PRESET_COUNT;
}

void SnesStandardController::apply_keymap_preset(int index) {
    switch (index) {
        case 0: keymap_ = snes_keymap_wasd();   break;
        case 1: keymap_ = snes_keymap_arrows();  break;
        default: break;
    }
}

int SnesStandardController::get_active_keymap_preset() const {
    if (keymap_.up == SDL_SCANCODE_W)  return 0;
    if (keymap_.up == SDL_SCANCODE_UP) return 1;
    return -1;
}

// ============================================================================
// GUI
// ============================================================================

#ifdef CERMU_HAS_GUI
void SnesStandardController::render_device_ui() {
    // Button state visualization
    bool a      = (button_state_ & A)      != 0;
    bool b      = (button_state_ & B)      != 0;
    bool x      = (button_state_ & X)      != 0;
    bool y      = (button_state_ & Y)      != 0;
    bool l      = (button_state_ & L)      != 0;
    bool r      = (button_state_ & R)      != 0;
    bool sel    = (button_state_ & SELECT)  != 0;
    bool start  = (button_state_ & START)   != 0;
    bool up     = (button_state_ & UP)      != 0;
    bool down   = (button_state_ & DOWN)    != 0;
    bool left   = (button_state_ & LEFT)    != 0;
    bool right  = (button_state_ & RIGHT)   != 0;

    ImGui::Text("%s %s %s %s  %s %s  %s %s  %s %s  %s %s",
                up    ? "U" : ".", down  ? "D" : ".",
                left  ? "L" : ".", right ? "R" : ".",
                sel   ? "Se" : "..", start ? "St" : "..",
                y     ? "Y" : ".", b     ? "B" : ".",
                x     ? "X" : ".", a     ? "A" : ".",
                l     ? "L" : ".", r     ? "R" : ".");

    render_input_source_badge();
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor snes_gamepad_descriptor = {
    "snes_gamepad",
    "SNES Standard Controller",
    "Standard SNS-005 gamepad with D-pad, Select, Start, B, A, X, Y, L, and R buttons",
    PortType::CONTROLLER_NES,
    false  // Not a bus device
};

REGISTER_DEVICE(snes_gamepad_descriptor, []() {
    return std::make_unique<SnesStandardController>();
})
