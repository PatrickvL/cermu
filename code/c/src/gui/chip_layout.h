/*
 * chip_layout.h - Generic chip layout declarations for IC packages
 * 
 * Support for:
 * - Multiple orientation markers (notch, dot, chamfer, bar, triangle)
 * - Various package types (DIP, SOIC, PLCC, QFP, QFN, BGA, TO, SOT)
 * - Chip markings (part number, manufacturer, date code, lot)
 * - Flexible pin notation styles
 * - Bus grouping and differential pairs
 * - Thermal pads and exposed pads
 */

#ifndef CHIP_LAYOUT_H
#define CHIP_LAYOUT_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <string>

#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>

// ============================================================================
// PIN TYPES AND STRUCTURES
// ============================================================================

// Pin types for visual categorization and color coding
enum class PinType {
    POWER,        // VCC, VSS, VDD, GND, AVDD, DVDD, etc.
    CLOCK,        // Clock inputs/outputs (φ0, φ1, φ2, CLK, XTAL, etc.)
    ADDRESS,      // Address bus lines (A0-A23, ADDR, etc.)
    DATA,         // Data bus lines (D0-D15, DATA, etc.)
    CONTROL,      // Control signals (RW, SYNC, RDY, AEC, CS, OE, WE, etc.)
    INTERRUPT,    // Interrupt lines (IRQ, NMI, RES, INT, INTR, etc.)
    SPECIAL,      // Special purpose (SO, BE, ML, NC, TEST, etc.)
    IO_PORT,      // I/O port lines (P0-P7, PA0-PA7, GPIO, etc.)
    ANALOG,       // Analog signals (AIN, AOUT, VREF, etc.)
    DIFFERENTIAL, // Differential pairs (TX+/TX-, RX+/RX-, CLK+/CLK-)
    NO_CONNECT    // Explicitly no-connect pins
};

// Pin side enumeration for package layout
enum class PinSide {
    LEFT,         // Left side pins (top to bottom)
    RIGHT,        // Right side pins (top to bottom) 
    TOP,          // Top side pins (left to right)
    BOTTOM        // Bottom side pins (left to right)
};

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

// Pin definition structure
struct ChipPin {
    uint8_t pin_number;      // Physical pin number (or grid position for BGA)
    const char* label;       // Pin label (e.g., "A0", "D7", "RW")
    PinType type;            // Pin type for color coding
    uint8_t bit_index;       // Bit index within bus (for address/data pins)
    bool invert_logic;       // True if pin is active-low
    const char* group_name;  // Bus/group name (e.g., "ADDR", "DATA", "PORTA")
    const char* alt_function;// Alternate function name
    bool is_differential_pos;// True if positive side of differential pair
    bool is_differential_neg;// True if negative side of differential pair
};

// Pin state for real-time visualization
struct PinState {
    bool is_active;          // Current pin state
    bool is_output;          // True if pin is output, false if input
    uint8_t value;           // For multi-bit values or analog levels (0-255)
    bool is_tristate;        // True if pin is in high-impedance state
    bool is_valid;           // True if pin state is valid/available
    float analog_voltage;    // For analog pins (0.0 - Vcc)
    bool is_pwm;             // True if pin is PWM output
    float pwm_duty_cycle;    // PWM duty cycle (0.0 - 1.0)
};

// Chip markings/labels
struct ChipMarkings {
    const char* part_number;     // Main part number (e.g., "74LS00", "ATmega328P")
    const char* manufacturer;    // Manufacturer name or logo
    const char* package_variant; // Package variant (e.g., "-AU", "-PN")
    const char* date_code;       // Date code (e.g., "2023-45")
    const char* lot_number;      // Lot/batch number
    const char* custom_text;     // Custom text line
    bool show_part_number;
    bool show_manufacturer;
    bool show_package_variant;
    bool show_date_code;
};

// Package layout configuration
struct PackageLayout {
    float width;             // Package width in millimeters
    float height;            // Package height in millimeters
    PackageType package_type;// Package type
    OrientationMarker marker;// Orientation marker type
    float pin_pitch;         // Pin-to-pin spacing in millimeters (for accurate scaling)
    bool has_thermal_pad;    // True if package has exposed thermal pad
    bool has_center_slug;    // True if package has center metal slug
    float thermal_pad_size;  // Size of thermal pad (0.0 - 1.0, relative to package)
};

// Pin layout arrays for each side
struct PinLayout { // TODO : Rename to ChipLayout
    std::vector<ChipPin> left_pins;     // Pins on left side (top to bottom)
    std::vector<ChipPin> right_pins;    // Pins on right side (top to bottom)
    std::vector<ChipPin> top_pins;      // Pins on top side (left to right)
    std::vector<ChipPin> bottom_pins;   // Pins on bottom side (left to right)
    std::vector<ChipPin> grid_pins;     // For BGA/LGA packages (grid layout)
    PackageLayout package;              // Package dimensions and info
    ChipMarkings markings;              // Chip markings/labels
    
    // Calculate total number of pins from all sides and grid
    size_t get_total_pins() const {
        return left_pins.size() + right_pins.size() + top_pins.size() + bottom_pins.size() + grid_pins.size();
    }
    
    // Generate package name from type and pin count
    std::string get_package_name() const;
};

// BGA grid position (for BGA/LGA packages)
struct BGAPosition {
    uint8_t row;    // Row (A, B, C, ...)
    uint8_t col;    // Column (1, 2, 3, ...)
};

// ============================================================================
// HELPER FUNCTIONS FOR COMMON PACKAGE TYPES
// ============================================================================

// Create standard DIP package layouts
PinLayout create_dip40_layout();
PinLayout create_dip28_layout(); 
PinLayout create_dip24_layout();
PinLayout create_dip20_layout();
PinLayout create_dip16_layout();
PinLayout create_dip14_layout();
PinLayout create_dip8_layout();

// Create SOIC/SOP package layouts
PinLayout create_soic8_layout();
PinLayout create_soic14_layout();
PinLayout create_soic16_layout();
PinLayout create_soic28_layout();

// Create PLCC package layouts (pins on all 4 sides)
PinLayout create_plcc28_layout();
PinLayout create_plcc44_layout();
PinLayout create_plcc68_layout();

// Create QFP package layouts (pins on all 4 sides)
PinLayout create_qfp32_layout();
PinLayout create_qfp44_layout();
PinLayout create_qfp64_layout();
PinLayout create_qfp100_layout();
PinLayout create_qfp144_layout();

// Create QFN package layouts
PinLayout create_qfn16_layout();
PinLayout create_qfn24_layout();
PinLayout create_qfn32_layout();
PinLayout create_qfn48_layout();

// Create BGA package layouts
PinLayout create_bga64_layout();
PinLayout create_bga100_layout();
PinLayout create_bga256_layout();

// Create power package layouts
PinLayout create_to220_layout();   // 3-pin power regulator
PinLayout create_to92_layout();    // 3-pin small transistor
PinLayout create_sot23_layout();   // 3/5/6-pin small transistor
PinLayout create_sot223_layout();  // Power package

// Create SIP layouts
PinLayout create_sip8_layout();
PinLayout create_sip9_layout();

// Helper functions for building custom layouts
PinLayout create_custom_dip(uint8_t total_pins, const char* part_name = nullptr);
PinLayout create_custom_qfp(uint8_t total_pins, const char* part_name = nullptr);
PinLayout create_custom_bga(uint8_t rows, uint8_t cols, const char* part_name = nullptr);

// Pin definition helpers
ChipPin make_power_pin(uint8_t num, const char* label);
ChipPin make_ground_pin(uint8_t num, const char* label = "GND");
ChipPin make_address_pin(uint8_t num, const char* label, uint8_t bit);
ChipPin make_data_pin(uint8_t num, const char* label, uint8_t bit);
ChipPin make_control_pin(uint8_t num, const char* label, bool active_low = false);
ChipPin make_clock_pin(uint8_t num, const char* label);
ChipPin make_interrupt_pin(uint8_t num, const char* label, bool active_low = true);
ChipPin make_gpio_pin(uint8_t num, const char* label, const char* port = nullptr);
ChipPin make_analog_pin(uint8_t num, const char* label);
ChipPin make_differential_pin(uint8_t num, const char* label, bool positive);
ChipPin make_nc_pin(uint8_t num);

#endif // CHIP_LAYOUT_H