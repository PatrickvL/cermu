#include "system_registry.h"
#include "emulated_system.h"
#include "formats/format_handler.h"
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
// identify_system - two-phase file-to-system matching
//
// Phase 1 (generic):  For systems with supported_formats, run each format's
//                     identify() callback.  Skip the system if nothing matches.
// Phase 2 (specific): Call the system's probe_file() to get confidence + config.
// ============================================================================

SystemMatch SystemRegistry::identify_system(const char* filepath,
                                            const uint8_t* data, size_t size) const {
    SystemMatch best;

    // Extract extension once for all format identify() calls
    std::string ext_str = filepath ? vfs_extension(filepath) : "";
    const char* ext = ext_str.empty() ? nullptr : ext_str.c_str();

    for (const auto& [descriptor, factory] : systems_) {
        if (!descriptor.probe_file) continue;

        const format_descriptor_t* matched_format = nullptr;
        float best_format_score = 0.0f;

        if (descriptor.supported_formats) {
            // Phase 1: generic format gatekeeper
            for (const format_descriptor_t* const* fp = descriptor.supported_formats; *fp; ++fp) {
                const format_descriptor_t* fmt = *fp;
                if (!fmt->identify) continue;

                float score = fmt->identify(data, size, ext);
                if (score > best_format_score) {
                    best_format_score = score;
                    matched_format    = fmt;
                }
            }

            // No format matched - skip this system entirely
            if (!matched_format) continue;
        }
        // else: supported_formats == nullptr -> probe unconditionally

        // Phase 2: system-specific probe
        SystemProbeResult probe = descriptor.probe_file(matched_format, filepath, data, size);

        if (probe.confidence > best.confidence) {
            best.confidence     = probe.confidence;
            best.system_name    = descriptor.short_name;
            best.matched_format = matched_format;
            best.configuration  = probe.configuration;
        }
    }

    return best;
}

// ============================================================================
// create_system_for_file - read file, identify system, instantiate
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

    // Delegate to the two-phase identification authority
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
            // Apply the configuration determined during identification
            system->set_configuration(match.configuration);
            system->apply_configuration();
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
