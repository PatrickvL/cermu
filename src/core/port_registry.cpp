// =============================================================================
// port_registry.cpp — Global port definition registry implementation
// =============================================================================

#include "core/port_registry.hpp"
#include <cstdio>
#include <cstring>

// ============================================================================
// Static sentinel for failed lookups
// ============================================================================

const PortDefinition PortRegistry::kEmpty = {
    PortType::CUSTOM, "Unknown", nullptr, 0, false, false
};

// ============================================================================
// Singleton
// ============================================================================

PortRegistry& PortRegistry::instance() {
    static PortRegistry registry;
    return registry;
}

// ============================================================================
// Registration
// ============================================================================

void PortRegistry::register_port(const PortDefinition& definition) {
    // Reject duplicate types (first registration wins).
    for (const auto& entry : entries_) {
        if (entry.type == definition.type) {
            printf("PortRegistry: WARNING — duplicate type '%s' ignored\n",
                   definition.name ? definition.name : "?");
            return;
        }
    }

    entries_.push_back(definition);
}

// ============================================================================
// Lookup
// ============================================================================

const PortDefinition& PortRegistry::lookup(PortType type) const {
    for (const auto& entry : entries_) {
        if (entry.type == type)
            return entry;
    }
    return kEmpty;
}

const PortDefinition& PortRegistry::lookup(std::string_view name) const {
    for (const auto& entry : entries_) {
        if (entry.name && name == entry.name)
            return entry;
    }
    return kEmpty;
}

bool PortRegistry::has_port(PortType type) const {
    for (const auto& entry : entries_) {
        if (entry.type == type)
            return true;
    }
    return false;
}

size_t PortRegistry::size() const {
    return entries_.size();
}
