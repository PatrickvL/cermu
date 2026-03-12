/**
 * neos_mouse.cpp - NEOS Mouse Implementation
 *
 * Implements the NEOS bit-bang protocol for mouse position data transfer
 * over CIA port lines.  See header for protocol details.
 */

#include "devices/input/neos_mouse.hpp"
#include "core/device_registry.hpp"
#include <cstdio>
#include <algorithm>

NEOSMouse::NEOSMouse() {
    binding_.type = HostInputType::HOST_MOUSE;
    binding_.label = "Host Mouse";
}

void NEOSMouse::on_input_source_will_change() {
    accum_dx_ = accum_dy_ = 0;
    latched_dx_ = latched_dy_ = 0;
    left_button_ = right_button_ = false;
    phase_ = 0;
}

bool NEOSMouse::process_sdl_event(const SDL_Event& event) {
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

void NEOSMouse::move(int dx, int dy) {
    accum_dx_ += dx;
    accum_dy_ += dy;
}

void NEOSMouse::latch_deltas() {
    // Clamp accumulated deltas to signed 8-bit range
    latched_dx_ = static_cast<int8_t>(std::max(-127, std::min(127, accum_dx_)));
    latched_dy_ = static_cast<int8_t>(std::max(-127, std::min(127, accum_dy_)));
    accum_dx_ = 0;
    accum_dy_ = 0;
    phase_ = 0;
    update_output();
}

void NEOSMouse::strobe() {
    phase_ = (phase_ + 1) & 3;
    update_output();
}

void NEOSMouse::update_output() {
    // Determine which nibble to output based on protocol phase
    uint8_t nibble = 0;
    auto dx = static_cast<uint8_t>(latched_dx_);
    auto dy = static_cast<uint8_t>(latched_dy_);

    switch (phase_) {
        case 0: nibble = (dx >> 4) & 0x0F; break;  // X high nibble
        case 1: nibble = dx & 0x0F;         break;  // X low nibble
        case 2: nibble = (dy >> 4) & 0x0F; break;  // Y high nibble
        case 3: nibble = dy & 0x0F;         break;  // Y low nibble
    }

    output_nibble(nibble);
}

void NEOSMouse::output_nibble(uint8_t nibble) {
    // Map 4 data bits to control port lines:
    //   bit 0 → JOY_UP   (pin 1)
    //   bit 1 → JOY_DOWN (pin 2)
    //   bit 2 → JOY_LEFT (pin 3)
    //   bit 3 → JOY_FIRE (pin 6)
    // Lines active-low: assert (pull LOW) when bit is set.
    set_signal_batch(ConnectorSignals::JOY_UP,   (nibble & 0x01) != 0);
    set_signal_batch(ConnectorSignals::JOY_DOWN,  (nibble & 0x02) != 0);
    set_signal_batch(ConnectorSignals::JOY_LEFT,  (nibble & 0x04) != 0);
    set_signal_batch(ConnectorSignals::JOY_FIRE,  (nibble & 0x08) != 0);
    // RIGHT line is used as strobe by the host — we don't drive it
    notify_port();
}

// ============================================================================
// GUI
// ============================================================================

#ifdef CERMU_HAS_GUI
void NEOSMouse::render_device_ui() {
    ImGui::Text("  Phase:%d  dX:%+4d dY:%+4d  LMB:%s RMB:%s",
                phase_, (int)latched_dx_, (int)latched_dy_,
                left_button_ ? "Y" : ".", right_button_ ? "Y" : ".");
    ImGui::Text("  Accum: dX:%+4d dY:%+4d", accum_dx_, accum_dy_);
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor neos_mouse_descriptor = {
    "mouse_neos",
    "NEOS Mouse",
    "NEOS mouse — third-party bit-bang protocol via CIA port lines",
    ConnectorType::CONTROL_PORT_DB9,
    false
};

REGISTER_DEVICE(neos_mouse_descriptor, []() {
    return std::make_unique<NEOSMouse>();
})
