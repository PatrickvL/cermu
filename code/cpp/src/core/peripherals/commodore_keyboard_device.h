#pragma once
/**
 * commodore_keyboard_device.h - Commodore Keyboard as Internal Peripheral
 *
 * Thin PeripheralDevice wrapper around the existing commodore_keyboard_t.
 * This is an *internal* device (always attached, never swappable), providing
 * a uniform representation in the connector/peripheral hierarchy.
 *
 * SDL key events are still routed through the KeyboardMapper — this device
 * does NOT handle SDL events directly.
 */

#include "../connector.h"
#include "../../chip/input/commodore_keyboard.h"

class CommodoreKeyboardDevice : public PeripheralDevice {
public:
    explicit CommodoreKeyboardDevice(commodore_keyboard_t* keyboard = nullptr);
    ~CommodoreKeyboardDevice() override = default;

    const char* get_name() const override { return "Keyboard"; }
    const char* get_id() const override   { return "commodore_keyboard"; }
    ConnectorType get_connector_type() const override { return ConnectorType::CUSTOM; }
    void reset() override;
    uint32_t get_output_signals() const override { return 0xFFFFFFFF; }

    // This device accepts host keyboard input, but routing is handled
    // externally by the KeyboardMapper.  We report it here so the
    // peripheral UI can show "Host Keyboard" as the input source.
    bool accepts_host_input() const override { return true; }
    int  get_supported_input_type_count() const override { return 1; }
    HostInputType get_supported_input_type(int index) const override {
        (void)index; return HostInputType::KEYBOARD;
    }
    const HostInputBinding& get_host_input_binding() const override { return binding_; }
    void set_host_input_binding(const HostInputBinding& binding) override { binding_ = binding; }

    /// Set/get the underlying keyboard
    void set_keyboard(commodore_keyboard_t* kb) { keyboard_ = kb; }
    commodore_keyboard_t* get_keyboard() const   { return keyboard_; }

private:
    commodore_keyboard_t* keyboard_;
    HostInputBinding binding_;
};
