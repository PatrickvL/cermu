#pragma once
/**
 * paddle_device.h - Paddle Controller Peripheral
 *
 * Emulates a Commodore paddle controller pair that plugs into a
 * Control Port (DB-9).  Each paddle has a potentiometer (0-255)
 * and a fire button.
 */

#include "../connector.h"

class PaddleDevice : public PeripheralDevice {
public:
    PaddleDevice();
    ~PaddleDevice() override = default;

    const char* get_name() const override { return "Paddles"; }
    const char* get_id() const override   { return "paddles"; }
    ConnectorType get_connector_type() const override { return ConnectorType::CONTROL_PORT_DB9; }
    void reset() override;
    uint32_t get_output_signals() const override;

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
    void set_fire_x(bool pressed);
    void set_fire_y(bool pressed);

    uint8_t get_pot_x() const { return pot_x_; }
    uint8_t get_pot_y() const { return pot_y_; }

private:
    uint32_t state_;
    uint8_t  pot_x_;
    uint8_t  pot_y_;
    HostInputBinding binding_;
};
