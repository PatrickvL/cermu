#pragma once
/**
 * mouse_device.h - Proportional Mouse/Trackball Peripheral
 *
 * Emulates a 1351-compatible proportional mouse that plugs into a
 * Control Port (DB-9).  Uses SID POT inputs for X/Y position and
 * joystick lines for button presses.
 */

#include "../connector.h"

class MouseDevice : public PeripheralDevice {
public:
    MouseDevice();
    ~MouseDevice() override = default;

    const char* get_name() const override { return "Mouse (1351)"; }
    const char* get_id() const override   { return "mouse_1351"; }
    ConnectorType get_connector_type() const override { return ConnectorType::CONTROL_PORT_DB9; }
    void reset() override;
    uint32_t get_output_signals() const override;

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

    // --- Mouse-specific API --------------------------------------------
    void set_left_button(bool pressed);
    void set_right_button(bool pressed);
    void move(int dx, int dy);  ///< Relative mouse movement

    /// Get current pot X/Y values (0-255), updated by move().
    uint8_t get_pot_x() const { return pot_x_; }
    uint8_t get_pot_y() const { return pot_y_; }

private:
    uint32_t state_;
    uint8_t  pot_x_;
    uint8_t  pot_y_;
    HostInputBinding binding_;
};
