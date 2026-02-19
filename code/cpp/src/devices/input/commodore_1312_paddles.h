#pragma once
/**
 * commodore_1312_paddles.h - Commodore 1312 Paddle Controller Pair
 *
 * Emulates a Commodore 1312 paddle controller pair that plugs into a
 * Control Port (DB-9).  Each paddle has a potentiometer (0-255)
 * and a fire button.
 */

#include "../../core/peripherals/pot_input_device.h"
#include <algorithm>

class Commodore1312Paddles : public PotInputDevice {
public:
    Commodore1312Paddles();
    ~Commodore1312Paddles() override = default;

    const char* get_name() const override { return "Commodore 1312 Paddles"; }
    const char* get_id() const override   { return "paddles_1312"; }

    // --- Host input support -------------------------------------------
    bool accepts_host_input() const override { return true; }
    int  get_supported_input_type_count() const override { return 2; }
    HostInputType get_supported_input_type(int index) const override {
        return (index == 0) ? HostInputType::HOST_MOUSE : HostInputType::SDL_GAMEPAD;
    }
    const HostInputBinding& get_host_input_binding() const override { return binding_; }
    void set_host_input_binding(const HostInputBinding& binding) override;
    bool process_sdl_event(const SDL_Event& event) override;

#ifdef IMGUI_VERSION
    void render_device_ui() override;
#endif

    /// Set paddle X position (0-255).
    void set_paddle_x(uint8_t value) { pot_x_ = value; }
    /// Set paddle Y position (0-255).
    void set_paddle_y(uint8_t value) { pot_y_ = value; }

    /// Fire buttons (active-low).
    void set_fire_x(bool pressed) { set_signal(ConnectorSignals::JOY_FIRE, pressed); }
    void set_fire_y(bool pressed) { set_signal(ConnectorSignals::JOY_UP, pressed); }

private:
    HostInputBinding binding_;
};
