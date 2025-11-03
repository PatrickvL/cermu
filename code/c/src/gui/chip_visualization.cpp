/*
 * chip_visualization.cpp - Enhanced chip visualization system implementation
 */

#include "chip_visualization.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

// ============================================================================
// CONFIGURATION IMPLEMENTATIONS
// ============================================================================

ChipVisualConfig::ChipVisualConfig() {
    apply_style(VisualStyle::CLASSIC_DARK);
}

void ChipVisualConfig::apply_style(VisualStyle new_style) {
    style = new_style;
    
    switch (style) {
        case VisualStyle::CLASSIC_DARK:
            chip_body_color = 0xFF2D2D30;
            chip_border_color = 0xFF808080;
            pin_border_color = 0xFF000000;
            text_color = 0xFFFFFFFF;
            active_text_color = 0xFF00FF00;
            pin_number_color = 0xFFA0A0A0;
            led_active_color = 0xFF00FF00;
            led_inactive_color = 0xFF404040;
            notch_color = 0xFF404040;
            marker_color = 0xFF606060;
            thermal_pad_color = 0xFF404060;
            group_border_color = 0xFF606060;
            break;
            
        case VisualStyle::CLASSIC_LIGHT:
            chip_body_color = 0xFFF0F0F0;
            chip_border_color = 0xFF404040;
            pin_border_color = 0xFF000000;
            text_color = 0xFF000000;
            active_text_color = 0xFF008000;
            pin_number_color = 0xFF606060;
            led_active_color = 0xFF00C000;
            led_inactive_color = 0xFFC0C0C0;
            notch_color = 0xFFC0C0C0;
            marker_color = 0xFF808080;
            thermal_pad_color = 0xFFD0D0E0;
            group_border_color = 0xFF808080;
            break;
            
        case VisualStyle::HIGH_CONTRAST:
            chip_body_color = 0xFF000000;
            chip_border_color = 0xFFFFFFFF;
            pin_border_color = 0xFFFFFFFF;
            text_color = 0xFFFFFFFF;
            active_text_color = 0xFF00FF00;
            pin_number_color = 0xFFFFFFFF;
            led_active_color = 0xFF00FF00;
            led_inactive_color = 0xFF808080;
            notch_color = 0xFFFFFFFF;
            marker_color = 0xFFFFFFFF;
            thermal_pad_color = 0xFF404040;
            group_border_color = 0xFFFFFFFF;
            break;
            
        default:
            chip_body_color = 0xFF1A1A1A;
            chip_border_color = 0xFFCCCCCC;
            pin_border_color = 0xFFCCCCCC;
            text_color = 0xFFFFFFFF;
            active_text_color = 0xFF00FF00;
            pin_number_color = 0xFFE0E0E0;
            led_active_color = 0xFF00FF00;
            led_inactive_color = 0xFF004000;
            notch_color = 0xFF606060;
            marker_color = 0xFF808080;
            thermal_pad_color = 0xFF404060;
            group_border_color = 0xFF606060;
            break;
    }
    
    // Common dimension settings
    pin_width = 10.0f;
    pin_height = 16.0f;
    led_radius = 3.0f;
    label_offset = 18.0f;
    pin_spacing_factor = 1.2f;
    chip_border_width = 2.0f;
    pin_border_width = 1.5f;
    marker_size = 8.0f;
    font_size = 12.0f;
    
    // Display options
    show_pin_numbers = true;
    show_pin_labels = true;
    show_led_indicators = true;
    show_package_name = true;
    show_chip_markings = true;
    show_pin_groups = false;
    show_thermal_pad = false;
    show_alternate_functions = false;
    show_voltage_levels = false;
    show_pwm_indicators = false;
    use_compact_layout = false;
    
    notation_style = PinNotationStyle::SLASH_PREFIX;
}

// ============================================================================
// CHIP VISUALIZATION CLASS
// ============================================================================

ChipVisualization::ChipVisualization(const PinLayout& pin_layout) 
    : layout(pin_layout), layout_dirty(true) {
    config = ChipVisualConfig();
    pin_states.resize(layout.get_total_pins());
}

void ChipVisualization::set_config(const ChipVisualConfig& new_config) {
    config = new_config;
    layout_dirty = true;
}

void ChipVisualization::apply_style_preset(VisualStyle style) {
    config.apply_style(style);
    layout_dirty = true;
}

void ChipVisualization::set_pin_state(int pin_number, const PinState& state) {
    if (pin_number > 0 && pin_number <= static_cast<int>(pin_states.size())) {
        pin_states[pin_number - 1] = state;
    }
}

void ChipVisualization::set_pin_states(const std::vector<PinState>& states) {
    pin_states = states;
    pin_states.resize(layout.get_total_pins());
}

const PinState& ChipVisualization::get_pin_state(int pin_number) const {
    static const PinState default_state = {false, false, 0, false, true};
    if (pin_number > 0 && pin_number <= static_cast<int>(pin_states.size())) {
        return pin_states[pin_number - 1];
    }
    return default_state;
}

void ChipVisualization::clear_pin_states() {
    for (auto& state : pin_states) {
        state = {false, false, 0, false, true};
    }
}

void ChipVisualization::add_pin_group(const PinGroup& group) {
    pin_groups.push_back(group);
}

void ChipVisualization::clear_pin_groups() {
    pin_groups.clear();
}

void ChipVisualization::draw(ImVec2 size) {
    ImDrawList* draw_list = igGetWindowDrawList();
    ImVec2 canvas_pos;
    igGetCursorScreenPos(&canvas_pos);
    
    if (size.x <= 0 || size.y <= 0) {
        size = get_minimum_size();
    }
    
    calculate_layout(size);
    
    // Draw the chip based on package type
    switch (layout.package.type) {
        case PackageType::DIP:
            draw_dip_package(draw_list);
            break;
        case PackageType::SOIC:
            draw_soic_package(draw_list);
            break;
        case PackageType::PLCC:
            draw_plcc_package(draw_list);
            break;
        case PackageType::QFP:
            draw_qfp_package(draw_list);
            break;
        case PackageType::QFN:
            draw_qfn_package(draw_list);
            break;
        case PackageType::BGA:
            draw_bga_package(draw_list);
            break;
        case PackageType::TO:
            draw_to_package(draw_list);
            break;
        case PackageType::SOT:
            draw_sot_package(draw_list);
            break;
        default:
            draw_custom_package(draw_list);
            break;
    }
    
    igDummy(size);
}

ImVec2 ChipVisualization::get_minimum_size() const {
    // Calculate minimum size based on package layout
    float width = layout.package.width_mils * 0.1f + 100.0f;  // Convert mil to pixels with padding
    float height = layout.package.height_mils * 0.1f + 100.0f;
    return {width, height};
}

void ChipVisualization::calculate_layout(ImVec2 available_size) {
    if (!layout_dirty) return;
    
    // Calculate chip dimensions
    float scale = std::min(available_size.x / (layout.package.width_mils * 0.1f + 100.0f),
                          available_size.y / (layout.package.height_mils * 0.1f + 100.0f));
    
    chip_size.x = layout.package.width_mils * 0.1f * scale;
    chip_size.y = layout.package.height_mils * 0.1f * scale;
    
    chip_position.x = available_size.x * 0.5f;
    chip_position.y = available_size.y * 0.5f;
    
    // Calculate pin positions
    pin_positions.clear();
    pin_rects.clear();
    pin_positions.resize(layout.get_total_pins());
    pin_rects.resize(layout.get_total_pins());
    
    // Calculate positions for each pin based on package layout
    int pin_index = 0;
    for (const auto& pin : layout.pins) {
        ImVec2 pos = {
            chip_position.x + pin.x_mils * 0.1f * scale,
            chip_position.y + pin.y_mils * 0.1f * scale
        };
        pin_positions[pin_index] = pos;
        
        ImRect rect = {
            pos.x - config.pin_width * 0.5f,
            pos.y - config.pin_height * 0.5f,
            pos.x + config.pin_width * 0.5f,
            pos.y + config.pin_height * 0.5f
        };
        pin_rects[pin_index] = rect;
        pin_index++;
    }
    
    layout_dirty = false;
}

void ChipVisualization::draw_dip_package(ImDrawList* draw_list) {
    // Draw chip body
    ImVec2 chip_min = {chip_position.x - chip_size.x * 0.5f, chip_position.y - chip_size.y * 0.5f};
    ImVec2 chip_max = {chip_position.x + chip_size.x * 0.5f, chip_position.y + chip_size.y * 0.5f};
    
    ImDrawList_AddRectFilled(draw_list, chip_min, chip_max, config.chip_body_color, 0.0f, 0);
    ImDrawList_AddRect(draw_list, chip_min, chip_max, config.chip_border_color, 0.0f, 0, config.chip_border_width);
    
    // Draw pin 1 notch if package has one
    if (layout.package.orientation_marker == OrientationMarker::NOTCH) {
        float notch_size = config.marker_size;
        ImVec2 notch_center = {chip_position.x, chip_min.y};
        ImDrawList_AddCircleFilled(draw_list, notch_center, notch_size, config.notch_color, 12);
    }
    
    draw_pins(draw_list);
    
    if (config.show_pin_labels) {
        draw_pin_labels(draw_list);
    }
    
    if (config.show_pin_numbers) {
        draw_pin_numbers(draw_list);
    }
    
    if (config.show_led_indicators) {
        draw_led_indicators(draw_list);
    }
}

void ChipVisualization::draw_pins(ImDrawList* draw_list) {
    for (size_t i = 0; i < layout.pins.size(); ++i) {
        const ChipPin& pin = layout.pins[i];
        const PinState& state = (i < pin_states.size()) ? pin_states[i] : PinState{false, false, 0, false, true};
        
        uint32_t pin_color = get_pin_color(pin.number);
        if (state.is_active && config.show_led_indicators) {
            pin_color = config.led_active_color;
        }
        
        ImDrawList_AddRectFilled(draw_list, 
            {pin_rects[i].Min.x, pin_rects[i].Min.y},
            {pin_rects[i].Max.x, pin_rects[i].Max.y},
            pin_color, 0.0f, 0);
            
        ImDrawList_AddRect(draw_list,
            {pin_rects[i].Min.x, pin_rects[i].Min.y},
            {pin_rects[i].Max.x, pin_rects[i].Max.y},
            config.pin_border_color, 0.0f, 0, config.pin_border_width);
    }
}

void ChipVisualization::draw_pin_labels(ImDrawList* draw_list) {
    for (size_t i = 0; i < layout.pins.size(); ++i) {
        const ChipPin& pin = layout.pins[i];
        if (pin.label.empty()) continue;
        
        std::string label = format_pin_label(pin.label, pin.is_inverted);
        
        ImVec2 text_size;
        igCalcTextSize(&text_size, label.c_str(), NULL, false, -1.0f);
        
        ImVec2 label_pos = {
            pin_positions[i].x - text_size.x * 0.5f,
            pin_positions[i].y + config.label_offset
        };
        
        ImDrawList_AddText_Vec2(draw_list, label_pos, config.text_color, label.c_str(), NULL);
    }
}

void ChipVisualization::draw_pin_numbers(ImDrawList* draw_list) {
    for (size_t i = 0; i < layout.pins.size(); ++i) {
        const ChipPin& pin = layout.pins[i];
        
        char pin_num[8];
        snprintf(pin_num, sizeof(pin_num), "%d", pin.number);
        
        ImVec2 text_size;
        igCalcTextSize(&text_size, pin_num, NULL, false, -1.0f);
        
        ImVec2 num_pos = {
            pin_positions[i].x - text_size.x * 0.5f,
            pin_positions[i].y - config.label_offset - text_size.y
        };
        
        ImDrawList_AddText_Vec2(draw_list, num_pos, config.pin_number_color, pin_num, NULL);
    }
}

void ChipVisualization::draw_led_indicators(ImDrawList* draw_list) {
    for (size_t i = 0; i < layout.pins.size(); ++i) {
        const PinState& state = (i < pin_states.size()) ? pin_states[i] : PinState{false, false, 0, false, true};
        
        uint32_t led_color = state.is_active ? config.led_active_color : config.led_inactive_color;
        
        ImVec2 led_pos = {
            pin_positions[i].x + config.pin_width * 0.5f + config.led_radius + 2.0f,
            pin_positions[i].y
        };
        
        ImDrawList_AddCircleFilled(draw_list, led_pos, config.led_radius, led_color, 8);
    }
}

std::string ChipVisualization::format_pin_label(const std::string& base_label, bool is_active_low) const {
    if (!is_active_low) {
        return base_label;
    }
    
    switch (config.notation_style) {
        case PinNotationStyle::SLASH_PREFIX:
            return "/" + base_label;
        case PinNotationStyle::TILDE_PREFIX:
            return "~" + base_label;
        case PinNotationStyle::HASH_SUFFIX:
            return base_label + "#";
        case PinNotationStyle::ASTERISK_SUFFIX:
            return base_label + "*";
        case PinNotationStyle::N_SUFFIX:
            return base_label + "_N";
        case PinNotationStyle::BAR_SUFFIX:
            return base_label + "_BAR";
        default:
            return base_label;
    }
}

uint32_t ChipVisualization::get_pin_color(int pin_number) const {
    // Find the pin and return color based on its type
    for (const auto& pin : layout.pins) {
        if (pin.number == pin_number) {
            switch (pin.type) {
                case PinType::POWER:     return 0xFF6464FF; // Red
                case PinType::CLOCK:     return 0xFF64FFFF; // Yellow
                case PinType::ADDRESS:   return 0xFFFF9664; // Light Blue
                case PinType::DATA:      return 0xFF96FF64; // Light Green
                case PinType::CONTROL:   return 0xFF6496FF; // Orange
                case PinType::INTERRUPT: return 0xFFFF64FF; // Magenta
                case PinType::SPECIAL:   return 0xFFC8C8C8; // Light Gray
                case PinType::IO_PORT:   return 0xFF64C8FF; // Pink
                default:                 return 0xFF808080; // Dark Gray
            }
        }
    }
    return 0xFF808080; // Default gray
}

// Stub implementations for other package types
void ChipVisualization::draw_soic_package(ImDrawList* draw_list) { draw_dip_package(draw_list); }
void ChipVisualization::draw_plcc_package(ImDrawList* draw_list) { draw_dip_package(draw_list); }
void ChipVisualization::draw_qfp_package(ImDrawList* draw_list) { draw_dip_package(draw_list); }
void ChipVisualization::draw_qfn_package(ImDrawList* draw_list) { draw_dip_package(draw_list); }
void ChipVisualization::draw_bga_package(ImDrawList* draw_list) { draw_dip_package(draw_list); }
void ChipVisualization::draw_to_package(ImDrawList* draw_list) { draw_dip_package(draw_list); }
void ChipVisualization::draw_sot_package(ImDrawList* draw_list) { draw_dip_package(draw_list); }
void ChipVisualization::draw_custom_package(ImDrawList* draw_list) { draw_dip_package(draw_list); }

void ChipVisualization::draw_chip_body(ImDrawList* draw_list) { /* Implemented in package-specific functions */ }
void ChipVisualization::draw_orientation_markers(ImDrawList* draw_list) { /* TODO */ }
void ChipVisualization::draw_chip_markings(ImDrawList* draw_list) { /* TODO */ }
void ChipVisualization::draw_thermal_pad(ImDrawList* draw_list) { /* TODO */ }
void ChipVisualization::draw_pin_groups(ImDrawList* draw_list) { /* TODO */ }

int ChipVisualization::get_hovered_pin() const {
    // TODO: Implement hover detection
    return -1;
}

bool ChipVisualization::is_pin_clicked(int pin_number) const {
    // TODO: Implement click detection
    return false;
}

void ChipVisualization::set_layout(const PinLayout& new_layout) {
    layout = new_layout;
    layout_dirty = true;
    pin_states.resize(layout.get_total_pins());
}

ImVec2 ChipVisualization::get_pin_position(int pin_number) const {
    for (size_t i = 0; i < layout.pins.size(); ++i) {
        if (layout.pins[i].number == pin_number) {
            return (i < pin_positions.size()) ? pin_positions[i] : ImVec2{0, 0};
        }
    }
    return {0, 0};
}

ImRect ChipVisualization::get_pin_rect(int pin_number) const {
    for (size_t i = 0; i < layout.pins.size(); ++i) {
        if (layout.pins[i].number == pin_number) {
            return (i < pin_rects.size()) ? pin_rects[i] : ImRect{0, 0, 0, 0};
        }
    }
    return {0, 0, 0, 0};
}

bool ChipVisualization::is_pin_hovered(int pin_number, ImVec2 mouse_pos) const {
    ImRect rect = get_pin_rect(pin_number);
    return mouse_pos.x >= rect.Min.x && mouse_pos.x <= rect.Max.x &&
           mouse_pos.y >= rect.Min.y && mouse_pos.y <= rect.Max.y;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

ChipVisualConfig create_dark_theme_config() {
    ChipVisualConfig config;
    config.apply_style(VisualStyle::CLASSIC_DARK);
    return config;
}

ChipVisualConfig create_light_theme_config() {
    ChipVisualConfig config;
    config.apply_style(VisualStyle::CLASSIC_LIGHT);
    return config;
}

ChipVisualConfig create_high_contrast_config() {
    ChipVisualConfig config;
    config.apply_style(VisualStyle::HIGH_CONTRAST);
    return config;
}

ChipVisualConfig create_colorful_config() {
    ChipVisualConfig config;
    config.apply_style(VisualStyle::COLORFUL);
    return config;
}

ChipVisualConfig create_monochrome_config() {
    ChipVisualConfig config;
    config.apply_style(VisualStyle::MONOCHROME);
    return config;
}

ChipVisualConfig create_datasheet_config() {
    ChipVisualConfig config;
    config.apply_style(VisualStyle::DATASHEET);
    return config;
}

ChipVisualConfig create_schematic_config() {
    ChipVisualConfig config;
    config.apply_style(VisualStyle::SCHEMATIC);
    return config;
}

PinGroup create_bus_group(const std::vector<int>& pins, const std::string& label, uint32_t color) {
    return {pins, label, color, true, false};
}

PinGroup create_differential_pair(int pin1, int pin2, const std::string& label, uint32_t color) {
    return {{pin1, pin2}, label, color, false, true};
}

PinGroup create_power_group(const std::vector<int>& pins, const std::string& label, uint32_t color) {
    return {pins, label, color, false, false};
}

uint32_t rgba_to_abgr(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (static_cast<uint32_t>(a) << 24) |
           (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(r);
}

void abgr_to_rgba(uint32_t abgr, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
    r = static_cast<uint8_t>(abgr & 0xFF);
    g = static_cast<uint8_t>((abgr >> 8) & 0xFF);
    b = static_cast<uint8_t>((abgr >> 16) & 0xFF);
    a = static_cast<uint8_t>((abgr >> 24) & 0xFF);
}