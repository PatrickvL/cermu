#pragma once
/**
 * device_registry.h - Peripheral Device Registry
 *
 * Maintains a global catalogue of all known peripheral device types that can be
 * attached to emulated systems.  Each device type self-registers during static
 * initialisation (using REGISTER_DEVICE), making it available to any system
 * whose connector ports are type-compatible.
 *
 * The registry provides:
 * - Enumeration of all registered devices
 * - Querying devices compatible with a given ConnectorType
 * - Factory creation of device instances by ID
 */

#include "connector.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// DEVICE DESCRIPTOR
// ============================================================================

/**
 * Static metadata for a peripheral device type.
 * Provided once per device class; the registry stores a copy.
 */
struct DeviceDescriptor {
    const char*     id;             ///< Unique machine-readable ID (e.g. "joystick", "1541")
    const char*     name;           ///< Human-readable display name
    const char*     description;    ///< Brief description / tooltip text
    ConnectorType   connector_type; ///< Required connector type
    bool            is_bus_device;  ///< True if multiple instances share one bus (e.g. IEC devices)
};

// ============================================================================
// DEVICE FACTORY
// ============================================================================

/// Factory function that creates a new PeripheralDevice instance.
using DeviceFactory = std::function<std::unique_ptr<PeripheralDevice>()>;

// ============================================================================
// DEVICE REGISTRY
// ============================================================================

/**
 * Singleton registry of all peripheral device types known to the emulator.
 *
 * Usage:
 *   // Query compatible devices for a port
 *   auto devices = DeviceRegistry::instance().get_compatible_devices(ConnectorType::CONTROL_PORT_DB9);
 *
 *   // Create a device by ID
 *   auto joy = DeviceRegistry::instance().create_device("joystick");
 */
class DeviceRegistry {
public:
    static DeviceRegistry& instance();

    /// Register a new device type (called during static initialisation).
    void register_device(const DeviceDescriptor& descriptor, DeviceFactory factory);

    /// Get all registered device descriptors.
    const std::vector<std::pair<DeviceDescriptor, DeviceFactory>>& get_all_devices() const {
        return devices_;
    }

    /// Get all device descriptors compatible with a given connector type.
    std::vector<const DeviceDescriptor*> get_compatible_devices(ConnectorType type) const;

    /// Create a device instance by its unique ID.  Returns nullptr if not found.
    std::unique_ptr<PeripheralDevice> create_device(const char* device_id) const;

    /// Check if a device ID is registered.
    bool has_device(const char* device_id) const;

    /// Get a descriptor by ID.  Returns nullptr if not found.
    const DeviceDescriptor* get_descriptor(const char* device_id) const;

private:
    DeviceRegistry() = default;
    std::vector<std::pair<DeviceDescriptor, DeviceFactory>> devices_;
};

// ============================================================================
// SELF-REGISTRATION MACRO
// ============================================================================

/**
 * Place this in a device's .cpp file to auto-register it at programme start.
 *
 * Example:
 *   REGISTER_DEVICE(joystick_descriptor, []() {
 *       return std::make_unique<JoystickDevice>();
 *   })
 */
#define REGISTER_DEVICE(descriptor, factory) \
    namespace { \
        struct DeviceRegistrar_##__LINE__ { \
            DeviceRegistrar_##__LINE__() { \
                DeviceRegistry::instance().register_device(descriptor, factory); \
            } \
        }; \
        static DeviceRegistrar_##__LINE__ s_device_registrar_##__LINE__; \
    }
