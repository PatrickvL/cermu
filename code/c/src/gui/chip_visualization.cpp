/*
 * chip_visualization.cpp - Generic chip visualization system implementation
 */

#include "chip_visualization.h"
#include <algorithm>
#include <cstdio>

// ============================================================================
// COLOR DEFINITIONS AND VISUAL CONFIGURATION
// ============================================================================

uint32_t get_pin_type_color(PinType type, VisualStyle style) {
    // Colors in ABGR format for ImGui
    
    if (style == VisualStyle::MONOCHROME) {
        return 0xFF808080; // All pins gray in monochrome
    }
    
    bool high_contrast = (style == VisualStyle::HIGH_CONTRAST);
    bool light_theme = (style == VisualStyle::CLASSIC_LIGHT || style == VisualStyle::DATASHEET);
    
    switch (type) {
        case PinType::POWER:
            return high_contrast ? 0xFF0000FF : (light_theme ? 0xFFCCCCFF : 0xFF6464FF); // Red
        case PinType::CLOCK:
            return high_contrast ? 0xFFFFFF00 : (light_theme ? 0xFFCCFFFF : 0xFF64FFFF); // Yellow
        case PinType::ADDRESS:
            return high_contrast ? 0xFFFFCC00 : (light_theme ? 0xFFFFDDBB : 0xFFFF9664); // Light Blue
        case PinType::DATA:
            return high_contrast ? 0xFF00FF00 : (light_theme ? 0xFFCCFFCC : 0xFF96FF64); // Light Green
        case PinType::CONTROL:
            return high_contrast ? 0xFFFF8000 : (light_theme ? 0xFFFFCC99 : 0xFF6496FF); // Orange
        case PinType::INTERRUPT:
            return high_contrast ? 0xFFFF00FF : (light_theme ? 0xFFFFCCFF : 0xFFFF64FF); // Magenta
        case PinType::SPECIAL:
            return high_contrast ? 0xFFCCCCCC : (light_theme ? 0xFFE0E0E0 : 0xFFC8C8C8); // Light Gray
        case PinType::IO_PORT:
            return high_contrast ? 0xFFFFCC80 : (light_theme ? 0xFFFFDDCC : 0xFF64C8FF); // Pink
        case PinType::ANALOG:
            return high_contrast ? 0xFF00CCFF : (light_theme ? 0xFFCCEEFF : 0xFF00A8FF); // Cyan
        case PinType::DIFFERENTIAL:
            return high_contrast ? 0xFF80FF80 : (light_theme ? 0xFFDDFFDD : 0xFF64FFC8); // Mint
        case PinType::NO_CONNECT:
            return high_contrast ? 0xFF404040 : (light_theme ? 0xFFC0C0C0 : 0xFF404040); // Dark Gray
        default:
            return 0xFF808080; // Gray
    }
}

ChipVisualConfig ChipVisualConfig::get_default() {
    return get_style(VisualStyle::CLASSIC_DARK);
}

ChipVisualConfig ChipVisualConfig::get_style(VisualStyle style) {
    ChipVisualConfig config;
    config.style = style;
    
    switch (style) {
        case VisualStyle::CLASSIC_DARK:
            config.chip_body_color = 0xFF2D2D30;
            config.chip_border_color = 0xFF808080;
            config.pin_border_color = 0xFF000000;
            config.text_color = 0xFFFFFFFF;
            config.active_text_color = 0xFF00FF00;
            config.pin_number_color = 0xFFA0A0A0;
            config.led_active_color = 0xFF00FF00;
            config.led_inactive_color = 0xFF404040;
            config.notch_color = 0xFF404040;
            config.marker_color = 0xFF606060;
            config.thermal_pad_color = 0xFF404060;
            config.group_border_color = 0xFF606060;
            break;
            
        case VisualStyle::CLASSIC_LIGHT:
            config.chip_body_color = 0xFFF0F0F0;
            config.chip_border_color = 0xFF404040;
            config.pin_border_color = 0xFF000000;
            config.text_color = 0xFF000000;
            config.active_text_color = 0xFF008000;
            config.pin_number_color = 0xFF606060;
            config.led_active_color = 0xFF00C000;
            config.led_inactive_color = 0xFFC0C0C0;
            config.notch_color = 0xFFC0C0C0;
            config.marker_color = 0xFF808080;
            config.thermal_pad_color = 0xFFD0D0E0;
            config.group_border_color = 0xFF808080;
            break;
            
        case VisualStyle::HIGH_CONTRAST:
            config.chip_body_color = 0xFF000000;
            config.chip_border_color = 0xFFFFFFFF;
            config.pin_border_color = 0xFFFFFFFF;
            config.text_color = 0xFFFFFFFF;
            config.active_text_color = 0xFF00FF00;
            config.pin_number_color = 0xFFFFFFFF;
            config.led_active_color = 0xFF00FF00;
            config.led_inactive_color = 0xFF808080;
            config.notch_color = 0xFFFFFFFF;
            config.marker_color = 0xFFFFFFFF;
            config.thermal_pad_color = 0xFF404040;
            config.group_border_color = 0xFFFFFFFF;
            break;
            
        case VisualStyle::COLORFUL:
            config.chip_body_color = 0xFF1A1A2E;
            config.chip_border_color = 0xFF00D4FF;
            config.pin_border_color = 0xFF000000;
            config.text_color = 0xFFFFFFFF;
            config.active_text_color = 0xFF00FFFF;
            config.pin_number_color = 0xFFC0C0FF;
            config.led_active_color = 0xFF00FFFF;
            config.led_inactive_color = 0xFF404060;
            config.notch_color = 0xFF606080;
            config.marker_color = 0xFF8080FF;
            config.thermal_pad_color = 0xFF404080;
            config.group_border_color = 0xFF6060A0;
            break;
            
        case VisualStyle::MONOCHROME:
            config.chip_body_color = 0xFF202020;
            config.chip_border_color = 0xFFFFFFFF;
            config.pin_border_color = 0xFFFFFFFF;
            config.text_color = 0xFFFFFFFF;
            config.active_text_color = 0xFFFFFFFF;
            config.pin_number_color = 0xFFC0C0C0;
            config.led_active_color = 0xFFFFFFFF;
            config.led_inactive_color = 0xFF606060;
            config.notch_color = 0xFF808080;
            config.marker_color = 0xFFC0C0C0;
            config.thermal_pad_color = 0xFF404040;
            config.group_border_color = 0xFF808080;
            break;
            
        case VisualStyle::DATASHEET:
            config.chip_body_color = 0xFFFFFFFF;
            config.chip_border_color = 0xFF000000;
            config.pin_border_color = 0xFF000000;
            config.text_color = 0xFF000000;
            config.active_text_color = 0xFF000000;
            config.pin_number_color = 0xFF000000;
            config.led_active_color = 0xFF808080;
            config.led_inactive_color = 0xFFD0D0D0;
            config.notch_color = 0xFFE0E0E0;
            config.marker_color = 0xFF000000;
            config.thermal_pad_color = 0xFFF0F0F0;
            config.group_border_color = 0xFF000000;
            break;
            
        case VisualStyle::SCHEMATIC:
            config.chip_body_color = 0xFFFFFFFF;
            config.chip_border_color = 0xFF000000;
            config.pin_border_color = 0xFF000000;
            config.text_color = 0xFF000000;
            config.active_text_color = 0xFF000000;
            config.pin_number_color = 0xFF000000;
            config.led_active_color = 0xFF000000;
            config.led_inactive_color = 0xFFFFFFFF;
            config.notch_color = 0xFFE0E0E0;
            config.marker_color = 0xFF000000;
            config.thermal_pad_color = 0xFFF8F8F8;
            config.group_border_color = 0xFF000000;
            break;
    }
    
    // Common settings
    config.pin_width = 8.0f;
    config.pin_height = 12.0f;
    config.led_radius = 3.0f;
    config.label_offset = 15.0f;
    config.pin_spacing_factor = 1.2f;
    config.chip_border_width = 2.0f;
    config.pin_border_width = 1.0f;
    config.marker_size = 8.0f;
    config.font_size = 12.0f;
    
    config.show_pin_numbers = true;
    config.show_pin_labels = true;
    config.show_led_indicators = (style != VisualStyle::DATASHEET && style != VisualStyle::SCHEMATIC);
    config.show_package_name = true;
    config.show_chip_markings = true;
    config.show_pin_groups = false;
    config.show_thermal_pad = true;
    config.show_alternate_functions = false;
    config.show_voltage_levels = false;
    config.show_pwm_indicators = false;
    config.use_compact_layout = false;
    
    config.notation_style = PinNotationStyle::SLASH_PREFIX;
    
    return config;
}

// ============================================================================
// CHIP VISUALIZATION CLASS IMPLEMENTATION
// ============================================================================

ChipVisualization::ChipVisualization(const PinLayout& layout, const ChipVisualConfig& config)
    : layout_(layout), config_(config) {
}

void ChipVisualization::render(ImVec2 chip_center, const std::vector<PinState>& pin_states, const char* chip_name) {
    render_chip_body(chip_center, chip_name);
    render_orientation_marker(chip_center);
    
    if (layout_.package.has_thermal_pad) {
        render_thermal_pad(chip_center);
    }
    
    if (config_.show_chip_markings) {
        render_chip_markings(chip_center);
    }
    
    if (config_.show_pin_groups) {
        render_pin_groups(chip_center);
    }
    
    // Render based on package type
    if (layout_.package.package_type == PackageType::BGA || 
        layout_.package.package_type == PackageType::LGA) {
        render_bga_grid(chip_center, pin_states);
    } else {
        render_pins(chip_center, pin_states);
    }
}

void ChipVisualization::render_chip_body(ImVec2 chip_center, const char* chip_name) {
    ImDrawList* draw_list = igGetWindowDrawList();
    
    // Scale chip dimensions from mils to reasonable pixel size
    // Target: ~150 pixels for left side CPU debug GUI
    const float MAX_CHIP_SIZE = 300.0f; // Maximum size in pixels
    const float ASPECT_RATIO = layout_.package.width / layout_.package.height;
    
    float chip_width, chip_height;
    if (ASPECT_RATIO > 1.0f) {
        // Wider than tall
        chip_width = MAX_CHIP_SIZE;
        chip_height = MAX_CHIP_SIZE / ASPECT_RATIO;
    } else {
        // Taller than wide
        chip_height = MAX_CHIP_SIZE;
        chip_width = MAX_CHIP_SIZE * ASPECT_RATIO;
    }
    
    // For DIP packages, ensure minimum width for pin visibility
    if (layout_.package.package_type == PackageType::DIP) {
        chip_width = std::max(chip_width, 80.0f);  // Minimum width for DIP
        chip_height = std::max(chip_height, 120.0f); // Minimum height for DIP
    }
    
    // Store scaled dimensions for use by other functions
    scaled_chip_width_ = chip_width;
    scaled_chip_height_ = chip_height;
    
    // Draw chip body based on package type
    switch (layout_.package.package_type) {
        case PackageType::DIP:
        case PackageType::SIP:
            render_dip_style(chip_center, chip_width, chip_height, chip_name);
            break;
            
        case PackageType::SOIC:
        case PackageType::SOP:
        case PackageType::SSOP:
        case PackageType::TSSOP:
            render_surface_mount_style(chip_center, chip_width, chip_height, chip_name);
            break;
            
        case PackageType::QFP:
        case PackageType::LQFP:
        case PackageType::TQFP:
        case PackageType::PLCC:
            render_qfp_style(chip_center, chip_width, chip_height, chip_name);
            break;
            
        case PackageType::BGA:
        case PackageType::LGA:
            render_bga_style(chip_center, chip_width, chip_height, chip_name);
            break;
            
        case PackageType::TO220:
        case PackageType::TO92:
        case PackageType::SOT23:
        case PackageType::SOT223:
            render_to_style(chip_center, chip_width, chip_height, chip_name);
            break;
            
        default:
            // Generic rectangular body
            ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
            ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
            ImDrawList_AddRectFilled(draw_list, chip_min, chip_max, config_.chip_body_color, 0.0f, 0);
            ImDrawList_AddRect(draw_list, chip_min, chip_max, config_.chip_border_color, 0.0f, 0, config_.chip_border_width);
            break;
    }
    
    // Draw chip name if provided
    if (chip_name) {
        ImVec2 label_size;
        igCalcTextSize(&label_size, chip_name, NULL, false, -1.0f);
        ImVec2 label_pos = {chip_center.x - label_size.x/2, chip_center.y - 10};
        ImDrawList_AddText_Vec2(draw_list, label_pos, config_.text_color, chip_name, NULL);
    }
    
    // Draw package name if enabled
    if (config_.show_package_name) {
        std::string package_name_str = get_package_type_string(layout_.package.package_type);
        const char* package_name = package_name_str.c_str();
        ImVec2 package_label_size;
        igCalcTextSize(&package_label_size, package_name, NULL, false, -1.0f);
        ImVec2 package_label_pos = {chip_center.x - package_label_size.x/2, chip_center.y + 15};
        ImDrawList_AddText_Vec2(draw_list, package_label_pos, config_.pin_number_color, 
                               package_name, NULL);
    }
}

void ChipVisualization::render_dip_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    ImDrawList* draw_list = igGetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // Draw rounded rectangle for DIP
    ImDrawList_AddRectFilled(draw_list, chip_min, chip_max, config_.chip_body_color, 2.0f, 0);
    ImDrawList_AddRect(draw_list, chip_min, chip_max, config_.chip_border_color, 2.0f, 0, config_.chip_border_width);
}

void ChipVisualization::render_surface_mount_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    ImDrawList* draw_list = igGetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // Surface mount packages are flatter looking
    ImDrawList_AddRectFilled(draw_list, chip_min, chip_max, config_.chip_body_color, 1.0f, 0);
    ImDrawList_AddRect(draw_list, chip_min, chip_max, config_.chip_border_color, 1.0f, 0, config_.chip_border_width);
}

void ChipVisualization::render_qfp_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    ImDrawList* draw_list = igGetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // QFP packages are typically square with chamfered corner
    ImDrawList_AddRectFilled(draw_list, chip_min, chip_max, config_.chip_body_color, 0.0f, 0);
    ImDrawList_AddRect(draw_list, chip_min, chip_max, config_.chip_border_color, 0.0f, 0, config_.chip_border_width);
}

void ChipVisualization::render_bga_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    ImDrawList* draw_list = igGetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // BGA packages show top view - square with marker
    ImDrawList_AddRectFilled(draw_list, chip_min, chip_max, config_.chip_body_color, 0.0f, 0);
    ImDrawList_AddRect(draw_list, chip_min, chip_max, config_.chip_border_color, 0.0f, 0, config_.chip_border_width * 1.5f);
}

void ChipVisualization::render_to_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    ImDrawList* draw_list = igGetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // TO packages have distinctive shape with heat sink tab
    ImDrawList_AddRectFilled(draw_list, chip_min, chip_max, config_.chip_body_color, 3.0f, 0);
    ImDrawList_AddRect(draw_list, chip_min, chip_max, config_.chip_border_color, 3.0f, 0, config_.chip_border_width);
    
    // Draw heat sink tab at top
    if (layout_.package.package_type == PackageType::TO220) {
        ImVec2 tab_min = {chip_center.x - chip_width/3, chip_min.y - 15};
        ImVec2 tab_max = {chip_center.x + chip_width/3, chip_min.y};
        ImDrawList_AddRectFilled(draw_list, tab_min, tab_max, config_.thermal_pad_color, 1.0f, 0);
        ImDrawList_AddRect(draw_list, tab_min, tab_max, config_.chip_border_color, 1.0f, 0, 1.0f);
    }
}

void ChipVisualization::render_orientation_marker(ImVec2 chip_center) {
    switch (layout_.package.marker) {
        case OrientationMarker::NOTCH:
            draw_notch(chip_center);
            break;
        case OrientationMarker::DOT:
            draw_dot_marker(chip_center);
            break;
        case OrientationMarker::CHAMFER:
            draw_chamfer(chip_center);
            break;
        case OrientationMarker::BAR:
            draw_bar_marker(chip_center);
            break;
        case OrientationMarker::TRIANGLE:
            draw_triangle_marker(chip_center);
            break;
        case OrientationMarker::NOTCH_AND_DOT:
            draw_notch(chip_center);
            draw_dot_marker(chip_center);
            break;
        case OrientationMarker::CIRCLE:
            // Similar to dot but unfilled
            {
                ImDrawList* draw_list = igGetWindowDrawList();
                float chip_width = layout_.package.width;
                float chip_height = layout_.package.height;
                ImVec2 marker_pos = {chip_center.x - chip_width/2 + 15, chip_center.y - chip_height/2 + 15};
                ImDrawList_AddCircle(draw_list, marker_pos, config_.marker_size, config_.marker_color, 12, 2.0f);
            }
            break;
        default:
            break;
    }
}

void ChipVisualization::draw_notch(ImVec2 chip_center) {
    ImDrawList* draw_list = igGetWindowDrawList();
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    ImVec2 notch_center = {chip_center.x, chip_center.y - chip_height/2};
    ImDrawList_AddCircleFilled(draw_list, notch_center, config_.marker_size, config_.notch_color, 12);
    ImDrawList_AddCircle(draw_list, notch_center, config_.marker_size, config_.chip_border_color, 12, 1.0f);
}

void ChipVisualization::draw_dot_marker(ImVec2 chip_center) {
    ImDrawList* draw_list = igGetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    ImVec2 marker_pos = {chip_center.x - chip_width/2 + 15, chip_center.y - chip_height/2 + 15};
    ImDrawList_AddCircleFilled(draw_list, marker_pos, config_.marker_size * 0.6f, config_.marker_color, 12);
}

void ChipVisualization::draw_chamfer(ImVec2 chip_center) {
    ImDrawList* draw_list = igGetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    float chamfer_size = config_.marker_size * 2;
    
    ImVec2 corner = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 p1 = corner;
    ImVec2 p2 = {corner.x + chamfer_size, corner.y};
    ImVec2 p3 = {corner.x, corner.y + chamfer_size};
    
    ImDrawList_AddTriangleFilled(draw_list, p1, p2, p3, config_.notch_color);
    ImDrawList_AddTriangle(draw_list, p1, p2, p3, config_.chip_border_color, 1.0f);
}

void ChipVisualization::draw_bar_marker(ImVec2 chip_center) {
    ImDrawList* draw_list = igGetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    
    ImVec2 bar_min = {chip_center.x - chip_width/3, chip_center.y - chip_height/2 + 5};
    ImVec2 bar_max = {chip_center.x + chip_width/3, chip_center.y - chip_height/2 + 8};
    ImDrawList_AddRectFilled(draw_list, bar_min, bar_max, config_.marker_color, 0.0f, 0);
}

void ChipVisualization::draw_triangle_marker(ImVec2 chip_center) {
    ImDrawList* draw_list = igGetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    float size = config_.marker_size;
    
    ImVec2 tip = {chip_center.x - chip_width/2 + 10, chip_center.y - chip_height/2 + 10};
    ImVec2 p2 = {tip.x + size, tip.y - size/2};
    ImVec2 p3 = {tip.x + size, tip.y + size/2};
    
    ImDrawList_AddTriangleFilled(draw_list, tip, p2, p3, config_.marker_color);
    ImDrawList_AddTriangle(draw_list, tip, p2, p3, config_.chip_border_color, 1.0f);
}

void ChipVisualization::render_thermal_pad(ImVec2 chip_center) {
    if (!layout_.package.has_thermal_pad) return;
    
    ImDrawList* draw_list = igGetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    float pad_size = layout_.package.thermal_pad_size * std::min(chip_width, chip_height);
    
    ImVec2 pad_min = {chip_center.x - pad_size/2, chip_center.y - pad_size/2};
    ImVec2 pad_max = {chip_center.x + pad_size/2, chip_center.y + pad_size/2};
    
    ImDrawList_AddRectFilled(draw_list, pad_min, pad_max, config_.thermal_pad_color, 0.0f, 0);
    ImDrawList_AddRect(draw_list, pad_min, pad_max, config_.pin_border_color, 0.0f, 0, 1.0f);
    
    // Add cross-hatch pattern
    for (int i = -3; i <= 3; i++) {
        float offset = i * pad_size / 8;
        ImVec2 line1_start = {chip_center.x + offset, pad_min.y};
        ImVec2 line1_end = {chip_center.x + offset, pad_max.y};
        ImDrawList_AddLine(draw_list, line1_start, line1_end, config_.pin_border_color, 0.5f);
        
        ImVec2 line2_start = {pad_min.x, chip_center.y + offset};
        ImVec2 line2_end = {pad_max.x, chip_center.y + offset};
        ImDrawList_AddLine(draw_list, line2_start, line2_end, config_.pin_border_color, 0.5f);
    }
}

void ChipVisualization::render_chip_markings(ImVec2 chip_center) {
    ImDrawList* draw_list = igGetWindowDrawList();
    float y_offset = -40;
    float line_height = 14;
    
    auto draw_marking = [&](const char* text) {
        if (text && strlen(text) > 0) {
            ImVec2 size;
            igCalcTextSize(&size, text, NULL, false, -1.0f);
            ImVec2 pos = {chip_center.x - size.x/2, chip_center.y + y_offset};
            ImDrawList_AddText_Vec2(draw_list, pos, config_.pin_number_color, text, NULL);
            y_offset += line_height;
        }
    };
    
    if (layout_.markings.show_part_number && layout_.markings.part_number) {
        draw_marking(layout_.markings.part_number);
    }
    if (layout_.markings.show_manufacturer && layout_.markings.manufacturer) {
        draw_marking(layout_.markings.manufacturer);
    }
    if (layout_.markings.show_package_variant && layout_.markings.package_variant) {
        draw_marking(layout_.markings.package_variant);
    }
    if (layout_.markings.show_date_code && layout_.markings.date_code) {
        draw_marking(layout_.markings.date_code);
    }
    if (layout_.markings.custom_text) {
        draw_marking(layout_.markings.custom_text);
    }
}

void ChipVisualization::render_pin_groups(ImVec2 chip_center) {
    // Draw boxes around grouped pins (e.g., address bus, data bus)
    // This would analyze pin groups and draw bounding boxes
    // Implementation omitted for brevity
}

void ChipVisualization::render_pins(ImVec2 chip_center, const std::vector<PinState>& pin_states) {
    render_pin_side(chip_center, layout_.left_pins, pin_states, PinSide::LEFT);
    render_pin_side(chip_center, layout_.right_pins, pin_states, PinSide::RIGHT);
    render_pin_side(chip_center, layout_.top_pins, pin_states, PinSide::TOP);
    render_pin_side(chip_center, layout_.bottom_pins, pin_states, PinSide::BOTTOM);
}

void ChipVisualization::render_bga_grid(ImVec2 chip_center, const std::vector<PinState>& pin_states) {
    // Render BGA ball grid
    // Calculate grid dimensions
    uint8_t max_row = 0, max_col = 0;
    for (const auto& pin : layout_.grid_pins) {
        // Parse pin number to extract row/col
        // BGA pins typically numbered like A1, B2, etc.
    }
    
    // Draw grid of balls
    ImDrawList* draw_list = igGetWindowDrawList();
    for (size_t i = 0; i < layout_.grid_pins.size(); i++) {
        const auto& pin = layout_.grid_pins[i];
        
        // Extract row/col from pin number or use index
        uint8_t row = i / 10; // Simplified - real implementation would parse label
        uint8_t col = i % 10;
        
        ImVec2 ball_pos = calculate_bga_position(chip_center, row, col);
        
        PinState state = {0, false, false, 0, false, false, false, true, 0.0f, false, 0.0f};
        if (pin.pin_number <= pin_states.size()) {
            state = pin_states[pin.pin_number - 1];
        }
        
        uint32_t ball_color = get_pin_type_color(pin.type, config_.style);
        if (state.is_active) {
            ball_color = config_.led_active_color;
        }
        
        ImDrawList_AddCircleFilled(draw_list, ball_pos, 3.0f, ball_color, 12);
        ImDrawList_AddCircle(draw_list, ball_pos, 3.0f, config_.pin_border_color, 12, 1.0f);
        
        // Draw label if enabled
        if (config_.show_pin_labels && pin.label) {
            ImVec2 label_pos = {ball_pos.x - 8, ball_pos.y - 6};
            ImDrawList_AddText_Vec2(draw_list, label_pos, config_.text_color, pin.label, NULL);
        }
    }
}

void ChipVisualization::render_pin_side(ImVec2 chip_center, const std::vector<ChipPin>& pins, 
                                       const std::vector<PinState>& pin_states, PinSide side) {
    for (size_t i = 0; i < pins.size(); i++) {
        const ChipPin& pin = pins[i];
        
        // Find corresponding pin state
        PinState state = {0, false, false, 0, false, false, false, true, 0.0f, false, 0.0f};
        if (pin.pin_number > 0 && pin.pin_number <= pin_states.size()) {
            state = pin_states[pin.pin_number - 1];
        }
        
        ImVec2 pin_pos = calculate_pin_position(chip_center, pin, i, side);
        render_single_pin(pin_pos, pin, state, side);
    }
}

void ChipVisualization::render_single_pin(ImVec2 pin_pos, const ChipPin& pin, const PinState& state, PinSide side) {
    ImDrawList* draw_list = igGetWindowDrawList();
    
    // Get pin type color
    uint32_t pin_color = get_pin_type_color(pin.type, config_.style);
    
    // Modify color based on state
    if (state.is_tristate) {
        pin_color = (pin_color & 0x00FFFFFF) | 0x80000000; // Semi-transparent
    }
    
    // Draw pin rectangle
    ImVec2 pin_min = {pin_pos.x - config_.pin_width/2, pin_pos.y - config_.pin_height/2};
    ImVec2 pin_max = {pin_pos.x + config_.pin_width/2, pin_pos.y + config_.pin_height/2};
    
    ImDrawList_AddRectFilled(draw_list, pin_min, pin_max, pin_color, 0.0f, 0);
    ImDrawList_AddRect(draw_list, pin_min, pin_max, config_.pin_border_color, 0.0f, 0, config_.pin_border_width);
    
    // Draw LED indicator if enabled
    if (config_.show_led_indicators) {
        ImVec2 led_pos = get_led_position(pin_pos, side);
        uint32_t led_color = state.is_active ? config_.led_active_color : config_.led_inactive_color;
        
        ImDrawList_AddCircleFilled(draw_list, led_pos, config_.led_radius, led_color, 12);
        ImDrawList_AddCircle(draw_list, led_pos, config_.led_radius, config_.pin_border_color, 12, 1.0f);
    }
    
    // Draw PWM indicator if enabled and applicable
    if (config_.show_pwm_indicators && state.is_pwm) {
         ImVec2 led_pos = get_led_position(pin_pos, side);
         float pwm_angle = state.pwm_duty_cycle * 2.0f * 3.14159f;
        ImDrawList_AddCircle(draw_list, led_pos, config_.led_radius + 2, 0xFF00FFFF, 12, 2.0f);
    }
    
    // Draw voltage level indicator for analog pins
    if (config_.show_voltage_levels && pin.type == PinType::ANALOG && state.is_valid) {
        ImVec2 led_pos = get_led_position(pin_pos, side);
        char voltage_str[16];
        snprintf(voltage_str, sizeof(voltage_str), "%.2fV", state.analog_voltage);
        ImVec2 voltage_pos = {led_pos.x - 15, led_pos.y + 10};
        ImDrawList_AddText_Vec2(draw_list, voltage_pos, config_.text_color, voltage_str, NULL);
    }
    
    // Draw pin label if enabled
    if (config_.show_pin_labels) {
        ImVec2 label_pos = get_label_position(pin_pos, pin, side);
        uint32_t text_color = state.is_active ? config_.active_text_color : config_.text_color;
        
        std::string formatted_label = format_pin_label(pin);
        ImDrawList_AddText_Vec2(draw_list, label_pos, text_color, formatted_label.c_str(), NULL);
        
        // Show alternate function if enabled
        if (config_.show_alternate_functions && pin.alt_function) {
            ImVec2 alt_pos = label_pos;
            alt_pos.y += 12;
            char alt_text[64];
            snprintf(alt_text, sizeof(alt_text), "(%s)", pin.alt_function);
            ImDrawList_AddText_Vec2(draw_list, alt_pos, config_.pin_number_color, alt_text, NULL);
        }
    }
    
    // Draw pin number if enabled
    if (config_.show_pin_numbers) {
        char pin_num_str[4];
        snprintf(pin_num_str, sizeof(pin_num_str), "%d", pin.pin_number);
        
        ImVec2 pin_num_pos;
        switch (side) {
            case PinSide::LEFT:
                pin_num_pos = {pin_pos.x + config_.pin_width/2 + 2, pin_pos.y - 6};
                break;
            case PinSide::RIGHT:
                pin_num_pos = {pin_pos.x - config_.pin_width/2 - 10, pin_pos.y - 6};
                break;
            case PinSide::TOP:
                pin_num_pos = {pin_pos.x - 6, pin_pos.y + config_.pin_height/2 + 2};
                break;
            case PinSide::BOTTOM:
                pin_num_pos = {pin_pos.x - 6, pin_pos.y - config_.pin_height/2 - 10};
                break;
        }
        
        ImDrawList_AddText_Vec2(draw_list, pin_num_pos, config_.pin_number_color, pin_num_str, NULL);
    }
    
    // Draw differential pair indicator
    if (pin.is_differential_pos || pin.is_differential_neg) {
        ImVec2 diff_pos = pin_pos;
        diff_pos.x += config_.pin_width/2 + 1;
        const char* diff_marker = pin.is_differential_pos ? "+" : "-";
        ImDrawList_AddText_Vec2(draw_list, diff_pos, config_.text_color, diff_marker, NULL);
    }
}

ImVec2 ChipVisualization::calculate_pin_position(ImVec2 chip_center, const ChipPin& pin, 
                                                 size_t index_in_side, PinSide side) const {
    // Use scaled dimensions instead of raw package dimensions
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    float margin = 20.0f; // Reduced margin for smaller chip size
    
    ImVec2 pin_pos = chip_center;
    
    switch (side) {
        case PinSide::LEFT: {
            pin_pos.x = chip_center.x - chip_width/2;
            float total_height = chip_height - margin;
            float pin_spacing = layout_.left_pins.size() > 1 ? 
                total_height / (layout_.left_pins.size() - 1) : 0;
            pin_pos.y = chip_center.y - total_height/2 + index_in_side * pin_spacing;
            break;
        }
        case PinSide::RIGHT: {
            pin_pos.x = chip_center.x + chip_width/2;
            float total_height = chip_height - margin;
            float pin_spacing = layout_.right_pins.size() > 1 ? 
                total_height / (layout_.right_pins.size() - 1) : 0;
            pin_pos.y = chip_center.y - total_height/2 + index_in_side * pin_spacing;
            break;
        }
        case PinSide::TOP: {
            pin_pos.y = chip_center.y - chip_height/2;
            float total_width = chip_width - margin;
            float pin_spacing = layout_.top_pins.size() > 1 ? 
                total_width / (layout_.top_pins.size() - 1) : 0;
            pin_pos.x = chip_center.x - total_width/2 + index_in_side * pin_spacing;
            break;
        }
        case PinSide::BOTTOM: {
            pin_pos.y = chip_center.y + chip_height/2;
            float total_width = chip_width - margin;
            float pin_spacing = layout_.bottom_pins.size() > 1 ? 
                total_width / (layout_.bottom_pins.size() - 1) : 0;
            pin_pos.x = chip_center.x - total_width/2 + index_in_side * pin_spacing;
            break;
        }
    }
    
    return pin_pos;
}

ImVec2 ChipVisualization::calculate_bga_position(ImVec2 chip_center, uint8_t row, uint8_t col) const {
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    float margin = 15.0f; // Reduced margin for smaller chip
    
    // Calculate grid dimensions
    uint8_t max_rows = 10; // Would be calculated from actual pins
    uint8_t max_cols = 10;
    
    float grid_width = chip_width - margin;
    float grid_height = chip_height - margin;
    float cell_width = grid_width / (max_cols - 1);
    float cell_height = grid_height / (max_rows - 1);
    
    ImVec2 pos;
    pos.x = chip_center.x - grid_width/2 + col * cell_width;
    pos.y = chip_center.y - grid_height/2 + row * cell_height;
    
    return pos;
}

ImVec2 ChipVisualization::get_led_position(ImVec2 pin_pos, PinSide side) const {
    ImVec2 led_pos = pin_pos;
    float offset = config_.led_radius + 2;
    
    switch (side) {
        case PinSide::LEFT:
            led_pos.x -= config_.pin_width/2 + offset;
            break;
        case PinSide::RIGHT:
            led_pos.x += config_.pin_width/2 + offset;
            break;
        case PinSide::TOP:
            led_pos.y -= config_.pin_height/2 + offset;
            break;
        case PinSide::BOTTOM:
            led_pos.y += config_.pin_height/2 + offset;
            break;
    }
    
    return led_pos;
}

ImVec2 ChipVisualization::get_label_position(ImVec2 pin_pos, const ChipPin& pin, PinSide side) const {
    ImVec2 label_pos = pin_pos;
    float offset = config_.label_offset;
    
    if (config_.use_compact_layout) {
        offset *= 0.7f;
    }
    
    switch (side) {
        case PinSide::LEFT:
            label_pos.x -= config_.pin_width/2 + offset + 30;
            label_pos.y -= 6;
            break;
        case PinSide::RIGHT:
            label_pos.x += config_.pin_width/2 + offset;
            label_pos.y -= 6;
            break;
        case PinSide::TOP:
            label_pos.y -= config_.pin_height/2 + offset + 8;
            label_pos.x -= 10;
            break;
        case PinSide::BOTTOM:
            label_pos.y += config_.pin_height/2 + offset;
            label_pos.x -= 10;
            break;
    }
    
    return label_pos;
}

std::string ChipVisualization::format_pin_label(const ChipPin& pin) const {
    if (!pin.invert_logic) {
        return std::string(pin.label);
    }
    
    std::string formatted;
    switch (config_.notation_style) {
        case PinNotationStyle::SLASH_PREFIX:
            formatted = "/" + std::string(pin.label);
            break;
        case PinNotationStyle::OVERLINE:
            // Unicode overline combining character
            formatted = std::string(pin.label) + "\u0305";
            break;
        case PinNotationStyle::TILDE_PREFIX:
            formatted = "~" + std::string(pin.label);
            break;
        case PinNotationStyle::HASH_SUFFIX:
            formatted = std::string(pin.label) + "#";
            break;
        case PinNotationStyle::ASTERISK_SUFFIX:
            formatted = std::string(pin.label) + "*";
            break;
        case PinNotationStyle::N_SUFFIX:
            formatted = std::string(pin.label) + "_N";
            break;
        case PinNotationStyle::BAR_SUFFIX:
            formatted = std::string(pin.label) + "_BAR";
            break;
        default:
            formatted = "/" + std::string(pin.label);
            break;
    }
    
    return formatted;
}

ImVec2 ChipVisualization::get_recommended_size() const {
    float width = (scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f) + 200; // Extra space for labels  
    float height = (scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f) + 100;
    
    // Add extra space for chip markings
    if (config_.show_chip_markings) {
        height += 60;
    }
    
    return {width, height};
}

void ChipVisualization::render_legend() {
    if (igCollapsingHeader_BoolPtr("Pin Type Legend", NULL, 0)) {
        igIndent(16.0f);
        
        struct LegendEntry {
            PinType type;
            const char* name;
            const char* description;
        };
        
        static const LegendEntry legend[] = {
            {PinType::POWER,       "Power",       "VCC, VDD, VSS, GND (power supply)"},
            {PinType::CLOCK,       "Clock",       "Clock signals (CLK, φ0, φ1, φ2, XTAL)"},
            {PinType::ADDRESS,     "Address",     "Address bus (A0-A23, ADDR)"},
            {PinType::DATA,        "Data",        "Data bus (D0-D15, DATA)"},
            {PinType::CONTROL,     "Control",     "Control signals (RW, CS, OE, WE, SYNC)"},
            {PinType::INTERRUPT,   "Interrupt",   "Interrupt lines (IRQ, NMI, INT, RES)"},
            {PinType::SPECIAL,     "Special",     "Special purpose (SO, BE, ML, TEST)"},
            {PinType::IO_PORT,     "I/O Port",    "I/O port lines (PA0-PA7, GPIO)"},
            {PinType::ANALOG,      "Analog",      "Analog signals (AIN, AOUT, VREF)"},
            {PinType::DIFFERENTIAL,"Differential","Differential pairs (TX+/TX-, RX+/RX-)"},
            {PinType::NO_CONNECT,  "No Connect",  "NC pins (leave unconnected)"}
        };
        
        for (const auto& entry : legend) {
            uint32_t color = get_pin_type_color(entry.type, config_.style);
            
            // Draw colored square
            ImVec2 cursor;
            igGetCursorScreenPos(&cursor);
            ImDrawList* draw_list = igGetWindowDrawList();
            ImVec2 square_max = {cursor.x + 12, cursor.y + 12};
            ImDrawList_AddRectFilled(draw_list, cursor, square_max, color, 0.0f, 0);
            ImDrawList_AddRect(draw_list, cursor, square_max, config_.pin_border_color, 0.0f, 0, 1.0f);
            
            ImVec2 legend_dummy = {16, 12};
            igDummy(legend_dummy);
            igSameLine(0, 4);
            igText("%s: %s", entry.name, entry.description);
        }
        
        igSeparator();
        igText("Pin Notation Styles:");
        
        const char* notation_examples[] = {
            "/ prefix: /IRQ, /CS, /WE",
            "~ prefix: ~IRQ, ~CS, ~WE",
            "# suffix: IRQ#, CS#, WE#",
            "* suffix: IRQ*, CS*, WE*",
            "_N suffix: IRQ_N, CS_N, WE_N",
            "_BAR suffix: IRQ_BAR, CS_BAR"
        };
        
        for (const char* example : notation_examples) {
            igBulletText("%s", example);
        }
        
        igSeparator();
        igText("Orientation Markers:");
        igBulletText("Notch: U-shaped cutout at top center");
        igBulletText("Dot: Physical dimple near pin 1");
        igBulletText("Chamfer: Beveled corner near pin 1");
        igBulletText("Bar: Stripe indicating pin 1 side");
        igBulletText("Triangle: Pointer to pin 1");
        
        igUnindent(16.0f);
    }
    
    // Package type information
    if (igCollapsingHeader_BoolPtr("Package Information", NULL, 0)) {
        igIndent(16.0f);
        
        std::string package_name_str = get_package_type_string(layout_.package.package_type);
        igText("Package: %s", package_name_str.c_str());
        
        // Calculate total pins
        int total_pins = layout_.left_pins.size() + layout_.right_pins.size() + 
                        layout_.top_pins.size() + layout_.bottom_pins.size() + 
                        layout_.grid_pins.size();
        igText("Total Pins: %d", total_pins);
        igText("Pin Pitch: %.2f mm", layout_.package.pin_pitch);
        
        const char* package_type_name = "Unknown";
        switch (layout_.package.package_type) {
            case PackageType::DIP:    package_type_name = "DIP (Dual In-line Package)"; break;
            case PackageType::SOIC:   package_type_name = "SOIC (Small Outline IC)"; break;
            case PackageType::PLCC:   package_type_name = "PLCC (Plastic Leaded Chip Carrier)"; break;
            case PackageType::QFP:    package_type_name = "QFP (Quad Flat Package)"; break;
            case PackageType::QFN:    package_type_name = "QFN (Quad Flat No-leads)"; break;
            case PackageType::BGA:    package_type_name = "BGA (Ball Grid Array)"; break;
            case PackageType::TO220:  package_type_name = "TO-220 (Power Package)"; break;
            case PackageType::SOT23:  package_type_name = "SOT-23 (Small Transistor)"; break;
            default: break;
        }
        igText("Type: %s", package_type_name);
        
        if (layout_.package.has_thermal_pad) {
            igText("Thermal Pad: Yes (%.0f%%)", layout_.package.thermal_pad_size * 100);
        }
        
        igUnindent(16.0f);
    }
}

const ChipPin* ChipVisualization::find_pin_by_number(uint8_t pin_number) const {
    auto search_side = [pin_number](const std::vector<ChipPin>& pins) -> const ChipPin* {
        for (const auto& pin : pins) {
            if (pin.pin_number == pin_number) {
                return &pin;
            }
        }
        return nullptr;
    };
    
    if (auto* pin = search_side(layout_.left_pins)) return pin;
    if (auto* pin = search_side(layout_.right_pins)) return pin;
    if (auto* pin = search_side(layout_.top_pins)) return pin;
    if (auto* pin = search_side(layout_.bottom_pins)) return pin;
    if (auto* pin = search_side(layout_.grid_pins)) return pin;
    
    return nullptr;
}

const ChipPin* ChipVisualization::find_pin_by_label(const char* label) const {
    auto search_side = [label](const std::vector<ChipPin>& pins) -> const ChipPin* {
        for (const auto& pin : pins) {
            if (strcmp(pin.label, label) == 0) {
                return &pin;
            }
        }
        return nullptr;
    };
    
    if (auto* pin = search_side(layout_.left_pins)) return pin;
    if (auto* pin = search_side(layout_.right_pins)) return pin;
    if (auto* pin = search_side(layout_.top_pins)) return pin;
    if (auto* pin = search_side(layout_.bottom_pins)) return pin;
    if (auto* pin = search_side(layout_.grid_pins)) return pin;
    
    return nullptr;
}

std::vector<const ChipPin*> ChipVisualization::find_pins_by_group(const char* group_name) const {
    std::vector<const ChipPin*> result;
    
    auto search_side = [&](const std::vector<ChipPin>& pins) {
        for (const auto& pin : pins) {
            if (pin.group_name && strcmp(pin.group_name, group_name) == 0) {
                result.push_back(&pin);
            }
        }
    };
    
    search_side(layout_.left_pins);
    search_side(layout_.right_pins);
    search_side(layout_.top_pins);
    search_side(layout_.bottom_pins);
    search_side(layout_.grid_pins);
    
    return result;
}

ImVec2 ChipVisualization::get_pin_position(ImVec2 chip_center, const ChipPin& pin) const {
    auto find_in_side = [&pin](const std::vector<ChipPin>& pins) -> size_t {
        for (size_t i = 0; i < pins.size(); i++) {
            if (pins[i].pin_number == pin.pin_number) {
                return i;
            }
        }
        return SIZE_MAX;
    };
    
    size_t index = find_in_side(layout_.left_pins);
    if (index != SIZE_MAX) {
        return calculate_pin_position(chip_center, pin, index, PinSide::LEFT);
    }
    
    index = find_in_side(layout_.right_pins);
    if (index != SIZE_MAX) {
        return calculate_pin_position(chip_center, pin, index, PinSide::RIGHT);
    }
    
    index = find_in_side(layout_.top_pins);
    if (index != SIZE_MAX) {
        return calculate_pin_position(chip_center, pin, index, PinSide::TOP);
    }
    
    index = find_in_side(layout_.bottom_pins);
    if (index != SIZE_MAX) {
        return calculate_pin_position(chip_center, pin, index, PinSide::BOTTOM);
    }
    
    return chip_center;
}

void ChipVisualization::render_settings_gui() {
    if (igCollapsingHeader_TreeNodeFlags("Visualization Settings", ImGuiTreeNodeFlags_None)) {
        bool config_changed = false;
        
        // Pin Display Options
        if (igCollapsingHeader_TreeNodeFlags("Pin Display", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (igCheckbox("Show Pin Numbers", &config_.show_pin_numbers)) config_changed = true;
            if (igCheckbox("Show Pin Labels", &config_.show_pin_labels)) config_changed = true;
            if (igCheckbox("Show LED Indicators", &config_.show_led_indicators)) config_changed = true;
            if (igCheckbox("Show Alternate Functions", &config_.show_alternate_functions)) config_changed = true;
            if (igCheckbox("Show Pin Groups", &config_.show_pin_groups)) config_changed = true;
            if (igCheckbox("Show Voltage Levels", &config_.show_voltage_levels)) config_changed = true;
            if (igCheckbox("Show PWM Indicators", &config_.show_pwm_indicators)) config_changed = true;
        }
        
        // Chip Display Options
        if (igCollapsingHeader_TreeNodeFlags("Chip Display", ImGuiTreeNodeFlags_None)) {
            if (igCheckbox("Show Package Name", &config_.show_package_name)) config_changed = true;
            if (igCheckbox("Show Chip Markings", &config_.show_chip_markings)) config_changed = true;
            if (igCheckbox("Show Thermal Pad", &config_.show_thermal_pad)) config_changed = true;
            if (igCheckbox("Use Compact Layout", &config_.use_compact_layout)) config_changed = true;
        }
        
        // Size Controls
        if (igCollapsingHeader_TreeNodeFlags("Sizing", ImGuiTreeNodeFlags_None)) {
            if (igSliderFloat("Pin Width", &config_.pin_width, 4.0f, 16.0f, "%.1f", ImGuiSliderFlags_None)) config_changed = true;
            if (igSliderFloat("Pin Height", &config_.pin_height, 6.0f, 20.0f, "%.1f", ImGuiSliderFlags_None)) config_changed = true;
            if (igSliderFloat("Pin Spacing", &config_.pin_spacing_factor, 0.5f, 2.0f, "%.1f", ImGuiSliderFlags_None)) config_changed = true;
            if (igSliderFloat("Font Size", &config_.font_size, 8.0f, 16.0f, "%.1f", ImGuiSliderFlags_None)) config_changed = true;
            if (igSliderFloat("Marker Size", &config_.marker_size, 2.0f, 12.0f, "%.1f", ImGuiSliderFlags_None)) config_changed = true;
        }
        
        // Pin Notation Style
        if (igCollapsingHeader_TreeNodeFlags("Pin Notation", ImGuiTreeNodeFlags_None)) {
            const char* notation_items[] = {"OVERLINE", "HASH", "SLASH", "UNDERSCORE"};
            int current_notation = (int)config_.notation_style;
            if (igCombo_Str_arr("Active-Low Style", &current_notation, notation_items, 4, -1)) {
                config_.notation_style = (PinNotationStyle)current_notation;
                config_changed = true;
            }
        }
        
        // Visual Style Presets
        if (igCollapsingHeader_TreeNodeFlags("Style Presets", ImGuiTreeNodeFlags_None)) {
            const char* style_items[] = {"CLASSIC_DARK", "MODERN_LIGHT", "HIGH_CONTRAST", "COLORBLIND_FRIENDLY"};
            int current_style = (int)config_.style;
            if (igCombo_Str_arr("Visual Style", &current_style, style_items, 4, -1)) {
                config_ = ChipVisualConfig::get_style((VisualStyle)current_style);
                config_changed = true;
            }
            
            igSameLine(0, -1.0f);
            if (igButton("Reset to Default", (ImVec2){0, 0})) {
                config_ = ChipVisualConfig::get_default();
                config_changed = true;
            }
        }
        
        // Note: config_changed is handled automatically since we're modifying config_ directly
    }
}