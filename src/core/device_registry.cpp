/**
 * device_registry.cpp - Peripheral Device Registry Implementation
 */

#include "core/device_registry.hpp"
#include <cstdio>
#include <cstring>

// ============================================================================
// SINGLETON
// ============================================================================

DeviceRegistry& DeviceRegistry::instance() {
    static DeviceRegistry registry;
    return registry;
}

// ============================================================================
// REGISTRATION
// ============================================================================

void DeviceRegistry::register_device(const DeviceDescriptor& descriptor, DeviceFactory factory) {
    // Check for duplicate ID
    for (const auto& [desc, _] : devices_) {
        if (std::strcmp(desc.id, descriptor.id) == 0) {
            printf("DeviceRegistry: WARNING — duplicate device ID '%s' ignored\n", descriptor.id);
            return;
        }
    }

    devices_.emplace_back(descriptor, std::move(factory));
    printf("DeviceRegistry: Registered device '%s' (%s) for %s\n",
           descriptor.id, descriptor.name,
           port_type_name(descriptor.port_type));
}

// ============================================================================
// QUERIES
// ============================================================================

std::vector<const DeviceDescriptor*> DeviceRegistry::get_compatible_devices(PortType type) const {
    std::vector<const DeviceDescriptor*> result;
    for (const auto& [desc, _] : devices_) {
        if (desc.port_type == type) {
            result.push_back(&desc);
        }
    }
    return result;
}

std::unique_ptr<PeripheralDevice> DeviceRegistry::create_device(const char* device_id) const {
    for (const auto& [desc, factory] : devices_) {
        if (std::strcmp(desc.id, device_id) == 0) {
            return factory();
        }
    }
    printf("DeviceRegistry: Unknown device ID '%s'\n", device_id);
    return nullptr;
}

bool DeviceRegistry::has_device(const char* device_id) const {
    for (const auto& [desc, _] : devices_) {
        if (std::strcmp(desc.id, device_id) == 0) return true;
    }
    return false;
}

const DeviceDescriptor* DeviceRegistry::get_descriptor(const char* device_id) const {
    for (const auto& [desc, _] : devices_) {
        if (std::strcmp(desc.id, device_id) == 0) return &desc;
    }
    return nullptr;
}
