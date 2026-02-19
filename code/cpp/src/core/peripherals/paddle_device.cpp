/**
 * paddle_device.cpp - Paddle Controller Implementation
 */

#include "paddle_device.h"
#include "../device_registry.h"

PaddleDevice::PaddleDevice()
    : state_(0xFFFFFFFF), pot_x_(128), pot_y_(128) {}

void PaddleDevice::reset() {
    state_ = 0xFFFFFFFF;
    pot_x_ = 128;
    pot_y_ = 128;
    if (port_) port_->notify_device_output_changed(state_);
}

uint32_t PaddleDevice::get_output_signals() const { return state_; }

void PaddleDevice::set_fire_x(bool pressed) {
    // Paddle A fire maps to FIRE line
    if (pressed) state_ &= ~(1u << ConnectorSignals::JOY_FIRE);
    else         state_ |=  (1u << ConnectorSignals::JOY_FIRE);
    if (port_) port_->notify_device_output_changed(state_);
}

void PaddleDevice::set_fire_y(bool pressed) {
    // Paddle B fire maps to UP line (active-low)
    if (pressed) state_ &= ~(1u << ConnectorSignals::JOY_UP);
    else         state_ |=  (1u << ConnectorSignals::JOY_UP);
    if (port_) port_->notify_device_output_changed(state_);
}

static const DeviceDescriptor paddle_descriptor = {
    "paddles",
    "Paddles",
    "Commodore paddle controller pair — analog potentiometers + fire buttons",
    ConnectorType::CONTROL_PORT_DB9,
    false
};

REGISTER_DEVICE(paddle_descriptor, []() {
    return std::make_unique<PaddleDevice>();
})
