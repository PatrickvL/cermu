/**
 * joystick_device.cpp - Digital Joystick Peripheral Implementation
 */

#include "joystick_device.h"
#include "../device_registry.h"
#include <cstdio>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

// ============================================================================
// CONSTRUCTION / RESET
// ============================================================================

JoystickDevice::JoystickDevice()
    : state_(0xFFFFFFFF)  // All lines released (idle)
{
}

void JoystickDevice::reset() {
    state_ = 0xFFFFFFFF;
    if (port_) {
        port_->notify_device_output_changed(state_);
    }
}

// ============================================================================
// SIGNAL OUTPUT
// ============================================================================

uint32_t JoystickDevice::get_output_signals() const {
    return state_;
}

void JoystickDevice::set_direction(uint8_t bit_index, bool pressed) {
    if (pressed) {
        state_ &= ~(1u << bit_index);   // Pull LOW (assert)
    } else {
        state_ |= (1u << bit_index);    // Release HIGH
    }

    // Notify the connector port that our output changed
    if (port_) {
        port_->notify_device_output_changed(state_);
    }
}

// ============================================================================
// GUI
// ============================================================================

#ifdef IMGUI_VERSION
void JoystickDevice::render_device_ui() {
    ImGui::Text("Joystick State:");
    ImGui::Text("  UP:    %s", (state_ & (1u << ConnectorSignals::JOY_UP))    ? "[ ]" : "[X]");
    ImGui::Text("  DOWN:  %s", (state_ & (1u << ConnectorSignals::JOY_DOWN))  ? "[ ]" : "[X]");
    ImGui::Text("  LEFT:  %s", (state_ & (1u << ConnectorSignals::JOY_LEFT))  ? "[ ]" : "[X]");
    ImGui::Text("  RIGHT: %s", (state_ & (1u << ConnectorSignals::JOY_RIGHT)) ? "[ ]" : "[X]");
    ImGui::Text("  FIRE:  %s", (state_ & (1u << ConnectorSignals::JOY_FIRE))  ? "[ ]" : "[X]");
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor joystick_descriptor = {
    "joystick",
    "Joystick",
    "Standard digital joystick (Atari-compatible, 4 directions + fire)",
    ConnectorType::CONTROL_PORT_DB9,
    false  // Not a bus device
};

REGISTER_DEVICE(joystick_descriptor, []() {
    return std::make_unique<JoystickDevice>();
})
