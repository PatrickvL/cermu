/*
 * chip_layout.h - Main chip layout header (consolidated view)
 *
 * This header provides a consolidated include of all chip layout components.
 * Individual components are now separated into focused header files.
 */

#ifndef CHIP_LAYOUT_H
#define CHIP_LAYOUT_H

// Include all separated chip layout components
#include "pin_types.h"
#include "package_types.h"
#include "pin_macros.h"
#include <vector>
#include <string>

// ============================================================================
// COMPLETE PIN LAYOUT STRUCTURE
// ============================================================================

// Complete pin layout structure
struct ChipLayout {
    PackageLayout package;           // Package dimensions and characteristics
    std::vector<ChipPin> left_pins;  // Left side pins (top to bottom)
    std::vector<ChipPin> right_pins; // Right side pins (top to bottom)
    std::vector<ChipPin> top_pins;   // Top side pins (left to right) 
    std::vector<ChipPin> bottom_pins;// Bottom side pins (left to right)
    std::vector<ChipPin> grid_pins;  // Grid pins for BGA packages
    ChipMarkings markings;           // Chip text markings
    
    // Calculate total number of pins from all sides and grid
    size_t get_total_pins() const {
        return left_pins.size() + right_pins.size() + top_pins.size() + bottom_pins.size() + grid_pins.size();
    }
    
    // Generate package name from type and pin count
    std::string get_package_name() const;
};

// ============================================================================
// STANDARD PACKAGE LAYOUT FUNCTIONS
// ============================================================================

// Create standard DIP package layouts
ChipLayout create_dip40_layout();
ChipLayout create_dip28_layout();
ChipLayout create_dip24_layout();
ChipLayout create_dip20_layout();
ChipLayout create_dip18_layout();
ChipLayout create_dip16_layout();
ChipLayout create_dip14_layout();
ChipLayout create_dip8_layout();

// Create SOIC/SOP package layouts
ChipLayout create_soic8_layout();
ChipLayout create_soic14_layout();
ChipLayout create_soic16_layout();
ChipLayout create_soic28_layout();

// Create PLCC package layouts (pins on all 4 sides)
ChipLayout create_plcc28_layout();
ChipLayout create_plcc44_layout();
ChipLayout create_plcc68_layout();

// Create QFP package layouts (pins on all 4 sides)
ChipLayout create_qfp32_layout();
ChipLayout create_qfp44_layout();
ChipLayout create_qfp64_layout();
ChipLayout create_qfp100_layout();
ChipLayout create_qfp144_layout();

// Create QFN package layouts
ChipLayout create_qfn16_layout();
ChipLayout create_qfn24_layout();
ChipLayout create_qfn32_layout();
ChipLayout create_qfn48_layout();

// Create BGA package layouts
ChipLayout create_bga64_layout();
ChipLayout create_bga100_layout();
ChipLayout create_bga256_layout();

// Create power package layouts
ChipLayout create_to220_layout();   // 3-pin power regulator
ChipLayout create_to92_layout();    // 3-pin small transistor
ChipLayout create_sot23_layout();   // 3/5/6-pin small transistor
ChipLayout create_sot223_layout();  // Power package

// Create SIP layouts
ChipLayout create_sip8_layout();
ChipLayout create_sip9_layout();

// Helper functions for building custom layouts
ChipLayout create_custom_dip(uint8_t total_pins, const char* part_name = nullptr);
ChipLayout create_custom_qfp(uint8_t total_pins, const char* part_name = nullptr);
ChipLayout create_custom_bga(uint8_t rows, uint8_t cols, const char* part_name = nullptr);

#endif // CHIP_LAYOUT_H