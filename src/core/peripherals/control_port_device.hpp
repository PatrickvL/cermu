#pragma once
/**
 * control_port_device.h - Base class for Control Port (DB-9) input devices
 *
 * Provides shared infrastructure for all devices that plug into a Commodore
 * Control Port and manipulate the digital signal lines (active-low, wired-AND):
 *
 *   - Digital joysticks (Atari-compatible)
 *   - Commodore 1350 mouse (digital joystick emulation)
 *   - NEOS mouse (bit-bang protocol via CIA port lines)
 *   - Light pen (LP trigger on pin 6)
 *
 * Shared code:
 *   - Active-low signal state management
 *   - set_signal() / clear_signal() with port notification
 *   - Standard reset (all lines released)
 *   - get_output_signals() returning cached state
 *
 * Devices that also use SID POT analog inputs (1351 mouse, paddles)
 * inherit from PotInputDevice instead.
 */

#include "core/peripherals/input_peripheral_device.hpp"

class ControlPortInputDevice : public InputPeripheralDevice {
public:
    ControlPortInputDevice() : signal_state_(0xFFFFFFFF) {}
    ~ControlPortInputDevice() override = default;

    ConnectorType get_connector_type() const override { return ConnectorType::CONTROL_PORT_DB9; }

    void reset() override {
        signal_state_ = 0xFFFFFFFF;  // All lines released (idle)
        if (port_) port_->notify_device_output_changed(signal_state_);
    }

    uint32_t get_output_signals() const override { return signal_state_; }

    /// Override: release all active-low signals and notify the port before
    /// the binding changes.  Subclasses that need additional cleanup (e.g.
    /// resetting accumulators) override on_input_source_will_change() instead.
    void set_host_input_binding(const HostInputBinding& binding) override {
        release_all_signals();
        on_input_source_will_change();
        notify_port();
        binding_ = binding;
        printf("%s: Input source changed to %s\n", get_name(), binding_.label.c_str());
    }

protected:
    /// Set a signal line LOW (active / asserted).
    void assert_signal(uint8_t bit_index) {
        signal_state_ &= ~(1u << bit_index);
        if (port_) port_->notify_device_output_changed(signal_state_);
    }

    /// Set a signal line HIGH (released / inactive).
    void release_signal(uint8_t bit_index) {
        signal_state_ |= (1u << bit_index);
        if (port_) port_->notify_device_output_changed(signal_state_);
    }

    /// Set a signal line to a given state (pressed=true → LOW).
    void set_signal(uint8_t bit_index, bool pressed) {
        if (pressed)
            signal_state_ &= ~(1u << bit_index);
        else
            signal_state_ |= (1u << bit_index);
        if (port_) port_->notify_device_output_changed(signal_state_);
    }

    /// Set multiple lines at once without per-line notification.
    /// Call notify_port() after batch updates.
    void set_signal_batch(uint8_t bit_index, bool pressed) {
        if (pressed)
            signal_state_ &= ~(1u << bit_index);
        else
            signal_state_ |= (1u << bit_index);
    }

    /// Notify the port of signal changes (used after batch updates).
    void notify_port() {
        if (port_) port_->notify_device_output_changed(signal_state_);
    }

    /// Release all signal lines (set all HIGH).
    void release_all_signals() {
        signal_state_ = 0xFFFFFFFF;
    }

    /// Check if a signal line is currently asserted (LOW).
    bool is_signal_asserted(uint8_t bit_index) const {
        return !(signal_state_ & (1u << bit_index));
    }

    uint32_t signal_state_;  ///< Active-low signal state for all port lines
};
