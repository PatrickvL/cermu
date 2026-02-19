/**
 * paddle_device.cpp - Paddle Controller Implementation
 */

#include "paddle_device.h"
#include "../device_registry.h"
#include <cstdio>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

PaddleDevice::PaddleDevice()
    : state_(0xFFFFFFFF), pot_x_(128), pot_y_(128)
{
    binding_.type = HostInputType::HOST_MOUSE;
    binding_.label = "Host Mouse";
}

void PaddleDevice::reset() {
    state_ = 0xFFFFFFFF;
    pot_x_ = 128;
    pot_y_ = 128;
    if (port_) port_->notify_device_output_changed(state_);
}

uint32_t PaddleDevice::get_output_signals() const { return state_; }

void PaddleDevice::set_host_input_binding(const HostInputBinding& binding) {
    state_ = 0xFFFFFFFF;
    pot_x_ = 128;
    pot_y_ = 128;
    if (port_) port_->notify_device_output_changed(state_);
    binding_ = binding;
    printf("Paddles: Input source changed to %s\n", binding_.label.c_str());
}

bool PaddleDevice::process_sdl_event(const SDL_Event& event) {
    if (binding_.type == HostInputType::HOST_MOUSE) {
        switch (event.type) {
            case SDL_MOUSEMOTION:
                // Mouse X motion maps to paddle X, Y to paddle Y
                set_paddle_x(static_cast<uint8_t>(
                    std::max(0, std::min(255, pot_x_ + event.motion.xrel))));
                set_paddle_y(static_cast<uint8_t>(
                    std::max(0, std::min(255, pot_y_ + event.motion.yrel))));
                return true;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                bool pressed = (event.type == SDL_MOUSEBUTTONDOWN);
                if (event.button.button == SDL_BUTTON_LEFT) {
                    set_fire_x(pressed);
                    return true;
                }
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    set_fire_y(pressed);
                    return true;
                }
                break;
            }
        }
    } else if (binding_.type == HostInputType::SDL_GAMEPAD) {
        // Filter by gamepad instance if bound
        if (event.type == SDL_CONTROLLERAXISMOTION) {
            if (binding_.gamepad_instance_id >= 0 &&
                event.caxis.which != binding_.gamepad_instance_id)
                return false;

            // Left stick X → paddle X, left stick Y → paddle Y
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                // Map -32768..32767 to 0..255
                set_paddle_x(static_cast<uint8_t>(
                    ((int)event.caxis.value + 32768) * 255 / 65535));
                return true;
            }
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                set_paddle_y(static_cast<uint8_t>(
                    ((int)event.caxis.value + 32768) * 255 / 65535));
                return true;
            }
        }
        if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
            if (binding_.gamepad_instance_id >= 0 &&
                event.cbutton.which != binding_.gamepad_instance_id)
                return false;

            bool pressed = (event.type == SDL_CONTROLLERBUTTONDOWN);
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                set_fire_x(pressed);
                return true;
            }
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                set_fire_y(pressed);
                return true;
            }
        }
    }
    return false;
}

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

// ============================================================================
// GUI
// ============================================================================

#ifdef IMGUI_VERSION
void PaddleDevice::render_device_ui() {
    bool fire_x = !(state_ & (1u << ConnectorSignals::JOY_FIRE));
    bool fire_y = !(state_ & (1u << ConnectorSignals::JOY_UP));

    ImGui::Text("  X:%3d  Y:%3d  Fire A:%s  B:%s",
                pot_x_, pot_y_, fire_x ? "Y" : ".", fire_y ? "Y" : ".");
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

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
