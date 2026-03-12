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

#include "devices/input/nes_zapper.hpp"
#include "core/device_registry.hpp"

#include <cstdio>
#include <SDL_events.h>

using PortSignals::NESControllerBit;

// ============================================================================
// CONSTRUCTION / RESET
// ============================================================================

NesZapper::NesZapper() {
    binding_.type  = HostInputType::HOST_MOUSE;
    binding_.label = "Host Mouse";
}

void NesZapper::reset() {
    aim_x_          = 128;
    aim_y_          = 120;
    trigger_pulled_ = false;
    light_detected_ = false;
    output_signals_ = 0xFFFFFFFF;
}

// ============================================================================
// HOST INPUT BINDING
// ============================================================================

void NesZapper::set_host_input_binding(const HostInputBinding& binding) {
    on_input_source_will_change();
    binding_ = binding;
    printf("NES Zapper: Input source changed to %s\n", binding_.label.c_str());
}

void NesZapper::on_input_source_will_change() {
    trigger_pulled_ = false;
    light_detected_ = false;
    aim_x_ = 128;
    aim_y_ = 120;
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
        aim_x_ = event.motion.x;
        aim_y_ = event.motion.y;
        return true;
    }

    // Left mouse button → trigger
    bool pressed;
    if (extract_mouse_button(event, SDL_BUTTON_LEFT, pressed)) {
        set_trigger(pressed);
        return true;
    }

    return false;
}

// ============================================================================
// GUI
// ============================================================================

#ifdef CERMU_HAS_GUI
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
    PortType::CONTROLLER_NES,
    false
};

REGISTER_DEVICE(nes_zapper_descriptor, []() {
    return std::make_unique<NesZapper>();
})
