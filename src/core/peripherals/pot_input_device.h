#pragma once
/**
 * pot_input_device.h - Base class for Control Port devices with SID POT analog input
 *
 * Extends ControlPortInputDevice with potentiometer (analog) value tracking.
 * The C64 SID chip reads POT values from pins 5 (POTX) and 9 (POTY) on
 * the Control Port DB-9 connector.  Values range 0-255.
 *
 * Devices that use POT inputs:
 *   - Commodore 1351 Mouse (proportional mouse, motion → POT delta)
 *   - Commodore 1312 Paddles (analog paddle pair, position → POT value)
 *
 * The system reads pot_x_ / pot_y_ via get_pot_x() / get_pot_y()
 * and feeds them to the SID's POTX ($D419) / POTY ($D41A) registers.
 */

#include "control_port_device.h"

class PotInputDevice : public ControlPortInputDevice {
public:
    PotInputDevice() : pot_x_(128), pot_y_(128) {}
    ~PotInputDevice() override = default;

    /// Get current POT X value (0-255).
    uint8_t get_pot_x() const { return pot_x_; }
    /// Get current POT Y value (0-255).
    uint8_t get_pot_y() const { return pot_y_; }

    void reset() override {
        ControlPortInputDevice::reset();
        pot_x_ = 128;
        pot_y_ = 128;
    }

protected:
    uint8_t pot_x_;  ///< POT X value (0-255), maps to SID POTX ($D419)
    uint8_t pot_y_;  ///< POT Y value (0-255), maps to SID POTY ($D41A)
};
