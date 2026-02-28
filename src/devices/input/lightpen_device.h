#pragma once
/**
 * lightpen_device.h - Light Pen Peripheral
 *
 * Emulates a light pen that plugs into Control Port 1 (DB-9).
 *
 * A real light pen has a photosensor that fires when the CRT beam sweeps
 * past its position.  In emulation the host mouse provides the "pen" position.
 *
 * Architecture:
 *   The VIC-II calls its lp_pin_read callback every cycle.  The system wrapper
 *   routes that callback through this device's get_lp_pin_state() method,
 *   passing the current beam position.  The device compares the beam position
 *   against the host mouse position (converted to VIC-II raster coordinates)
 *   and returns the LP pin level:
 *     HIGH (true)  = beam is elsewhere, no trigger
 *     LOW  (false) = beam is at the pen position, trigger
 *
 *   The VIC-II's LP pin is directly wired to Control Port 1 pin 6.
 *   On a HIGH→LOW edge the VIC-II latches its current x_coordinate/2 → LPX,
 *   raster_counter → LPY, and sets the ILP interrupt flag (once per frame).
 */

#include "../../core/peripherals/control_port_device.h"
#include <functional>

class LightpenDevice : public ControlPortInputDevice {
public:
    LightpenDevice();
    ~LightpenDevice() override = default;

    const char* get_name() const override { return "Light Pen"; }
    const char* get_id() const override   { return "lightpen"; }

    // --- Host input support -------------------------------------------
    bool accepts_host_input() const override { return true; }
    int  get_supported_input_type_count() const override { return 1; }
    HostInputType get_supported_input_type(int index) const override {
        (void)index; return HostInputType::HOST_MOUSE;
    }
    const HostInputBinding& get_host_input_binding() const override { return binding_; }
    void set_host_input_binding(const HostInputBinding& binding) override;
    bool process_sdl_event(const SDL_Event& event) override;

#ifdef CERMU_HAS_GUI
    void render_device_ui() override;
#endif

    // --- LP pin state -------------------------------------------------
    // Called by the system wrapper's lp_pin_read callback each VIC-II cycle.
    // beam_x / beam_y are the VIC-II's current x_coordinate / raster_counter.
    // Returns true = HIGH (no trigger), false = LOW (beam at pen position).
    bool get_lp_pin_state(uint16_t beam_x, uint16_t beam_y) const;

    // --- Display geometry (set by system wrapper) ---------------------
    // Screen rect in SDL window coordinates where the emulated display is drawn.
    // Updated each frame by the GUI after rendering the display image.
    void set_display_screen_rect(float x, float y, float w, float h) {
        disp_rect_x_ = x;  disp_rect_y_ = y;
        disp_rect_w_ = w;  disp_rect_h_ = h;
    }

    // VIC-II configuration values needed for coordinate conversion.
    // Set once by the system wrapper during initialization.
    void set_vic_geometry(uint16_t first_visible_x, uint16_t pixels_per_line,
                          uint16_t first_visible_line, uint16_t visible_lines,
                          uint16_t visible_pixels_per_line) {
        first_visible_x_    = first_visible_x;
        pixels_per_line_    = pixels_per_line;
        first_visible_line_ = first_visible_line;
        visible_lines_      = visible_lines;
        visible_pixels_per_line_ = visible_pixels_per_line;
    }

    // Raw accessors (for debugging / GUI display)
    int get_pen_x() const { return pen_sdl_x_; }
    int get_pen_y() const { return pen_sdl_y_; }
    bool is_triggered() const { return triggered_; }

private:
    // Host cursor position in raw SDL window coordinates
    int  pen_sdl_x_ = -1;
    int  pen_sdl_y_ = -1;
    bool triggered_  = false;   // Mouse button held (pen touching screen)

    HostInputBinding binding_;

    // Display rect in SDL window coordinates (updated each frame by GUI)
    float disp_rect_x_ = 0, disp_rect_y_ = 0;
    float disp_rect_w_ = 0, disp_rect_h_ = 0;

    // VIC-II display geometry for coordinate conversion
    uint16_t first_visible_x_    = 480;   // x_coordinate at framebuffer pixel 0
    uint16_t pixels_per_line_    = 504;   // Total x_coordinate values per line
    uint16_t first_visible_line_ = 0;     // raster_counter at framebuffer row 0
    uint16_t visible_lines_      = 284;   // Height of framebuffer (lines rendered)
    uint16_t visible_pixels_per_line_ = 403; // Width of framebuffer

    // Convert raw SDL mouse position to VIC-II raster coordinates.
    // Returns false if mouse is outside the display area.
    bool sdl_to_vic(int* vic_x, int* vic_y) const;
};
