// =============================================================================
// chip_registry.cpp — Global chip factory registry implementation
// =============================================================================

#include "core/chip_registry.hpp"
#include <cstdio>

// ============================================================================
// Singleton
// ============================================================================

ChipRegistry& ChipRegistry::instance() {
    static ChipRegistry registry;
    return registry;
}

// ============================================================================
// Registration
// ============================================================================

void ChipRegistry::register_chip(std::string_view name, FactoryFn factory) {
    // Reject null factories — they can't create anything.
    if (!factory) {
        printf("ChipRegistry: WARNING — null factory for '%.*s' ignored\n",
               int(name.size()), name.data());
        return;
    }

    // Reject duplicate names (first registration wins).
    for (const auto& entry : entries_) {
        if (entry.name == name) {
            printf("ChipRegistry: WARNING — duplicate chip name '%.*s' ignored\n",
                   int(name.size()), name.data());
            return;
        }
    }

    entries_.push_back({name, factory});
}

// ============================================================================
// Lookup
// ============================================================================

ChipRegistry::FactoryFn ChipRegistry::lookup(std::string_view name) const {
    for (const auto& entry : entries_) {
        if (entry.name == name)
            return entry.factory;
    }
    return nullptr;
}

bool ChipRegistry::has_chip(std::string_view name) const {
    return lookup(name) != nullptr;
}

size_t ChipRegistry::size() const {
    return entries_.size();
}
