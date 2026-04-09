/*
 * dragon_system.cpp — Dragon 32/64 system registration
 *
 * Implementation lives in the shared MC6809E + MC6847 VDG system template.
 * This file provides the Dragon-specific system descriptors and registration.
 */

#include "systems/mc6809_vdg/mc6809_vdg_system.hpp"
#include "core/system_registry.hpp"

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor dragon32_descriptor = {
    "Dragon 32", "Dragon 32",
    "Dragon 32 — MC6809E, MC6847, 2×PIA, 32KB RAM (1982)",
    "dragon", {"Dragon 32", "Dragon32", "Dragon"},
    nullptr,
    create_mc6809_vdg_hardware_traits<MC6809VDGVariant::DRAGON32>(),
    nullptr,
    "Dragon Data", 1982, "MC6809E", SystemType::Home
};

static SystemDescriptor dragon64_descriptor = {
    "Dragon 64", "Dragon 64",
    "Dragon 64 — MC6809E, MC6847, 2×PIA, 64KB RAM (1983)",
    "dragon", {"Dragon 64", "Dragon64"},
    nullptr,
    create_mc6809_vdg_hardware_traits<MC6809VDGVariant::DRAGON64>(),
    nullptr,
    "Dragon Data", 1983, "MC6809E", SystemType::Home
};

// ============================================================================
// REGISTRATION
// ============================================================================

REGISTER_SYSTEM(dragon32_descriptor, [] {
    return std::make_unique<MC6809VDGSystem<MC6809VDGVariant::DRAGON32>>();
});

REGISTER_SYSTEM(dragon64_descriptor, [] {
    return std::make_unique<MC6809VDGSystem<MC6809VDGVariant::DRAGON64>>();
});
