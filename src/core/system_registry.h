#pragma once

#include <memory>
#include <string>
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
 * Result of identifying which emulated system best matches a file.
 * Returned by SystemRegistry::identify_system().
 */
struct SystemMatch {
    std::string system_name;   /**< Short name of the best-match system, or "" */
    float       confidence;    /**< Confidence in the match (0.0–1.0) */
};

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

    /**
     * Identify the best emulated system for a file.
     *
     * Iterates every registered system's can_load_file callback and
     * returns the one with the highest confidence.  This is the single
     * authority for "which system handles this file?" — used by both
     * create_system_for_file() and the archive scanner.
     *
     * @param filepath  Path (or VFS path) for extension / context hints
     * @param data      File content (first N bytes or entire file)
     * @param size      Total file size in bytes
     * @return          Best match (may have confidence 0 if nothing matched)
     */
    SystemMatch identify_system(const char* filepath,
                                const uint8_t* data, size_t size) const;
    
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
 * Can be used multiple times in the same file (e.g., for system variants)
 */
#define REGISTER_SYSTEM_CONCAT_IMPL(a, b) a##b
#define REGISTER_SYSTEM_CONCAT(a, b) REGISTER_SYSTEM_CONCAT_IMPL(a, b)
#define REGISTER_SYSTEM(descriptor, factory) \
    namespace { \
        struct REGISTER_SYSTEM_CONCAT(SystemRegistrar_, __LINE__) { \
            REGISTER_SYSTEM_CONCAT(SystemRegistrar_, __LINE__)() { \
                SystemRegistry::instance().register_system(descriptor, factory); \
            } \
        }; \
        static REGISTER_SYSTEM_CONCAT(SystemRegistrar_, __LINE__) \
            REGISTER_SYSTEM_CONCAT(registrar_, __LINE__); \
    }
