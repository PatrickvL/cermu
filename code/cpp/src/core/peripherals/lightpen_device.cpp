/**
 * lightpen_device.cpp - Light Pen Implementation
 */

#include "lightpen_device.h"
#include "../device_registry.h"

LightpenDevice::LightpenDevice()
    : state_(0xFFFFFFFF), pen_x_(0), pen_y_(0), triggered_(false) {}

void LightpenDevice::reset() {
    state_ = 0xFFFFFFFF;
    triggered_ = false;
    if (port_) port_->notify_device_output_changed(state_);
}

uint32_t LightpenDevice::get_output_signals() const { return state_; }

void LightpenDevice::set_triggered(bool active) {
    triggered_ = active;
    // The lightpen input on the C64 is directly connected to LP pin on VIC-II,
    // exposed at Control Port 1, pin 6 (active-low: pull LOW to trigger latch).
    if (active) {
        state_ &= ~(1u << ConnectorSignals::LIGHT_PEN);
    } else {
        state_ |= (1u << ConnectorSignals::LIGHT_PEN);
    }
    if (port_) port_->notify_device_output_changed(state_);
}

static const DeviceDescriptor lightpen_descriptor = {
    "lightpen",
    "Light Pen",
    "Light pen — triggers VIC-II lightpen latch at screen coordinates",
    ConnectorType::CONTROL_PORT_DB9,
    false
};

REGISTER_DEVICE(lightpen_descriptor, []() {
    return std::make_unique<LightpenDevice>();
})
