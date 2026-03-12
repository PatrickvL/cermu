/**
 * lightpen_device.cpp - Light Pen Implementation
 *
 * The lightpen peripheral emulates the photosensor behaviour.
 * The VIC-II's lp_pin_read callback calls get_lp_pin_state() every cycle,
 * passing the current beam position.  This device converts the host mouse
 * position to VIC-II raster coordinates and returns LOW when the beam is
 * at the pen's target position (and the button is pressed).
 *
 * The VIC-II detects the negative edge on its LP pin and latches the
 * coordinates internally — the device itself never touches LPX/LPY.
 */

#include "devices/input/lightpen_device.hpp"
#include "core/device_registry.hpp"
#include <cstdio>

LightpenDevice::LightpenDevice()
{
    binding_.type = HostInputType::HOST_MOUSE;
    binding_.label = "Host Mouse";
}

void LightpenDevice::on_input_source_will_change() {
    triggered_ = false;
}

bool LightpenDevice::process_sdl_event(const SDL_Event& event) {
    if (binding_.type != HostInputType::HOST_MOUSE) return false;

    switch (event.type) {
        case SDL_MOUSEMOTION:
            pen_sdl_x_ = event.motion.x;
            pen_sdl_y_ = event.motion.y;
            return true;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                triggered_ = (event.type == SDL_MOUSEBUTTONDOWN);
                return true;
            }
            break;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Coordinate conversion: SDL window → VIC-II raster
// ---------------------------------------------------------------------------

bool LightpenDevice::sdl_to_vic(int* vic_x, int* vic_y) const {
    // Display rect must be set and non-zero
    if (disp_rect_w_ <= 0 || disp_rect_h_ <= 0) return false;

    // Normalise SDL mouse position relative to the display rect
    float rel_x = ((float)pen_sdl_x_ - disp_rect_x_) / disp_rect_w_;
    float rel_y = ((float)pen_sdl_y_ - disp_rect_y_) / disp_rect_h_;

    // Outside the display area?
    if (rel_x < 0.0f || rel_x >= 1.0f || rel_y < 0.0f || rel_y >= 1.0f)
        return false;

    // Framebuffer pixel coordinates (0-based within visible area)
    int fb_x = (int)(rel_x * (float)visible_pixels_per_line_);
    int fb_y = (int)(rel_y * (float)visible_lines_);

    // Convert to VIC-II x_coordinate (wraps around pixels_per_line)
    *vic_x = ((int)first_visible_x_ + fb_x) % (int)pixels_per_line_;
    // Convert to VIC-II raster_counter
    *vic_y = (int)first_visible_line_ + fb_y;

    return true;
}

// ---------------------------------------------------------------------------
// LP pin state query (called from VIC-II's lp_pin_read callback each cycle)
// ---------------------------------------------------------------------------

bool LightpenDevice::get_lp_pin_state(uint16_t beam_x, uint16_t beam_y) const {
    // Not triggered (pen not touching screen) → HIGH
    if (!triggered_ || pen_sdl_x_ < 0 || pen_sdl_y_ < 0)
        return true;

    // Convert SDL mouse position to VIC-II coordinates
    int vic_x, vic_y;
    if (!sdl_to_vic(&vic_x, &vic_y))
        return true;  // Mouse outside display → HIGH

    // Compare beam position with pen target (per-cycle = 8-pixel granularity)
    if ((uint16_t)vic_y == beam_y && (beam_x / 8) == ((uint16_t)vic_x / 8))
        return false;  // LOW — beam at pen position

    return true;  // HIGH — beam elsewhere
}

// ============================================================================
// GUI
// ============================================================================

#ifdef CERMU_HAS_GUI
void LightpenDevice::render_device_ui() {
    int vic_x = -1, vic_y = -1;
    if (pen_sdl_x_ >= 0 && pen_sdl_y_ >= 0) {
        sdl_to_vic(&vic_x, &vic_y);
    }
    ImGui::Text("  SDL (%d, %d)  VIC (%d, %d)  %s",
                pen_sdl_x_, pen_sdl_y_, vic_x, vic_y,
                triggered_ ? "ACTIVE" : "idle");
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor lightpen_descriptor = {
    "lightpen",
    "Light Pen",
    "Light pen — triggers VIC-II lightpen latch at screen coordinates",
    PortType::CONTROL_PORT_DB9,
    false
};

REGISTER_DEVICE(lightpen_descriptor, []() {
    return std::make_unique<LightpenDevice>();
})
