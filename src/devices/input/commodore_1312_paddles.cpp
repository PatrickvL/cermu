/**
 * commodore_1312_paddles.cpp - Commodore 1312 Paddle Controller Implementation
 */

#include "devices/input/commodore_1312_paddles.h"
#include "core/device_registry.h"
#include <cstdio>
#include <algorithm>

Commodore1312Paddles::Commodore1312Paddles() {
    binding_.type = HostInputType::HOST_MOUSE;
    binding_.label = "Host Mouse";
}

void Commodore1312Paddles::on_input_source_will_change() {
    pot_x_ = 128;
    pot_y_ = 128;
}

bool Commodore1312Paddles::process_sdl_event(const SDL_Event& event) {
    if (binding_.type == HostInputType::HOST_MOUSE) {
        switch (event.type) {
            case SDL_MOUSEMOTION:
                // Mouse X motion maps to paddle X, Y to paddle Y
                set_paddle_x(static_cast<uint8_t>(
                    std::max(0, std::min(255, (int)pot_x_ + event.motion.xrel))));
                set_paddle_y(static_cast<uint8_t>(
                    std::max(0, std::min(255, (int)pot_y_ + event.motion.yrel))));
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
        if (!should_accept_gamepad_event(event)) return false;

        if (event.type == SDL_CONTROLLERAXISMOTION) {

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

// ============================================================================
// GUI
// ============================================================================

#ifdef CERMU_HAS_GUI
void Commodore1312Paddles::render_device_ui() {
    bool fire_x = is_signal_asserted(ConnectorSignals::JOY_FIRE);
    bool fire_y = is_signal_asserted(ConnectorSignals::JOY_UP);

    ImGui::Text("  X:%3d  Y:%3d  Fire A:%s  B:%s",
                pot_x_, pot_y_, fire_x ? "Y" : ".", fire_y ? "Y" : ".");
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor paddles_1312_descriptor = {
    "paddles_1312",
    "Commodore 1312 Paddles",
    "Commodore 1312 paddle controller pair — analog potentiometers + fire buttons",
    ConnectorType::CONTROL_PORT_DB9,
    false
};

REGISTER_DEVICE(paddles_1312_descriptor, []() {
    return std::make_unique<Commodore1312Paddles>();
})
