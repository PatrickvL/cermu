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

#include "core/cermu.hpp"
#include "devices/input/nes_standard_controller.hpp"
#include "core/device_registry.hpp"

#include <cstdio>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>

using PortSignals::NESControllerBit;

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
// HOST INPUT BINDING
// ============================================================================

void NesStandardController::set_host_input_binding(const HostInputBinding& binding) {
    on_input_source_will_change();
    binding_ = binding;
    log_info("NES Controller: Input source changed to %s\n", binding_.label.c_str());
}

void NesStandardController::on_input_source_will_change() {
    button_state_ = 0;
    output_signals_ = 0xFFFFFFFF;
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

    // Keep Port cache in sync so read_signals() returns fresh D0
    if (port_) port_->notify_device_output_changed(output_signals_);
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
    SDL_Scancode sc;
    bool pressed;
    if (!extract_key_event(event, sc, pressed)) return false;

    // Configurable mapping via NesKeyMap (default: Arrows + Z/X/Enter/RShift)
    if (sc == keymap_.up)     { set_button_state(UP, pressed);     return true; }
    if (sc == keymap_.down)   { set_button_state(DOWN, pressed);   return true; }
    if (sc == keymap_.left)   { set_button_state(LEFT, pressed);   return true; }
    if (sc == keymap_.right)  { set_button_state(RIGHT, pressed);  return true; }
    if (sc == keymap_.b)      { set_button_state(B, pressed);      return true; }
    if (sc == keymap_.a)      { set_button_state(A, pressed);      return true; }
    if (sc == keymap_.start)  { set_button_state(START, pressed);  return true; }
    if (sc == keymap_.select) { set_button_state(SELECT, pressed); return true; }

    return false;
}

bool NesStandardController::process_gamepad_event(const SDL_Event& event) {
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

static const ControllerKeyMapPreset nes_presets[] = {
    { "WASD + Space/LShift/Enter/Tab",
      { SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D,
        SDL_SCANCODE_SPACE, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RETURN, SDL_SCANCODE_TAB },
      8, false },
    { "IJKL + ./,/;/O",
      { SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
        SDL_SCANCODE_PERIOD, SDL_SCANCODE_COMMA, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_O },
      8, false },
    { "Arrows + X/Z/Enter/RShift",
      { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_X, SDL_SCANCODE_Z, SDL_SCANCODE_RETURN, SDL_SCANCODE_RSHIFT },
      8, false },
};

static constexpr int NES_PRESET_COUNT = static_cast<int>(
    sizeof(nes_presets) / sizeof(nes_presets[0]));

const ControllerKeyMapPreset* NesStandardController::get_keymap_presets_table() const {
    return nes_presets;
}

int NesStandardController::get_keymap_presets_table_size() const {
    return NES_PRESET_COUNT;
}

void NesStandardController::apply_keymap_preset(int index) {
    switch (index) {
        case 0: keymap_ = nes_keymap_wasd();   break;
        case 1: keymap_ = nes_keymap_ijkl();   break;
        case 2: keymap_ = nes_keymap_arrows();  break;
        default: break;
    }
}

int NesStandardController::get_active_keymap_preset() const {
    if (keymap_.up == SDL_SCANCODE_W)  return 0;
    if (keymap_.up == SDL_SCANCODE_I)  return 1;
    if (keymap_.up == SDL_SCANCODE_UP) return 2;
    return -1;
}

// ============================================================================
// GUI
// ============================================================================

#ifdef CERMU_HAS_GUI
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

    render_input_source_badge();
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor nes_gamepad_descriptor = {
    "nes_gamepad",
    "NES Standard Controller",
    "Standard NES-004 gamepad with D-pad, Select, Start, B, and A buttons",
    PortType::CONTROLLER_NES,
    false  // Not a bus device
};

REGISTER_DEVICE(nes_gamepad_descriptor, []() {
    return std::make_unique<NesStandardController>();
})
