/**
 * commodore_1351_mouse.cpp - Commodore 1351 Proportional Mouse Implementation
 */

#include "commodore_1351_mouse.h"
#include "../../core/device_registry.h"
#include <cstdio>
#include <SDL_events.h>

#ifdef CERMU_HAS_GUI
#include "imgui.h"
#endif

Commodore1351Mouse::Commodore1351Mouse() {
    binding_.type = HostInputType::HOST_MOUSE;
    binding_.label = "Host Mouse";
}

void Commodore1351Mouse::set_host_input_binding(const HostInputBinding& binding) {
    release_all_signals();
    notify_port();
    binding_ = binding;
    printf("1351 Mouse: Input source changed to %s\n", binding_.label.c_str());
}

bool Commodore1351Mouse::process_sdl_event(const SDL_Event& event) {
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

void Commodore1351Mouse::move(int dx, int dy) {
    // The 1351 encodes movement in the low 6 bits of the SID POT registers.
    pot_x_ = static_cast<uint8_t>((pot_x_ + dx) & 0xFF);
    pot_y_ = static_cast<uint8_t>((pot_y_ + dy) & 0xFF);
}

// ============================================================================
// GUI
// ============================================================================

#ifdef CERMU_HAS_GUI
void Commodore1351Mouse::render_device_ui() {
    bool lmb = is_signal_asserted(ConnectorSignals::JOY_FIRE);
    bool rmb = is_signal_asserted(ConnectorSignals::JOY_UP);

    ImGui::Text("  POT X:%3d  Y:%3d  LMB:%s RMB:%s",
                pot_x_, pot_y_, lmb ? "Y" : ".", rmb ? "Y" : ".");
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor mouse_1351_descriptor = {
    "mouse_1351",
    "Commodore 1351 Mouse",
    "Commodore 1351 proportional mouse — uses SID POT inputs for position",
    ConnectorType::CONTROL_PORT_DB9,
    false
};

REGISTER_DEVICE(mouse_1351_descriptor, []() {
    return std::make_unique<Commodore1351Mouse>();
})
