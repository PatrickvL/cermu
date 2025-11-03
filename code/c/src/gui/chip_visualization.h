/*
 * chip_visualization.h - Generic chip visualization system for IC packages
 * 
 * This header provides a flexible system for visualizing various IC packages
 * with pin-accurate layouts, LED indicators, and trait-based pin assignments.
 * Supports DIP packages with pins on 2 or 4 sides.
 */

#ifndef CHIP_VISUALIZATION_H
#define CHIP_VISUALIZATION_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>

#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>

// ============================================================================
// PIN TYPES AND STRUCTURES
// ============================================================================

// Pin types for visual categorization and color coding
enum class PinType {
    POWER,        // VCC, VSS, VDD, GND
    CLOCK,        // Clock inputs/outputs (φ0, φ1, φ2, CLK, etc.)
    ADDRESS,      // Address bus lines (A0-A23, etc.)
    DATA,         // Data bus lines (D0-D15, etc.)
    CONTROL,      // Control signals (RW, SYNC, RDY, AEC, etc.)
    INTERRUPT,    // Interrupt lines (IRQ, NMI, RES, INT, etc.)
    SPECIAL,      // Special purpose (SO, BE, ML, NC, etc.)
    IO_PORT       // I/O port lines (P0-P7, etc.)
};

// Pin side enumeration for package layout
enum class PinSide {
    LEFT,         // Left side pins (top to bottom)
    RIGHT,        // Right side pins (top to bottom) 
    TOP,          // Top side pins (left to right)
    BOTTOM        // Bottom side pins (left to right)
};

// Pin definition structure
struct ChipPin {
    uint8_t pin_number;      // Physical pin number
    const char* label;       // Pin label (e.g., "A0", "D7", "RW")
    PinType type;            // Pin type for color coding
    uint8_t bit_index;       // Bit index within bus (for address/data pins)
    bool invert_logic;       // True if pin is active-low
};

// Pin state for real-time visualization
struct PinState {
    bool is_active;          // Current pin state
    bool is_output;          // True if pin is output, false if input
    uint8_t value;           // For multi-bit values or analog levels
    bool is_tristate;        // True if pin is in high-impedance state
    bool is_valid;           // True if pin state is valid/available
};

// Package layout configuration
struct PackageLayout {
    float width;             // Package width in pixels
    float height;            // Package height in pixels
    bool has_notch;          // True if package has pin 1 notch indicator
    const char* vendor;      // Manufacturer (e.g., "MOS Technology", "Ricoh", "WDC")
    const char* chip_id;     // Chip identifier (e.g., "6502", "6510", "2A03")
};

// Pin layout arrays for each side
struct PinLayout {
    std::vector<ChipPin> left_pins;     // Pins on left side (top to bottom)
    std::vector<ChipPin> right_pins;    // Pins on right side (top to bottom)
    std::vector<ChipPin> top_pins;      // Pins on top side (left to right)
    std::vector<ChipPin> bottom_pins;   // Pins on bottom side (left to right)
    PackageLayout package;              // Package dimensions and info
    
    // Calculate total number of pins from all sides
    size_t get_total_pins() const {
        return left_pins.size() + right_pins.size() + top_pins.size() + bottom_pins.size();
    }
};

// ============================================================================
// COLOR SCHEME AND VISUAL SETTINGS
// ============================================================================

// Visual configuration for chip rendering
struct ChipVisualConfig {
    uint32_t chip_body_color;           // Chip body fill color (ABGR format)
    uint32_t chip_border_color;         // Chip outline color
    uint32_t pin_border_color;          // Pin outline color
    uint32_t text_color;                // Default text color
    uint32_t active_text_color;         // Text color when pin is active
    uint32_t pin_number_color;          // Pin number text color
    uint32_t led_active_color;          // LED color when pin is active
    uint32_t led_inactive_color;        // LED color when pin is inactive
    uint32_t notch_color;               // Pin 1 notch indicator color
    
    float pin_width;                    // Pin rectangle width
    float pin_height;                   // Pin rectangle height
    float led_size;                     // LED indicator radius
    float label_offset;                 // Distance from pin to label
    float pin_spacing_factor;           // Spacing multiplier between pins
    float chip_border_width;            // Chip outline thickness
    float pin_border_width;             // Pin outline thickness
    
    bool show_pin_numbers;              // Display physical pin numbers
    bool show_pin_labels;               // Display pin labels
    bool show_led_indicators;           // Display LED status indicators
    bool show_package_name;             // Display package type name
    
    static ChipVisualConfig get_default();
};

// Get color for pin type
uint32_t get_pin_type_color(PinType type);

// ============================================================================
// CHIP VISUALIZATION CLASS
// ============================================================================

class ChipVisualization {
public:
    ChipVisualization(const PinLayout& layout, const ChipVisualConfig& config = ChipVisualConfig::get_default());
    
    // Main rendering function
    void render(ImVec2 chip_center, const std::vector<PinState>& pin_states, const char* chip_name = nullptr);
    
    // Render individual components
    void render_chip_body(ImVec2 chip_center, const char* chip_name = nullptr);
    void render_pins(ImVec2 chip_center, const std::vector<PinState>& pin_states);
    void render_legend();
    
    // Configuration
    void set_visual_config(const ChipVisualConfig& config) { config_ = config; }
    const ChipVisualConfig& get_visual_config() const { return config_; }
    const PinLayout& get_pin_layout() const { return layout_; }
    
    // Pin lookup by number or label
    const ChipPin* find_pin_by_number(uint8_t pin_number) const;
    const ChipPin* find_pin_by_label(const char* label) const;
    
    // Get pin position for external drawing
    ImVec2 get_pin_position(ImVec2 chip_center, const ChipPin& pin) const;
    
private:
    PinLayout layout_;
    ChipVisualConfig config_;
    
    void render_pin_side(ImVec2 chip_center, const std::vector<ChipPin>& pins, 
                        const std::vector<PinState>& pin_states, PinSide side);
    void render_single_pin(ImVec2 pin_pos, const ChipPin& pin, const PinState& state, PinSide side);
    ImVec2 calculate_pin_position(ImVec2 chip_center, const ChipPin& pin, size_t index_in_side, PinSide side) const;
    ImVec2 get_led_position(ImVec2 pin_pos, PinSide side) const;
    ImVec2 get_label_position(ImVec2 pin_pos, const ChipPin& pin, PinSide side) const;
};

// ============================================================================
// HELPER FUNCTIONS FOR COMMON PACKAGE TYPES
// ============================================================================

// Create standard DIP package layouts
PinLayout create_dip40_layout();
PinLayout create_dip28_layout(); 
PinLayout create_dip24_layout();
PinLayout create_dip16_layout();

// Create PLCC package layouts (pins on all 4 sides)
PinLayout create_plcc44_layout();
PinLayout create_plcc68_layout();

// Create QFP package layouts (pins on all 4 sides)
PinLayout create_qfp64_layout();
PinLayout create_qfp100_layout();

#endif // CHIP_VISUALIZATION_H