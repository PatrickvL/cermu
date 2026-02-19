/**
 * lightpen_device.cpp - Light Pen Implementation
 */

#include "lightpen_device.h"
#include "../../core/device_registry.h"
#include <cstdio>
#include <SDL_events.h>

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

LightpenDevice::LightpenDevice()
    : pen_x_(0), pen_y_(0), triggered_(false)
{
    binding_.type = HostInputType::HOST_MOUSE;
    binding_.label = "Host Mouse";
}

void LightpenDevice::set_host_input_binding(const HostInputBinding& binding) {
    release_all_signals();
    triggered_ = false;
    notify_port();
    binding_ = binding;
    printf("Light Pen: Input source changed to %s\n", binding_.label.c_str());
}

bool LightpenDevice::process_sdl_event(const SDL_Event& event) {
    if (binding_.type != HostInputType::HOST_MOUSE) return false;

    switch (event.type) {
        case SDL_MOUSEMOTION:
            set_position(event.motion.x, event.motion.y);
            return true;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                set_triggered(event.type == SDL_MOUSEBUTTONDOWN);
                return true;
            }
            break;
    }
    return false;
}

void LightpenDevice::set_triggered(bool active) {
    triggered_ = active;
    // The lightpen input on the C64 is directly connected to LP pin on VIC-II,
    // exposed at Control Port 1, pin 6 (active-low: pull LOW to trigger latch).
    set_signal(ConnectorSignals::LIGHT_PEN, active);
}

// ============================================================================
// GUI
// ============================================================================

#ifdef IMGUI_VERSION
void LightpenDevice::render_device_ui() {
    ImGui::Text("  Pos (%d, %d)  Triggered: %s",
                pen_x_, pen_y_, triggered_ ? "Yes" : "No");
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

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
