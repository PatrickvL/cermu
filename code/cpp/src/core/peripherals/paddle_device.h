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
};
