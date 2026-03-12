#pragma once
/**
 * commodore_1351_mouse.h - Commodore 1351 Proportional Mouse
 *
 * Emulates a Commodore 1351 proportional mouse that plugs into a
 * Control Port (DB-9).  Uses SID POT inputs for X/Y position and
 * joystick lines for button presses.
 */

#include "core/peripherals/pot_input_device.hpp"

class Commodore1351Mouse : public PotInputDevice {
public:
    Commodore1351Mouse();
    ~Commodore1351Mouse() override = default;

    const char* get_name() const override { return "Commodore 1351 Mouse"; }
    const char* get_id() const override   { return "mouse_1351"; }

    // --- Host input support -------------------------------------------
    int  get_supported_input_type_count() const override { return 1; }
    HostInputType get_supported_input_type(int index) const override {
        (void)index; return HostInputType::HOST_MOUSE;
    }
    bool process_sdl_event(const SDL_Event& event) override;

#ifdef CERMU_HAS_GUI
    void render_device_ui() override;
#endif

    // --- Mouse-specific API --------------------------------------------
    void set_left_button(bool pressed)  { set_signal(ConnectorSignals::JOY_FIRE, pressed); }
    void set_right_button(bool pressed) { set_signal(ConnectorSignals::JOY_UP, pressed); }
    void move(int dx, int dy);  ///< Relative mouse movement
};
