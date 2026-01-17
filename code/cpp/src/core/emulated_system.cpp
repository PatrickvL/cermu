#include "emulated_system.h"
#include <cstring>
#include <algorithm>
#include <fstream>

SystemRegistry& SystemRegistry::instance() {
    static SystemRegistry registry;
    return registry;
}

void SystemRegistry::register_system(const SystemDescriptor& descriptor, SystemFactory factory) {
    systems_.push_back({descriptor, factory});
}

std::unique_ptr<IEmulatedSystem> SystemRegistry::create_system_for_file(const char* filepath) {
    if (!filepath) {
        return nullptr;
    }
    
    // Read file header for content-based detection
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return nullptr;
    }
    
    // Read first 64KB or entire file, whichever is smaller
    std::vector<uint8_t> data;
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    size_t read_size = std::min<size_t>(file_size, 65536);
    data.resize(read_size);
    file.read(reinterpret_cast<char*>(data.data()), read_size);
    file.close();
    
    // Find system with highest confidence
    float best_confidence = 0.0f;
    SystemFactory best_factory = nullptr;
    
    for (const auto& [descriptor, factory] : systems_) {
        if (descriptor.can_load_file) {
            float confidence = descriptor.can_load_file(filepath, data.data(), file_size);
            if (confidence > best_confidence) {
                best_confidence = confidence;
                best_factory = factory;
            }
        }
    }
    
    // Require at least 50% confidence
    if (best_confidence >= 0.5f && best_factory) {
        return best_factory();
    }
    
    return nullptr;
}

std::unique_ptr<IEmulatedSystem> SystemRegistry::create_system_by_name(const char* short_name) {
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