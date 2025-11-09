/*
 * chip_visualization.h - Generic chip visualization system for IC packages
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

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <string>

// Include ImGui only when available (conditional compilation)
#ifdef IMGUI_VERSION
#include <imgui.h>
#endif
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
    
    static ChipVisualConfig get_default();
    static ChipVisualConfig get_style(VisualStyle style);
};

// Get color for pin type
uint32_t get_pin_type_color(PinType type, VisualStyle style = VisualStyle::CLASSIC_DARK);

// ============================================================================
// CHIP VISUALIZATION CLASS
// ============================================================================

class ChipVisualization {
public:
    ChipVisualization(const ChipLayout& layout, const ChipVisualConfig& config = ChipVisualConfig::get_default());
    
    // Main rendering function
#ifdef IMGUI_VERSION
    void render(ImVec2 chip_center, const std::vector<PinSignalState>& pin_states, const char* chip_name = nullptr);
    
    // Render individual components
    void render_chip_body(ImVec2 chip_center, const char* chip_name = nullptr);
    void render_pins(ImVec2 chip_center, const std::vector<PinSignalState>& pin_states);
    void render_orientation_marker(ImVec2 chip_center);
    void render_thermal_pad(ImVec2 chip_center);
    void render_chip_markings(ImVec2 chip_center);
    void render_pin_groups(ImVec2 chip_center);
    void render_legend();
    void render_bga_grid(ImVec2 chip_center, const std::vector<PinSignalState>& pin_states);
#else
    void render(void* chip_center, const std::vector<PinSignalState>& pin_states, const char* chip_name = nullptr) {}
    void render_chip_body(void* chip_center, const char* chip_name = nullptr) {}
    void render_pins(void* chip_center, const std::vector<PinSignalState>& pin_states) {}
    void render_orientation_marker(void* chip_center) {}
    void render_thermal_pad(void* chip_center) {}
    void render_chip_markings(void* chip_center) {}
    void render_pin_groups(void* chip_center) {}
    void render_legend() {}
    void render_bga_grid(void* chip_center, const std::vector<PinSignalState>& pin_states) {}
#endif
    
    // Settings GUI
    void render_settings_gui();
    
    // Configuration
    void set_visual_config(const ChipVisualConfig& config) { config_ = config; }
    const ChipVisualConfig& get_visual_config() const { return config_; }
    ChipVisualConfig& get_mutable_visual_config() { return config_; }
    const ChipLayout& get_pin_layout() const { return layout_; }
    void set_pin_layout(const ChipLayout& layout) { layout_ = layout; }
    
    // Pin lookup by number or label
    const ChipPin* find_pin_by_number(uint8_t pin_number) const;
    const ChipPin* find_pin_by_label(const char* label) const;
    std::vector<const ChipPin*> find_pins_by_group(const char* group_name) const;
    
    // Get pin position for external drawing
#ifdef IMGUI_VERSION
    ImVec2 get_pin_position(ImVec2 chip_center, const ChipPin& pin) const;
#else
    void* get_pin_position(void* chip_center, const ChipPin& pin) const { return nullptr; }
#endif
    
    // Format pin label according to notation style
    std::string format_pin_label(const ChipPin& pin) const;
    
    // Get recommended window size for chip
#ifdef IMGUI_VERSION
    ImVec2 get_recommended_size() const;
#else
    void* get_recommended_size() const { return nullptr; }
#endif
    
private:
    ChipLayout layout_;
    ChipVisualConfig config_;
    
    // Scaled chip dimensions for rendering (calculated from package dimensions)
    mutable float scaled_chip_width_ = 0.0f;
    mutable float scaled_chip_height_ = 0.0f;
    
#ifdef IMGUI_VERSION
    void render_pin_side(ImVec2 chip_center, const std::vector<ChipPin>& pins,
                        const std::vector<PinSignalState>& pin_states, PinSide side);
    void render_single_pin(ImVec2 pin_pos, const ChipPin& pin, const PinSignalState& state, PinSide side);
    void render_dip_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name);
    void render_surface_mount_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name);
    void render_qfp_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name);
    void render_bga_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name);
    void render_to_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name);
    
    ImVec2 calculate_pin_position(ImVec2 chip_center, const ChipPin& pin, size_t index_in_side, PinSide side) const;
    ImVec2 calculate_bga_position(ImVec2 chip_center, uint8_t row, uint8_t col) const;
    ImVec2 get_led_position(ImVec2 pin_pos, PinSide side) const;
    ImVec2 get_label_position(ImVec2 pin_pos, const ChipPin& pin, PinSide side) const;
    
    void draw_notch(ImVec2 chip_center);
    void draw_dot_marker(ImVec2 chip_center);
    void draw_chamfer(ImVec2 chip_center);
    void draw_bar_marker(ImVec2 chip_center);
    void draw_triangle_marker(ImVec2 chip_center);
#else
    void render_pin_side(void* chip_center, const std::vector<ChipPin>& pins,
                        const std::vector<PinSignalState>& pin_states, PinSide side) {}
    void render_single_pin(void* pin_pos, const ChipPin& pin, const PinSignalState& state, PinSide side) {}
    void render_dip_style(void* chip_center, float chip_width, float chip_height, const char* chip_name) {}
    void render_surface_mount_style(void* chip_center, float chip_width, float chip_height, const char* chip_name) {}
    void render_qfp_style(void* chip_center, float chip_width, float chip_height, const char* chip_name) {}
    void render_bga_style(void* chip_center, float chip_width, float chip_height, const char* chip_name) {}
    void render_to_style(void* chip_center, float chip_width, float chip_height, const char* chip_name) {}
    
    void* calculate_pin_position(void* chip_center, const ChipPin& pin, size_t index_in_side, PinSide side) const { return nullptr; }
    void* calculate_bga_position(void* chip_center, uint8_t row, uint8_t col) const { return nullptr; }
    void* get_led_position(void* pin_pos, PinSide side) const { return nullptr; }
    void* get_label_position(void* pin_pos, const ChipPin& pin, PinSide side) const { return nullptr; }
    
    void draw_notch(void* chip_center) {}
    void draw_dot_marker(void* chip_center) {}
    void draw_chamfer(void* chip_center) {}
    void draw_bar_marker(void* chip_center) {}
    void draw_triangle_marker(void* chip_center) {}
#endif
};

