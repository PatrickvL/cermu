#pragma once
/**
 * commodore_1350_mouse.h - Commodore 1350 Mouse (Digital / Joystick-emulation)
 *
 * The Commodore 1350 mouse is a two-button mouse that emulates a digital
 * joystick.  It converts relative mouse motion into joystick direction
 * signals via the CIA port lines — it does NOT use SID POT inputs.
 *
 * Movement:  relative motion → UP/DOWN/LEFT/RIGHT lines (active-low)
 * Buttons:   Left = FIRE (pin 6), Right = UP line (active-low)
 *
 * Because it uses only digital lines, it works with any software that
 * supports a joystick but offers only 8-directional movement, not the
 * proportional tracking that the 1351 provides.
 */

#include "core/peripherals/control_port_device.hpp"

class Commodore1350Mouse : public ControlPortInputDevice {
public:
    Commodore1350Mouse();
    ~Commodore1350Mouse() override = default;

    const char* get_name() const override { return "Commodore 1350 Mouse"; }
    const char* get_id() const override   { return "mouse_1350"; }

    // --- Host input support -------------------------------------------
    int  get_supported_input_type_count() const override { return 1; }
    HostInputType get_supported_input_type(int index) const override {
        (void)index; return HostInputType::HOST_MOUSE;
    }
    bool process_sdl_event(const SDL_Event& event) override;
    void on_input_source_will_change() override;

#ifdef CERMU_HAS_GUI
    void render_device_ui() override;
#endif

    // --- Mouse-specific API --------------------------------------------

    /// Move the mouse by relative delta (pixels).  Motion is accumulated and
    /// converted to digital direction pulses once the threshold is crossed.
    void move(int dx, int dy);

    /// Buttons map to joystick lines.
    void set_left_button(bool pressed)  { set_signal(PortSignals::JOY_FIRE, pressed); }
    void set_right_button(bool pressed) { set_signal(PortSignals::JOY_UP, pressed); }

private:
    int accum_x_ = 0;  ///< Accumulated X motion between ticks
    int accum_y_ = 0;  ///< Accumulated Y motion between ticks

    /// Threshold (pixels) before a direction pulse is generated.
    static constexpr int MOTION_THRESHOLD = 2;
};
