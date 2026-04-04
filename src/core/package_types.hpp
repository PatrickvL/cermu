/*
 * package_types.h - IC Package type definitions and layouts
 * 
 * Physical package characteristics including dimensions, pin arrangements,
 * orientation markers, and standard package configurations.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// ============================================================================
// PACKAGE TYPE ENUMERATIONS
// ============================================================================

// Package type enumeration
enum class PackageType {
    DIP,          // Dual In-line Package (through-hole) - typically 8, 14, 16, 18, 20, 22, 24, 28, 40, 48, 64 pins
    SOIC,         // Small Outline IC - typically 8, 14, 16, 20, 24, 28, 32 pins
    SOP,          // Small Outline Package - typically 8, 14, 16, 20, 24, 28 pins
    SSOP,         // Shrink Small Outline Package - typically 14, 16, 20, 24, 28, 38, 48, 56 pins
    TSSOP,        // Thin Shrink Small Outline Package - typically 14, 16, 20, 24, 28, 38, 48, 56, 64 pins
    PLCC,         // Plastic Leaded Chip Carrier - typically 20, 28, 44, 52, 68, 84 pins
    QFP,          // Quad Flat Package - typically 32, 44, 52, 64, 80, 100, 128, 144, 176, 208 pins
    LQFP,         // Low-profile Quad Flat Package - typically 32, 44, 48, 64, 80, 100, 128, 144, 176 pins
    TQFP,         // Thin Quad Flat Package - typically 32, 44, 48, 64, 80, 100, 144 pins
    QFN,          // Quad Flat No-leads - typically 16, 20, 24, 28, 32, 40, 44, 48, 56, 64, 68, 76 pins
    DFN,          // Dual Flat No-leads - typically 6, 8, 10, 12, 14, 16 pins
    BGA,          // Ball Grid Array - typically 64, 100, 144, 169, 256, 324, 400, 484, 676, 900+ pins
    LGA,          // Land Grid Array - typically 64, 100, 144, 225, 256, 400, 484+ pins
    SIP,          // Single In-line Package - typically 3, 4, 5, 6, 7, 8, 9, 10 pins
    TO220,        // Transistor Outline 220 (power package) - typically 3, 5, 7 pins
    TO92,         // Transistor Outline 92 - typically 3 pins
    SOT23,        // Small Outline Transistor 23 - typically 3, 5, 6, 8 pins
    SOT223,       // Small Outline Transistor 223 - typically 3, 4, 5, 6, 8 pins
    CUSTOM        // Custom package type - variable pin count
};

// Orientation marker types
enum class OrientationMarker {
    NONE,         // No orientation marker
    NOTCH,        // U-shaped cutout at top center
    DOT,          // Dot/dimple near pin 1
    CHAMFER,      // Beveled/cut corner near pin 1
    BAR,          // Bar/stripe indicating pin 1 side
    TRIANGLE,     // Triangle pointing to pin 1
    CIRCLE,       // Circle at pin 1 location
    NOTCH_AND_DOT,// Both notch and dot (common)
    CUSTOM        // Custom marker
};

// ============================================================================
// PACKAGE LAYOUT STRUCTURES
// ============================================================================

// Package layout configuration
struct PackageLayout {
    float width;                     // Package width (mil)
    float height;                    // Package height (mil)
    PackageType package_type;        // Package type
    OrientationMarker marker;        // Orientation marker type
    float pin_pitch;                 // Distance between pins (mil)
    bool has_thermal_pad;            // Package has thermal pad/exposed pad
    bool has_center_slug;            // Package has center slug/tab
    float thermal_pad_size;          // Size of thermal pad (relative to package)
};

// Chip markings/labels — extra text lines rendered on the DIP package visualization.
// Identity fields (part_number, manufacturer) come from ChipLayout::chip_info instead.
// Empty string_view = not shown.
struct ChipMarkings {
    std::string_view package_variant;    // "C", "N", "W", or memory type
    std::string_view custom_text;        // Freeform extra line
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Enum-to-string conversion functions
const std::string get_package_type_string(PackageType package_type);

