/*
 * chip_visualization.h - Enhanced chip visualization system for IC packages
 * 
 * Enhanced version with support for:
 * - Multiple orientation markers (notch, dot, chamfer, bar, triangle)
 * - Various package types (DIP, SOIC, PLCC, QFP, QFN, BGA, TO, SOT)
 * - Chip markings (part number, manufacturer, date code, lot)
 * - Flexible pin notation styles
 * - Bus grouping and differential pairs
 * - Thermal pads and exposed pads
 * - Multiple visual styles
 */

#ifndef CHIP_VISUALIZATION_H
#define CHIP_VISUALIZATION_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <string>

#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include "../core/chip_layout.h"

// ============================================================================
// COLOR SCHEME AND VISUAL SETTINGS
// ============================================================================

// Pin notation style
enum class PinNotationStyle {
    SLASH_PREFIX,     // /CS, /IRQ, /WE
    OVERLINE,         // C̄S̄, ĪR̄Q̄, W̄Ē (requires Unicode)
    TILDE_PREFIX,     // ~CS, ~IRQ, ~WE
    HASH_SUFFIX,      // CS#, IRQ#, WE#
    ASTERISK_SUFFIX,  // CS*, IRQ*, WE*
    N_SUFFIX,         // CS_N, IRQ_N, WE_N
    BAR_SUFFIX        // CS_BAR, IRQ_BAR
};

// Visual style presets
enum class VisualStyle {
    CLASSIC_DARK,      // Dark theme with traditional colors
    CLASSIC_LIGHT,     // Light theme with traditional colors
    HIGH_CONTRAST,     // High contrast for accessibility
    COLORFUL,          // Vibrant colors
    MONOCHROME,        // Black and white
    DATASHEET,         // Datasheet-style rendering
    SCHEMATIC          // Schematic symbol style
};

// Visual configuration for chip rendering
struct ChipVisualConfig {
    // Colors (ABGR format for ImGui)
    uint32_t chip_body_color;           // Chip body fill color
    uint32_t chip_border_color;         // Chip outline color
    uint32_t pin_border_color;          // Pin outline color
    uint32_t text_color;                // Default text color
    uint32_t active_text_color;         // Text color when pin is active
    uint32_t pin_number_color;          // Pin number text color
    uint32_t led_active_color;          // LED color when pin is active
    uint32_t led_inactive_color;        // LED color when pin is inactive
    uint32_t notch_color;               // Pin 1 notch indicator color
    uint32_t marker_color;              // Orientation marker color
    uint32_t thermal_pad_color;         // Thermal pad color
    uint32_t group_border_color;        // Color for pin grouping boxes
    
    // Dimensions
    float pin_width;                    // Pin rectangle width
    float pin_height;                   // Pin rectangle height
    float led_radius;                   // LED indicator radius
    float label_offset;                 // Distance from pin to label
    float pin_spacing_factor;           // Spacing multiplier between pins
    float chip_border_width;            // Chip outline thickness
    float pin_border_width;             // Pin outline thickness
    float marker_size;                  // Orientation marker size
    float font_size;                    // Base font size
    
    // Display options
    bool show_pin_numbers;              // Display physical pin numbers
    bool show_pin_labels;               // Display pin labels
    bool show_led_indicators;           // Display LED status indicators
    bool show_package_name;             // Display package type name
    bool show_chip_markings;            // Display chip markings (part #, etc.)
    bool show_pin_groups;               // Draw boxes around pin groups
    bool show_thermal_pad;              // Display thermal pad
    bool show_alternate_functions;      // Show alternate pin functions
    bool show_voltage_levels;           // Show voltage levels on analog pins
    bool show_pwm_indicators;           // Show PWM indicators
    bool use_compact_layout;            // Use more compact spacing
    
    // Pin notation
    PinNotationStyle notation_style;    // Style for active-low pins
    
    // Style preset
    VisualStyle style;                  // Visual style preset
    
    // Default constructor with reasonable defaults
    ChipVisualConfig();
    
    // Apply a style preset
    void apply_style(VisualStyle style);
};

// Pin grouping information
struct PinGroup {
    std::vector<int> pins;              // Pin numbers in this group
    std::string label;                  // Group label (e.g., "PORTB", "DATA")
    uint32_t color;                     // Group highlight color
    bool is_bus;                        // Is this a bus group?
    bool is_differential_pair;          // Is this a differential pair?
};

// ============================================================================
// CHIP VISUALIZATION CLASS
// ============================================================================

class ChipVisualization {
private:
    PinLayout layout;                   // Pin layout information
    ChipVisualConfig config;            // Visual configuration
    std::vector<PinGroup> pin_groups;   // Pin groupings
    
    // Current pin states
    std::vector<PinState> pin_states;   // State for each pin
    
    // Cached drawing data
    ImVec2 chip_size;                   // Calculated chip size
    ImVec2 chip_position;               // Chip position in draw area
    std::vector<ImVec2> pin_positions;  // Calculated pin positions
    std::vector<ImRect> pin_rects;      // Pin interaction rectangles
    bool layout_dirty;                  // Need to recalculate layout
    
    // Drawing helpers
    void calculate_layout(ImVec2 available_size);
    void draw_chip_body(ImDrawList* draw_list);
    void draw_pins(ImDrawList* draw_list);
    void draw_pin_labels(ImDrawList* draw_list);
    void draw_pin_numbers(ImDrawList* draw_list);
    void draw_led_indicators(ImDrawList* draw_list);
    void draw_orientation_markers(ImDrawList* draw_list);
    void draw_chip_markings(ImDrawList* draw_list);
    void draw_thermal_pad(ImDrawList* draw_list);
    void draw_pin_groups(ImDrawList* draw_list);
    
    // Package-specific drawing
    void draw_dip_package(ImDrawList* draw_list);
    void draw_soic_package(ImDrawList* draw_list);
    void draw_plcc_package(ImDrawList* draw_list);
    void draw_qfp_package(ImDrawList* draw_list);
    void draw_qfn_package(ImDrawList* draw_list);
    void draw_bga_package(ImDrawList* draw_list);
    void draw_to_package(ImDrawList* draw_list);
    void draw_sot_package(ImDrawList* draw_list);
    void draw_custom_package(ImDrawList* draw_list);
    
    // Utility functions
    ImVec2 get_pin_position(int pin_number) const;
    ImRect get_pin_rect(int pin_number) const;
    std::string format_pin_label(const std::string& base_label, bool is_active_low) const;
    uint32_t get_pin_color(int pin_number) const;
    bool is_pin_hovered(int pin_number, ImVec2 mouse_pos) const;
    
public:
    // Constructor
    explicit ChipVisualization(const PinLayout& layout);
    
    // Configuration
    void set_config(const ChipVisualConfig& config);
    const ChipVisualConfig& get_config() const { return config; }
    void apply_style_preset(VisualStyle style);
    
    // Pin state management
    void set_pin_state(int pin_number, const PinState& state);
    void set_pin_states(const std::vector<PinState>& states);
    const PinState& get_pin_state(int pin_number) const;
    void clear_pin_states();
    
    // Pin grouping
    void add_pin_group(const PinGroup& group);
    void clear_pin_groups();
    
    // Drawing
    void draw(ImVec2 size = ImVec2(0, 0));
    ImVec2 get_minimum_size() const;
    
    // Interaction
    int get_hovered_pin() const;
    bool is_pin_clicked(int pin_number) const;
    
    // Layout information
    const PinLayout& get_layout() const { return layout; }
    void set_layout(const PinLayout& new_layout);
    void invalidate_layout() { layout_dirty = true; }
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Create default configuration for different styles
ChipVisualConfig create_dark_theme_config();
ChipVisualConfig create_light_theme_config();
ChipVisualConfig create_high_contrast_config();
ChipVisualConfig create_colorful_config();
ChipVisualConfig create_monochrome_config();
ChipVisualConfig create_datasheet_config();
ChipVisualConfig create_schematic_config();

// Pin group helpers
PinGroup create_bus_group(const std::vector<int>& pins, const std::string& label, uint32_t color = 0xFF4080FF);
PinGroup create_differential_pair(int pin1, int pin2, const std::string& label, uint32_t color = 0xFF40FF80);
PinGroup create_power_group(const std::vector<int>& pins, const std::string& label, uint32_t color = 0xFFFF4040);

// Color utilities
uint32_t rgba_to_abgr(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
void abgr_to_rgba(uint32_t abgr, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a);

#endif // CHIP_VISUALIZATION_H