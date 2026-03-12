#pragma once
// =============================================================================
// chip_registry.h — Global chip factory registry
// =============================================================================
//
// Maps string names (e.g. "MOS6502", "MOS6581") to factory functions.
// Enables file-driven system declarations (TOML/LJON) to resolve chip types
// by name at runtime, sharing the same factory function signature that
// compile-time Slot<T> / make_chip_manifest() produces.
//
// Self-registration:
//   Chips register via a static-initialiser macro placed in their .cpp file.
//   By the time main() runs, all compiled-in chips are available for lookup.
//
// Relationship with Slot<T>:
//   Slot<T> resolves factories at compile time via resolve_slot_factory<T>().
//   ChipRegistry resolves factories at runtime via lookup(name).
//   Both produce ChipSlot::FactoryFn — same type, same contract.
//   Slot<T> is syntactic sugar; the registry is the general-purpose path.
//
// =============================================================================

#include "core/chip.hpp"          // ChipBase, bus_state_t
#include <string_view>

// Forward declaration — full definition in chip_manifest.hpp.
// Only needed for the function pointer signature, not for the type's layout.
struct ChipSlot;

// =============================================================================
// ChipRegistry — singleton factory registry
// =============================================================================

class ChipRegistry {
public:
    /// Factory function signature — identical to ChipSlot::FactoryFn.
    using FactoryFn = ChipBase* (*)(const ChipSlot& slot,
                                    const bus_state_t* system_bus,
                                    uint8_t* buffer);

    /// Access the singleton.
    static ChipRegistry& instance();

    /// Register a chip type under the given name.
    /// Duplicate names are rejected with a warning (first registration wins).
    void register_chip(std::string_view name, FactoryFn factory);

    /// Look up a factory by name.  Returns nullptr if unknown.
    [[nodiscard]] FactoryFn lookup(std::string_view name) const;

    /// Check if a chip name is registered.
    [[nodiscard]] bool has_chip(std::string_view name) const;

    /// Number of registered chip types.
    [[nodiscard]] size_t size() const;

    /// Iterate all registrations (for diagnostics / tooling).
    struct Entry {
        std::string_view name;
        FactoryFn        factory;
    };
    [[nodiscard]] const std::vector<Entry>& entries() const { return entries_; }

private:
    ChipRegistry() = default;
    std::vector<Entry> entries_;
};

// =============================================================================
// REGISTER_CHIP — self-registration macro
// =============================================================================
//
// Place in a chip's .cpp file to auto-register its factory at programme start.
// Can be used multiple times in the same translation unit (unique per __LINE__).
//
// Usage with an explicit factory function:
//   REGISTER_CHIP("MOS6502", &MOS6502::create_from_slot)
//
// Usage with a type that satisfies SlotCreatable or is default-constructible:
//   REGISTER_CHIP_TYPE("MOS6502", MOS6502)
//   (requires #include "core/chip_manifest.hpp" for resolve_slot_factory<T>())
//

#define REGISTER_CHIP_CONCAT_IMPL_(a, b) a##b
#define REGISTER_CHIP_CONCAT_(a, b) REGISTER_CHIP_CONCAT_IMPL_(a, b)

#define REGISTER_CHIP(name, factory) \
    namespace { \
        struct REGISTER_CHIP_CONCAT_(ChipRegistrar_, __LINE__) { \
            REGISTER_CHIP_CONCAT_(ChipRegistrar_, __LINE__)() { \
                ChipRegistry::instance().register_chip(name, factory); \
            } \
        }; \
        static REGISTER_CHIP_CONCAT_(ChipRegistrar_, __LINE__) \
            REGISTER_CHIP_CONCAT_(chip_registrar_, __LINE__); \
    }

/// Type-based registration — resolves the factory via the same compile-time
/// chain that Slot<T> uses (SlotCreatable → default construction → nullptr).
/// Requires chip_manifest.hpp to be included for resolve_slot_factory<T>().
#define REGISTER_CHIP_TYPE(name, Type) \
    REGISTER_CHIP(name, resolve_slot_factory<Type>())


// =============================================================================
// Runtime slot creation from the registry
// =============================================================================
//
// Bridges file-driven declarations (TOML/LJON) to ChipSlot.  A parsed system
// description provides the chip name as a string; this function resolves the
// factory via ChipRegistry and returns a fully populated ChipSlot.
//
// Returns a slot with factory == nullptr if the name is not registered.
// The caller must include chip_manifest.hpp for the full ChipSlot definition.
// This function is intentionally a template so that it compiles without
// ChipSlot being complete in this header (deferred instantiation).
//
//   #include "core/chip_manifest.hpp"   // for ChipSlot
//   #include "core/chip_registry.hpp"
//   ChipSlot slot = make_slot_from_registry("MOS6502", 0, 0);
//

template<typename Slot = ChipSlot>
inline Slot make_slot_from_registry(
    std::string_view name,
    uint32_t base_addr,
    size_t   size_bytes,
    uint32_t addr_mask  = 0,
    const char* label   = nullptr,
    uint16_t condition  = 0)
{
    return Slot{
        base_addr, size_bytes, addr_mask,
        ChipRegistry::instance().lookup(name),
        label, condition
    };
}
