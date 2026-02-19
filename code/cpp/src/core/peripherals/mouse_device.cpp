/**
 * mouse_device.cpp - Proportional Mouse (1351) Implementation
 */

#include "mouse_device.h"
#include "../device_registry.h"
#include <cstdio>
#include <SDL_events.h>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

MouseDevice::MouseDevice()
    : state_(0xFFFFFFFF)
    , pot_x_(128)
    , pot_y_(128)
{
    binding_.type = HostInputType::HOST_MOUSE;
    binding_.label = "Host Mouse";
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

void MouseDevice::set_host_input_binding(const HostInputBinding& binding) {
    state_ = 0xFFFFFFFF;
    if (port_) port_->notify_device_output_changed(state_);
    binding_ = binding;
    printf("Mouse 1351: Input source changed to %s\n", binding_.label.c_str());
}

bool MouseDevice::process_sdl_event(const SDL_Event& event) {
    if (binding_.type != HostInputType::HOST_MOUSE) return false;

    switch (event.type) {
        case SDL_MOUSEMOTION:
            move(event.motion.xrel, event.motion.yrel);
            return true;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            bool pressed = (event.type == SDL_MOUSEBUTTONDOWN);
            if (event.button.button == SDL_BUTTON_LEFT) {
                set_left_button(pressed);
                return true;
            }
            if (event.button.button == SDL_BUTTON_RIGHT) {
                set_right_button(pressed);
                return true;
            }
            break;
        }
    }
    return false;
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
    pot_x_ = static_cast<uint8_t>((pot_x_ + dx) & 0xFF);
    pot_y_ = static_cast<uint8_t>((pot_y_ + dy) & 0xFF);
}

// ============================================================================
// GUI
// ============================================================================

#ifdef IMGUI_VERSION
void MouseDevice::render_device_ui() {
    bool lmb = !(state_ & (1u << ConnectorSignals::JOY_FIRE));
    bool rmb = !(state_ & (1u << ConnectorSignals::JOY_UP));

    ImGui::Text("  POT X:%3d  Y:%3d  LMB:%s RMB:%s",
                pot_x_, pot_y_, lmb ? "Y" : ".", rmb ? "Y" : ".");
}
#endif

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
