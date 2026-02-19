#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <utility>

// Forward declarations
class EmulatedSystem;
struct SystemDescriptor;

/**
 * System factory function type
 * Each system provides a factory function to create instances
 */
using SystemFactory = std::function<std::unique_ptr<EmulatedSystem>()>;

/**
 * System registry - maintains list of available emulated systems
 * Systems self-register during static initialization
 */
class SystemRegistry {
public:
    static SystemRegistry& instance();
    
    // Register a new system (called during static initialization by system implementations)
    void register_system(const SystemDescriptor& descriptor, SystemFactory factory);
    
    // Get all registered systems
    const std::vector<std::pair<SystemDescriptor, SystemFactory>>& get_systems() const {
        return systems_;
    }
    
    // Get all system descriptors (for listing available systems)
    std::vector<SystemDescriptor> get_all_descriptors() const;
    
    // Find best system for a file (returns nullptr if no suitable system found)
    std::unique_ptr<EmulatedSystem> create_system_for_file(const char* filepath);
    
    // Create a specific system by short name (e.g., "C64", "CHIP8")
    std::unique_ptr<EmulatedSystem> create_system_by_name(const char* short_name);
    
    // Alias for consistency
    std::unique_ptr<EmulatedSystem> create_system(const char* short_name) {
        return create_system_by_name(short_name);
    }
    
private:
    SystemRegistry() = default;
    std::vector<std::pair<SystemDescriptor, SystemFactory>> systems_;
};

/**
 * Helper macro for system registration
 * Place this in each system's .cpp file to auto-register the system
 */
#define REGISTER_SYSTEM(descriptor, factory) \
    namespace { \
        struct SystemRegistrar { \
            SystemRegistrar() { \
                SystemRegistry::instance().register_system(descriptor, factory); \
            } \
        }; \
        static SystemRegistrar registrar; \
    }
