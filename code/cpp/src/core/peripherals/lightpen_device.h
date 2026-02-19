#pragma once
/**
 * lightpen_device.h - Light Pen Peripheral
 *
 * Emulates a light pen that plugs into Control Port 1 (DB-9).
 * Triggers VIC-II's lightpen latch when the pen detects the CRT beam
 * at its position.
 */

#include "control_port_device.h"

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

#ifdef IMGUI_VERSION
    void render_device_ui() override;
#endif

    /// Set pen X/Y coordinates (system display coordinates).
    void set_position(int x, int y) { pen_x_ = x; pen_y_ = y; }

    /// Trigger the lightpen (pen touching screen).
    void set_triggered(bool active);

    int get_pen_x() const { return pen_x_; }
    int get_pen_y() const { return pen_y_; }
    bool is_triggered() const { return triggered_; }

private:
    int      pen_x_;
    int      pen_y_;
    bool     triggered_;
    HostInputBinding binding_;
};
