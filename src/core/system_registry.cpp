#include "system_registry.h"
#include "emulated_system.h"
#include "vfs/vfs.h"
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// SystemRegistry Implementation
// ============================================================================

SystemRegistry& SystemRegistry::instance() {
    static SystemRegistry registry;
    return registry;
}

void SystemRegistry::register_system(const SystemDescriptor& descriptor, SystemFactory factory) {
    printf("SystemRegistry: Registering system: %s (%s)\n", descriptor.name, descriptor.short_name);
    systems_.push_back({descriptor, factory});
}

// ============================================================================
// identify_system — single authority for file-to-system matching
// ============================================================================

SystemMatch SystemRegistry::identify_system(const char* filepath,
                                            const uint8_t* data, size_t size) const {
    SystemMatch best;

    for (const auto& [descriptor, factory] : systems_) {
        if (!descriptor.can_load_file) continue;

        float confidence = descriptor.can_load_file(filepath, data, size);
        if (confidence > best.confidence) {
            best.confidence   = confidence;
            best.system_name  = descriptor.short_name;
        }
    }

    return best;
}

// ============================================================================
// create_system_for_file — read file, identify system, instantiate
// ============================================================================

std::unique_ptr<EmulatedSystem> SystemRegistry::create_system_for_file(const char* filepath) {
    if (!filepath) {
        return nullptr;
    }
    
    printf("SystemRegistry: %zu systems registered\n", systems_.size());
    
    // Read file content via VFS (handles both filesystem and archive paths)
    size_t file_size = 0;
    uint8_t* data = vfs_read_file(filepath, &file_size);
    if (!data) {
        printf("SystemRegistry: Failed to open file: %s\n", filepath);
        return nullptr;
    }
    
    printf("SystemRegistry: File size: %zu bytes\n", file_size);
    
    // Delegate to the single identification authority
    auto match = identify_system(filepath, data, file_size);
    free(data);

    printf("SystemRegistry: Best match: %s (confidence: %.2f)\n",
           match.system_name.empty() ? "none" : match.system_name.c_str(),
           match.confidence);
    
    // Require at least 50% confidence
    if (match.confidence < 0.5f) return nullptr;

    // Find the factory for the winning system
    for (const auto& [descriptor, factory] : systems_) {
        if (match.system_name == descriptor.short_name) {
            auto system = factory();
            system->apply_file_configuration(filepath);
            return system;
        }
    }
    
    return nullptr;
}

std::unique_ptr<EmulatedSystem> SystemRegistry::create_system_by_name(const char* short_name) {
    if (!short_name) {
        return nullptr;
    }
    
    for (const auto& [descriptor, factory] : systems_) {
        if (strcmp(descriptor.short_name, short_name) == 0) {
            return factory();
        }
    }
    
    return nullptr;
}

std::vector<SystemDescriptor> SystemRegistry::get_all_descriptors() const {
    std::vector<SystemDescriptor> descriptors;
    for (const auto& pair : systems_) {
        descriptors.push_back(pair.first);
    }
    return descriptors;
}