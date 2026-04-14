/*
 * coco_system.cpp — TRS-80 Color Computer 1 & 2 system registration
 *
 * Implementation lives in the shared MC6809E + MC6847 VDG system template.
 * This file provides the CoCo-specific system descriptors and registration.
 */

#include "systems/mc6809_vdg/mc6809_vdg_system.hpp"
#include "core/system_registry.hpp"

// ============================================================================
// SYSTEM DESCRIPTORS
// ============================================================================

static SystemDescriptor coco1_descriptor = {
    "TRS-80 CoCo", "CoCo",
    "TRS-80 Color Computer — MC6809E, MC6847, 2×PIA (1980)",
    "coco", {"CoCo", "CoCo1", "Color Computer", "TRS-80 CoCo"},
    nullptr,
    create_mc6809_vdg_hardware_traits<MC6809VDGVariant::COCO1>(),
    nullptr,
    "Tandy/Radio Shack", 1980, "MC6809E", SystemType::Home
};

static SystemDescriptor coco2_descriptor = {
    "TRS-80 CoCo 2", "CoCo2",
    "TRS-80 Color Computer 2 — MC6809E, MC6847, 2×PIA, Extended BASIC (1983)",
    "coco", {"CoCo2", "CoCo 2", "Color Computer 2"},
    nullptr,
    create_mc6809_vdg_hardware_traits<MC6809VDGVariant::COCO2>(),
    nullptr,
    "Tandy/Radio Shack", 1983, "MC6809E", SystemType::Home
};

// ============================================================================
// REGISTRATION
// ============================================================================

REGISTER_SYSTEM(coco1_descriptor, [] {
    return std::make_unique<MC6809VDGSystem<MC6809VDGVariant::COCO1>>();
});

REGISTER_SYSTEM(coco2_descriptor, [] {
    return std::make_unique<MC6809VDGSystem<MC6809VDGVariant::COCO2>>();
});
