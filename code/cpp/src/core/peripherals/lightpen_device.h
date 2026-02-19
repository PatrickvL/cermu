#pragma once
/**
 * lightpen_device.h - Light Pen Peripheral
 *
 * Emulates a light pen that plugs into Control Port 1 (DB-9).
 * Triggers VIC-II's lightpen latch when the pen detects the CRT beam
 * at its position.
 */

#include "../connector.h"

class LightpenDevice : public PeripheralDevice {
public:
    LightpenDevice();
    ~LightpenDevice() override = default;

    const char* get_name() const override { return "Light Pen"; }
    const char* get_id() const override   { return "lightpen"; }
    ConnectorType get_connector_type() const override { return ConnectorType::CONTROL_PORT_DB9; }
    void reset() override;
    uint32_t get_output_signals() const override;

    /// Set pen X/Y coordinates (system display coordinates).
    void set_position(int x, int y) { pen_x_ = x; pen_y_ = y; }

    /// Trigger the lightpen (pen touching screen).
    void set_triggered(bool active);

    int get_pen_x() const { return pen_x_; }
    int get_pen_y() const { return pen_y_; }
    bool is_triggered() const { return triggered_; }

private:
    uint32_t state_;
    int      pen_x_;
    int      pen_y_;
    bool     triggered_;
};
