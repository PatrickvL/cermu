/**
 * nes_zapper.cpp — NES Zapper Light Gun Implementation
 *
 * The Zapper uses two signals on the controller port:
 *   D3: Light sensor — active-low when bright pixel detected at aim point
 *   D4: Trigger — active-low when trigger is pulled
 *
 * In real hardware, the Zapper's photodiode detects light from the CRT.
 * Games flash the screen white (or draw white rectangles on targets) and
 * check whether the sensor sees light. This implementation relies on the
 * system to sample the PPU framebuffer at (aim_x_, aim_y_) and call
 * set_light_detected() each frame.
 */

#include "nes_zapper.h"
#include "../../core/device_registry.h"

#include <SDL_events.h>

using ConnectorSignals::NESControllerBit;

#ifdef IMGUI_VERSION
#include "imgui.h"
#endif

// ============================================================================
// CONSTRUCTION / RESET
// ============================================================================

NesZapper::NesZapper() {
    binding_.type  = HostInputType::HOST_MOUSE;
    binding_.label = "Mouse";
}

void NesZapper::reset() {
    aim_x_          = 128;
    aim_y_          = 120;
    trigger_pulled_ = false;
    light_detected_ = false;
    output_signals_ = 0xFFFFFFFF;
}

// ============================================================================
// SIGNAL PROTOCOL
// ============================================================================

void NesZapper::on_signal_change(uint32_t /*signal_state*/) {
    // The Zapper is output-only on D3/D4; it doesn't react to
    // CLK or LATCH from the system. Output is updated directly
    // via set_light_detected() and set_trigger().
}

void NesZapper::update_output() {
    uint32_t signals = 0xFFFFFFFF;  // All released

    // D3: Light sensor — active-low (LOW = light detected)
    if (light_detected_) {
        signals &= ~(1u << NESControllerBit::NES_D3);
    }

    // D4: Trigger — active-low (LOW = trigger pulled)
    if (trigger_pulled_) {
        signals &= ~(1u << NESControllerBit::NES_D4);
    }

    output_signals_ = signals;
}

// ============================================================================
// SDL EVENT PROCESSING
// ============================================================================

bool NesZapper::process_sdl_event(const SDL_Event& event) {
    if (binding_.type != HostInputType::HOST_MOUSE) return false;

    // Mouse movement → aim position
    if (event.type == SDL_MOUSEMOTION) {
        // The caller (system) is responsible for translating screen
        // coordinates to NES pixel coordinates. We store raw coords
        // and the system will map them when checking the framebuffer.
        aim_x_ = event.motion.x;
        aim_y_ = event.motion.y;
        return true;
    }

    // Left mouse button → trigger
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        set_trigger(true);
        return true;
    }
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        set_trigger(false);
        return true;
    }

    return false;
}

// ============================================================================
// GUI
// ============================================================================

#ifdef IMGUI_VERSION
void NesZapper::render_device_ui() {
    ImGui::Text("Aim: (%d, %d)", aim_x_, aim_y_);
    ImGui::Text("Trigger: %s  Light: %s",
                trigger_pulled_ ? "PULLED" : "---",
                light_detected_ ? "DETECTED" : "---");
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor nes_zapper_descriptor = {
    "nes_zapper",
    "NES Zapper",
    "NES-005 light gun — detects bright pixels at aim point, trigger on mouse click",
    ConnectorType::CONTROLLER_NES,
    false
};

REGISTER_DEVICE(nes_zapper_descriptor, []() {
    return std::make_unique<NesZapper>();
})
