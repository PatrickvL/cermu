#pragma once
// =============================================================================
// port_registry.hpp — Global port definition registry
// =============================================================================
//
// Maps PortType enum values (and string names) to PortDefinition
// descriptors.  Standard port types self-register from port.cpp
// via the REGISTER_PORT macro during static initialisation.
//
// Relationship with Port:
//   Port is constructed from a PortDefinition.  The registry
//   provides a central place to look up definitions by type or name, so
//   boards can create ports without hard-coding signal tables inline.
//
// =============================================================================

#include "core/port.hpp"   // PortType, PortDefinition
#include <string_view>
#include <vector>

// =============================================================================
// PortRegistry — singleton definition registry
// =============================================================================

class PortRegistry {
public:
    /// Access the singleton.
    static PortRegistry& instance();

    /// Register a connector port definition.
    /// Duplicate types are rejected with a warning (first registration wins).
    void register_port(const PortDefinition& definition);

    /// Look up a definition by connector type.
    /// Returns a default empty definition if not found.
    [[nodiscard]] const PortDefinition& lookup(PortType type) const;

    /// Look up a definition by human-readable name (case-sensitive).
    /// Returns a default empty definition if not found.
    [[nodiscard]] const PortDefinition& lookup(std::string_view name) const;

    /// Check if a connector port type is registered.
    [[nodiscard]] bool has_port(PortType type) const;

    /// Number of registered connector definitions.
    [[nodiscard]] size_t size() const;

    /// Iterate all registrations (for diagnostics / tooling).
    [[nodiscard]] const std::vector<PortDefinition>& entries() const { return entries_; }

private:
    PortRegistry() = default;
    std::vector<PortDefinition> entries_;

    static const PortDefinition kEmpty;
};

// =============================================================================
// REGISTER_PORT — self-registration macro
// =============================================================================
//
// Place in a .cpp file to auto-register a PortDefinition at programme
// start.  Can be used multiple times in the same translation unit.
//
// Usage:
//   REGISTER_PORT(PortDefinition{
//       PortType::CONTROL_PORT_DB9,
//       "Control Port (DB-9)",
//       PortSignals::CONTROL_PORT_SIGNALS,
//       PortSignals::CONTROL_PORT_SIGNAL_COUNT,
//       false, false
//   })
//

#define REGISTER_PORT_CONCAT_IMPL_(a, b) a##b
#define REGISTER_PORT_CONCAT_(a, b) REGISTER_PORT_CONCAT_IMPL_(a, b)

#define REGISTER_PORT(...) \
    namespace { \
        struct REGISTER_PORT_CONCAT_(PortRegistrar_, __LINE__) { \
            REGISTER_PORT_CONCAT_(PortRegistrar_, __LINE__)() { \
                PortRegistry::instance().register_port(__VA_ARGS__); \
            } \
        }; \
        static REGISTER_PORT_CONCAT_(PortRegistrar_, __LINE__) \
            REGISTER_PORT_CONCAT_(port_registrar_, __LINE__); \
    }
