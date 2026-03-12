/**
 * commodore_keyboard_device.cpp - Commodore Keyboard Internal Peripheral
 */

#include "devices/keyboard/commodore_keyboard_device.h"
#include "core/device_registry.h"

CommodoreKeyboardDevice::CommodoreKeyboardDevice(commodore_keyboard_t* keyboard)
    : keyboard_(keyboard)
{
    binding_.type  = HostInputType::KEYBOARD;
    binding_.label = "Host Keyboard";
}

void CommodoreKeyboardDevice::reset() {
    // Keyboard reset is handled by the system, not by this wrapper.
}

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor keyboard_descriptor = {
    "commodore_keyboard",
    "Keyboard",
    "Commodore keyboard matrix — internal device, always attached",
    ConnectorType::CUSTOM,
    false
};

REGISTER_DEVICE(keyboard_descriptor, []() {
    return std::make_unique<CommodoreKeyboardDevice>();
})
