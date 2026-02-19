/**
 * mouse_device.cpp - Proportional Mouse (1351) Implementation
 */

#include "mouse_device.h"
#include "../device_registry.h"

MouseDevice::MouseDevice()
    : state_(0xFFFFFFFF)
    , pot_x_(128)
    , pot_y_(128)
{
}

void MouseDevice::reset() {
    state_ = 0xFFFFFFFF;
    pot_x_ = 128;
    pot_y_ = 128;
    if (port_) port_->notify_device_output_changed(state_);
}

uint32_t MouseDevice::get_output_signals() const {
    return state_;
}

void MouseDevice::set_left_button(bool pressed) {
    // Left button maps to FIRE (same as joystick fire button)
    if (pressed) {
        state_ &= ~(1u << ConnectorSignals::JOY_FIRE);
    } else {
        state_ |= (1u << ConnectorSignals::JOY_FIRE);
    }
    if (port_) port_->notify_device_output_changed(state_);
}

void MouseDevice::set_right_button(bool pressed) {
    // Right button maps to UP line on the control port
    if (pressed) {
        state_ &= ~(1u << ConnectorSignals::JOY_UP);
    } else {
        state_ |= (1u << ConnectorSignals::JOY_UP);
    }
    if (port_) port_->notify_device_output_changed(state_);
}

void MouseDevice::move(int dx, int dy) {
    // The 1351 encodes movement in the low 6 bits of the SID POT registers.
    // Each movement delta changes the pot value proportionally.
    // Here we accumulate into the pot_x_/pot_y_ counters (wrap at 0-255).
    pot_x_ = static_cast<uint8_t>((pot_x_ + dx) & 0xFF);
    pot_y_ = static_cast<uint8_t>((pot_y_ + dy) & 0xFF);
    // POT values are communicated via the POTX/POTY analog lines;
    // the system wrapper reads these via get_pot_x()/get_pot_y().
}

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor mouse_descriptor = {
    "mouse_1351",
    "Mouse (1351)",
    "Commodore 1351 proportional mouse — uses SID POT inputs for position",
    ConnectorType::CONTROL_PORT_DB9,
    false
};

REGISTER_DEVICE(mouse_descriptor, []() {
    return std::make_unique<MouseDevice>();
})
