// =============================================================================
// input_registry.cpp — ChipRegistry registration for header-only input chips
// =============================================================================
//
// Input chips without a non-GUI .cpp file need this translation unit to pull in
// their headers and register them in the ChipRegistry.
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"

#include "chip/input/cd4021.hpp"
#include "chip/input/keyboard_encoder.hpp"

REGISTER_CHIP_TYPE("CD4021", CD4021)
REGISTER_CHIP_TYPE("KeyboardEncoder", KeyboardEncoder)
