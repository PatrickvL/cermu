/**
 * commodore_1350_mouse.cpp - Commodore 1350 Mouse (Digital) Implementation
 *
 * The 1350 converts mouse motion into joystick direction signals.
 * It accumulates relative motion and fires direction pulses when
 * the accumulated delta exceeds a threshold.
 */

#include "commodore_1350_mouse.h"
#include "../../core/device_registry.h"
#include <cstdio>
#include <cmath>
#include <SDL_events.h>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

Commodore1350Mouse::Commodore1350Mouse() {
    binding_.type = HostInputType::HOST_MOUSE;
    binding_.label = "Host Mouse";
}

void Commodore1350Mouse::set_host_input_binding(const HostInputBinding& binding) {
    release_all_signals();
    accum_x_ = 0;
    accum_y_ = 0;
    notify_port();
    binding_ = binding;
    printf("1350 Mouse: Input source changed to %s\n", binding_.label.c_str());
}

bool Commodore1350Mouse::process_sdl_event(const SDL_Event& event) {
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

void Commodore1350Mouse::move(int dx, int dy) {
    accum_x_ += dx;
    accum_y_ += dy;

    // Convert accumulated motion to digital direction signals.
    // Release direction when motion is below threshold (provides pulsing behavior).
    bool left  = (accum_x_ < -MOTION_THRESHOLD);
    bool right = (accum_x_ > MOTION_THRESHOLD);
    bool up    = (accum_y_ < -MOTION_THRESHOLD);
    bool down  = (accum_y_ > MOTION_THRESHOLD);

    // Reset accumulator when threshold crossed
    if (left || right) accum_x_ = 0;
    if (up || down)    accum_y_ = 0;

    // Batch-update direction signals to avoid per-line port notifications
    set_signal_batch(ConnectorSignals::JOY_LEFT,  left);
    set_signal_batch(ConnectorSignals::JOY_RIGHT, right);
    set_signal_batch(ConnectorSignals::JOY_UP,    up);
    set_signal_batch(ConnectorSignals::JOY_DOWN,  down);
    notify_port();
}

// ============================================================================
// GUI
// ============================================================================

#ifdef IMGUI_VERSION
void Commodore1350Mouse::render_device_ui() {
    bool u = is_signal_asserted(ConnectorSignals::JOY_UP);
    bool d = is_signal_asserted(ConnectorSignals::JOY_DOWN);
    bool l = is_signal_asserted(ConnectorSignals::JOY_LEFT);
    bool r = is_signal_asserted(ConnectorSignals::JOY_RIGHT);
    bool f = is_signal_asserted(ConnectorSignals::JOY_FIRE);

    ImGui::Text("  %s %s %s %s  LMB:%s",
                u ? "U" : ".", d ? "D" : ".", l ? "L" : ".",
                r ? "R" : ".", f ? "Y" : ".");
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor mouse_1350_descriptor = {
    "mouse_1350",
    "Commodore 1350 Mouse",
    "Commodore 1350 digital mouse — emulates joystick via direction pulses",
    ConnectorType::CONTROL_PORT_DB9,
    false
};

REGISTER_DEVICE(mouse_1350_descriptor, []() {
    return std::make_unique<Commodore1350Mouse>();
})
