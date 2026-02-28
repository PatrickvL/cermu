/*
 * chip_visualization.cpp - Generic chip visualization system implementation
 */

#include "chip_visualization.h"
#include "global_chip_style.h"
#include <algorithm>
#include <cstdio>

// Only compile ImGui-dependent code when ImGui is available
#ifdef CERMU_HAS_GUI

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
            config.datasheet_grid_color = 0xFF404040;
            config.dimension_line_color = 0xFF808080;
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
            config.datasheet_grid_color = 0xFFD0D0D0;
            config.dimension_line_color = 0xFF808080;
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
            config.datasheet_grid_color = 0xFF808080;
            config.dimension_line_color = 0xFFFFFFFF;
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
            config.datasheet_grid_color = 0xFF404060;
            config.dimension_line_color = 0xFF8080FF;
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
            config.datasheet_grid_color = 0xFF808080;
            config.dimension_line_color = 0xFFFFFFFF;
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
            config.datasheet_grid_color = 0xFFE0E0E0;
            config.dimension_line_color = 0xFF000000;
            break;
            
        case VisualStyle::DATASHEET_70S:
            // Early 70s hand-drafted style - rougher lines, labels outside
            config.chip_body_color = 0xFFFFFFFF;
            config.chip_border_color = 0xFF000000;
            config.pin_border_color = 0xFF000000;
            config.text_color = 0xFF000000;
            config.active_text_color = 0xFF000000;
            config.pin_number_color = 0xFF000000;
            config.led_active_color = 0xFF404040;
            config.led_inactive_color = 0xFFE0E0E0;
            config.notch_color = 0xFFD0D0D0;
            config.marker_color = 0xFF000000;
            config.thermal_pad_color = 0xFFF8F8F8;
            config.group_border_color = 0xFF606060;  // Dashed grouping boxes
            config.datasheet_grid_color = 0xFFF0F0F0;
            config.dimension_line_color = 0xFF000000;
            break;
            
        case VisualStyle::DATASHEET_80S:
            // Mid 80s CAD-generated style - clean lines, labels inside
            config.chip_body_color = 0xFFFFFFFF;
            config.chip_border_color = 0xFF000000;
            config.pin_border_color = 0xFF000000;
            config.text_color = 0xFF000000;
            config.active_text_color = 0xFF000000;
            config.pin_number_color = 0xFF000000;
            config.led_active_color = 0xFF606060;
            config.led_inactive_color = 0xFFE0E0E0;
            config.notch_color = 0xFFE0E0E0;
            config.marker_color = 0xFF000000;
            config.thermal_pad_color = 0xFFF0F0F0;
            config.group_border_color = 0xFF000000;
            config.datasheet_grid_color = 0xFFF0F0F0;
            config.dimension_line_color = 0xFF000000;
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
            config.datasheet_grid_color = 0xFFE0E0E0;
            config.dimension_line_color = 0xFF000000;
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
// CHIP VISUALIZATION CLASS IMPLEMENTATION - STATELESS GLOBAL RENDERER
// ============================================================================

// Global renderer instance
ChipVisualization& GetGlobalChipRenderer() {
    static ChipVisualization instance;
    return instance;
}

const ChipVisualConfig& ChipVisualization::get_visual_config() const {
    return GetGlobalChipConfig();
}

void ChipVisualization::render(const ChipLayout& layout, ImVec2 chip_center, const std::vector<PinSignalState>& pin_states, const char* chip_name) {
    // Draw datasheet grid if enabled (behind everything)
    const auto& config = get_visual_config();
    if (config.show_datasheet_grid) {
        render_datasheet_grid(layout, chip_center);
    }
    
    render_chip_body(layout, chip_center, chip_name);
    render_orientation_marker(layout, chip_center);
    
    if (layout.package.has_thermal_pad) {
        render_thermal_pad(layout, chip_center);
    }
    
    if (config.show_chip_markings) {
        render_chip_markings(layout, chip_center);
    }
    
    if (config.show_pin_groups) {
        render_pin_groups(layout, chip_center);
    }
    
    // Render based on package type
    if (layout.package.package_type == PackageType::BGA ||
        layout.package.package_type == PackageType::LGA) {
        render_bga_grid(layout, chip_center, pin_states);
    } else {
        render_pins(layout, chip_center, pin_states);
    }
    
    // Draw dimension lines if enabled (on top)
    if (config.show_dimension_lines) {
        render_dimension_lines(layout, chip_center);
    }
    
    // Draw pin pitch indicators if enabled
    if (config.show_pin_pitch_indicators) {
        render_pin_pitch_indicators(layout, chip_center);
    }
}

void ChipVisualization::render_chip_body(const ChipLayout& layout, ImVec2 chip_center, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Scale chip dimensions from mils to reasonable pixel size
    // Target: ~150 pixels for left side CPU debug GUI
    const float MAX_CHIP_SIZE = 300.0f; // Maximum size in pixels
    const float ASPECT_RATIO = layout.package.width / layout.package.height;
    
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
    if (layout.package.package_type == PackageType::DIP) {
        chip_width = std::max(chip_width, 80.0f);  // Minimum width for DIP
        chip_height = std::max(chip_height, 120.0f); // Minimum height for DIP
    }
    
    // Store scaled dimensions for use by other functions
    scaled_chip_width_ = chip_width;
    scaled_chip_height_ = chip_height;
    
    // Draw chip body based on package type
    switch (layout.package.package_type) {
        case PackageType::DIP:
        case PackageType::SIP:
            render_dip_style(layout, chip_center, chip_width, chip_height, chip_name);
            break;
            
        case PackageType::SOIC:
        case PackageType::SOP:
        case PackageType::SSOP:
        case PackageType::TSSOP:
            render_surface_mount_style(layout, chip_center, chip_width, chip_height, chip_name);
            break;
            
        case PackageType::QFP:
        case PackageType::LQFP:
        case PackageType::TQFP:
        case PackageType::PLCC:
            render_qfp_style(layout, chip_center, chip_width, chip_height, chip_name);
            break;
            
        case PackageType::BGA:
        case PackageType::LGA:
            render_bga_style(layout, chip_center, chip_width, chip_height, chip_name);
            break;
            
        case PackageType::TO220:
        case PackageType::TO92:
        case PackageType::SOT23:
        case PackageType::SOT223:
            render_to_style(layout, chip_center, chip_width, chip_height, chip_name);
            break;
            
        default:
            // Generic rectangular body
            ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
            ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
            draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 0.0f, 0);
            draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 0.0f, 0, config.chip_border_width);
            break;
    }
    
    // Draw chip name if provided
    if (chip_name) {
        ImVec2 label_size;
        label_size = ImGui::CalcTextSize(chip_name);
        ImVec2 label_pos = {chip_center.x - label_size.x/2, chip_center.y - 10};
        draw_list->AddText(label_pos, config.text_color, chip_name);
    }
    
    // Draw package name if enabled
    if (config.show_package_name) {
        std::string package_name_str = get_package_type_string(layout.package.package_type);
        const char* package_name = package_name_str.c_str();
        ImVec2 package_label_size;
        package_label_size = ImGui::CalcTextSize(package_name);
        ImVec2 package_label_pos = {chip_center.x - package_label_size.x/2, chip_center.y + 15};
        draw_list->AddText(package_label_pos, config.pin_number_color,
                               package_name);
    }
}

// Simplified implementation - just implement the core methods needed
void ChipVisualization::render_dip_style(const ChipLayout& layout, ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // Basic rectangular body with datasheet mode support
    float border_radius = (config.datasheet_mode == DatasheetMode::EXTERNAL_LABELING) ? 1.5f : 2.0f;
    draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, border_radius, 0);
    draw_list->AddRect(chip_min, chip_max, config.chip_border_color, border_radius, 0, config.chip_border_width);
}

void ChipVisualization::render_surface_mount_style(const ChipLayout& layout, ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 1.0f, 0);
    draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 1.0f, 0, config.chip_border_width);
}

void ChipVisualization::render_qfp_style(const ChipLayout& layout, ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 0.0f, 0);
    draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 0.0f, 0, config.chip_border_width);
}

void ChipVisualization::render_bga_style(const ChipLayout& layout, ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 0.0f, 0);
    draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 0.0f, 0, config.chip_border_width * 1.5f);
}

void ChipVisualization::render_to_style(const ChipLayout& layout, ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 3.0f, 0);
    draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 3.0f, 0, config.chip_border_width);
    
    // Draw heat sink tab at top for TO220
    if (layout.package.package_type == PackageType::TO220) {
        ImVec2 tab_min = {chip_center.x - chip_width/3, chip_min.y - 15};
        ImVec2 tab_max = {chip_center.x + chip_width/3, chip_min.y};
        draw_list->AddRectFilled(tab_min, tab_max, config.thermal_pad_color, 1.0f, 0);
        draw_list->AddRect(tab_min, tab_max, config.chip_border_color, 1.0f, 0, 1.0f);
    }
}

void ChipVisualization::render_orientation_marker(const ChipLayout& layout, ImVec2 chip_center) {
    switch (layout.package.marker) {
        case OrientationMarker::NOTCH:
            draw_notch(layout, chip_center);
            break;
        case OrientationMarker::DOT:
            draw_dot_marker(layout, chip_center);
            break;
        case OrientationMarker::CHAMFER:
            draw_chamfer(layout, chip_center);
            break;
        case OrientationMarker::BAR:
            draw_bar_marker(layout, chip_center);
            break;
        case OrientationMarker::TRIANGLE:
            draw_triangle_marker(layout, chip_center);
            break;
        case OrientationMarker::NOTCH_AND_DOT:
            draw_notch(layout, chip_center);
            draw_dot_marker(layout, chip_center);
            break;
        default:
            break;
    }
}

void ChipVisualization::draw_notch(const ChipLayout& layout, ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    ImVec2 notch_center = {chip_center.x, chip_center.y - chip_height/2};
    draw_list->AddCircleFilled(notch_center, config.marker_size, config.notch_color, 12);
    draw_list->AddCircle(notch_center, config.marker_size, config.chip_border_color, 12, 1.0f);
}

void ChipVisualization::draw_dot_marker(const ChipLayout& layout, ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    ImVec2 marker_pos = {chip_center.x - chip_width/2 + 15, chip_center.y - chip_height/2 + 15};
    draw_list->AddCircleFilled(marker_pos, config.marker_size * 0.6f, config.marker_color, 12);
}

void ChipVisualization::draw_chamfer(const ChipLayout& layout, ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    float chamfer_size = config.marker_size * 2;
    
    ImVec2 corner = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 p1 = corner;
    ImVec2 p2 = {corner.x + chamfer_size, corner.y};
    ImVec2 p3 = {corner.x, corner.y + chamfer_size};
    
    draw_list->AddTriangleFilled(p1, p2, p3, config.notch_color);
    draw_list->AddTriangle(p1, p2, p3, config.chip_border_color, 1.0f);
}

void ChipVisualization::draw_bar_marker(const ChipLayout& layout, ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    
    ImVec2 bar_min = {chip_center.x - chip_width/3, chip_center.y - chip_height/2 + 5};
    ImVec2 bar_max = {chip_center.x + chip_width/3, chip_center.y - chip_height/2 + 8};
    draw_list->AddRectFilled(bar_min, bar_max, config.marker_color, 0.0f, 0);
}

void ChipVisualization::draw_triangle_marker(const ChipLayout& layout, ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    float size = config.marker_size;
    
    ImVec2 tip = {chip_center.x - chip_width/2 + 10, chip_center.y - chip_height/2 + 10};
    ImVec2 p2 = {tip.x + size, tip.y - size/2};
    ImVec2 p3 = {tip.x + size, tip.y + size/2};
    
    draw_list->AddTriangleFilled(tip, p2, p3, config.marker_color);
    draw_list->AddTriangle(tip, p2, p3, config.chip_border_color, 1.0f);
}

void ChipVisualization::render_thermal_pad(const ChipLayout& layout, ImVec2 chip_center) {
    if (!layout.package.has_thermal_pad) return;
    
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    float pad_size = layout.package.thermal_pad_size * std::min(chip_width, chip_height);
    
    ImVec2 pad_min = {chip_center.x - pad_size/2, chip_center.y - pad_size/2};
    ImVec2 pad_max = {chip_center.x + pad_size/2, chip_center.y + pad_size/2};
    
    draw_list->AddRectFilled(pad_min, pad_max, config.thermal_pad_color, 0.0f, 0);
    draw_list->AddRect(pad_min, pad_max, config.pin_border_color, 0.0f, 0, 1.0f);
}

void ChipVisualization::render_chip_markings(const ChipLayout& layout, ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float y_offset = -40;
    float line_height = 14;
    
    auto draw_marking = [&](const char* text) {
        if (text != nullptr && text[0] != '\0') {
            ImVec2 size = ImGui::CalcTextSize(text);
            ImVec2 pos = {chip_center.x - size.x/2, chip_center.y + y_offset};
            draw_list->AddText(pos, config.pin_number_color, text);
            y_offset += line_height;
        }
    };
    
    if (layout.markings.show_part_number && !layout.markings.part_number.empty()) {
        draw_marking(layout.markings.part_number.data());
    }
    if (layout.markings.show_manufacturer && !layout.markings.manufacturer.empty()) {
        draw_marking(layout.markings.manufacturer.data());
    }
    if (layout.markings.show_package_variant && !layout.markings.package_variant.empty()) {
        draw_marking(layout.markings.package_variant.data());
    }
    if (layout.markings.show_date_code && !layout.markings.date_code.empty()) {
        draw_marking(layout.markings.date_code.data());
    }
    if (!layout.markings.custom_text.empty()) {
        draw_marking(layout.markings.custom_text.data());
    }
}

void ChipVisualization::render_pin_groups(const ChipLayout& layout, ImVec2 chip_center) {
    // Draw boxes around grouped pins (implementation omitted for brevity)
}

void ChipVisualization::render_pins(const ChipLayout& layout, ImVec2 chip_center, const std::vector<PinSignalState>& pin_states) {
    render_pin_side(layout, chip_center, layout.left_pins, pin_states, PinSide::LEFT);
    render_pin_side(layout, chip_center, layout.right_pins, pin_states, PinSide::RIGHT);
    render_pin_side(layout, chip_center, layout.top_pins, pin_states, PinSide::TOP);
    render_pin_side(layout, chip_center, layout.bottom_pins, pin_states, PinSide::BOTTOM);
}

void ChipVisualization::render_bga_grid(const ChipLayout& layout, ImVec2 chip_center, const std::vector<PinSignalState>& pin_states) {
    // Simplified BGA rendering (implementation omitted for brevity)
}

void ChipVisualization::render_pin_side(const ChipLayout& layout, ImVec2 chip_center, const std::vector<ChipPin>& pins, 
                                       const std::vector<PinSignalState>& pin_states, PinSide side) {
    for (size_t i = 0; i < pins.size(); i++) {
        const ChipPin& pin = pins[i];
        
        // Find corresponding pin state
        PinSignalState state = {0, false, false, 0, false, false, false, true, 0.0f, false, 0.0f};
        if (pin.pin_number > 0 && pin.pin_number <= pin_states.size()) {
            state = pin_states[pin.pin_number - 1];
        }
        
        ImVec2 pin_pos = calculate_pin_position(layout, chip_center, pin, i, side);
        render_single_pin(pin_pos, pin, state, side);
    }
}

void ChipVisualization::render_single_pin(ImVec2 pin_pos, const ChipPin& pin, const PinSignalState& state, PinSide side) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Get pin type color
    uint32_t pin_color = get_pin_type_color(pin.get_pin_type(), config.style);
    
    // Draw pin rectangle
    ImVec2 pin_min = {pin_pos.x - config.pin_width/2, pin_pos.y - config.pin_height/2};
    ImVec2 pin_max = {pin_pos.x + config.pin_width/2, pin_pos.y + config.pin_height/2};
    
    draw_list->AddRectFilled(pin_min, pin_max, pin_color, 0.0f, 0);
    draw_list->AddRect(pin_min, pin_max, config.pin_border_color, 0.0f, 0, config.pin_border_width);
    
    // Draw LED indicator if enabled
    if (config.show_led_indicators) {
        ImVec2 led_pos = get_led_position(pin_pos, side);
        uint32_t led_color = state.signal_level ? config.led_active_color : config.led_inactive_color;
        
        draw_list->AddCircleFilled(led_pos, config.led_radius, led_color, 12);
        draw_list->AddCircle(led_pos, config.led_radius, config.pin_border_color, 12, 1.0f);
    }
    
    // Draw pin label if enabled
    if (config.show_pin_labels) {
        ImVec2 label_pos = get_label_position(pin_pos, pin, side);
        uint32_t text_color = state.signal_level ? config.active_text_color : config.text_color;
        
        std::string formatted_label = format_pin_label(pin);
        draw_list->AddText(label_pos, text_color, formatted_label.c_str());
    }
    
    // Draw pin number if enabled
    if (config.show_pin_numbers) {
        char pin_num_str[4];
        snprintf(pin_num_str, sizeof(pin_num_str), "%d", pin.pin_number);
        
        // Basic pin number positioning
        ImVec2 pin_num_pos = {pin_pos.x - 6, pin_pos.y - 6};
        draw_list->AddText(pin_num_pos, config.pin_number_color, pin_num_str);
    }
}

// Simplified helper functions
ImVec2 ChipVisualization::calculate_pin_position(const ChipLayout& layout, ImVec2 chip_center, const ChipPin& pin, 
                                                 size_t index_in_side, PinSide side) const {
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    float margin = 20.0f;
    
    ImVec2 pin_pos = chip_center;
    
    switch (side) {
        case PinSide::LEFT: {
            pin_pos.x = chip_center.x - chip_width/2;
            float total_height = chip_height - margin;
            float pin_spacing = layout.left_pins.size() > 1 ? 
                total_height / (layout.left_pins.size() - 1) : 0;
            pin_pos.y = chip_center.y - total_height/2 + index_in_side * pin_spacing;
            break;
        }
        case PinSide::RIGHT: {
            pin_pos.x = chip_center.x + chip_width/2;
            float total_height = chip_height - margin;
            float pin_spacing = layout.right_pins.size() > 1 ? 
                total_height / (layout.right_pins.size() - 1) : 0;
            pin_pos.y = chip_center.y - total_height/2 + index_in_side * pin_spacing;
            break;
        }
        case PinSide::TOP: {
            pin_pos.y = chip_center.y - chip_height/2;
            float total_width = chip_width - margin;
            float pin_spacing = layout.top_pins.size() > 1 ? 
                total_width / (layout.top_pins.size() - 1) : 0;
            pin_pos.x = chip_center.x - total_width/2 + index_in_side * pin_spacing;
            break;
        }
        case PinSide::BOTTOM: {
            pin_pos.y = chip_center.y + chip_height/2;
            float total_width = chip_width - margin;
            float pin_spacing = layout.bottom_pins.size() > 1 ? 
                total_width / (layout.bottom_pins.size() - 1) : 0;
            pin_pos.x = chip_center.x - total_width/2 + index_in_side * pin_spacing;
            break;
        }
    }
    
    return pin_pos;
}

ImVec2 ChipVisualization::get_led_position(ImVec2 pin_pos, PinSide side) const {
    const auto& config = get_visual_config();
    ImVec2 led_pos = pin_pos;
    float offset = config.led_radius + 2;
    
    switch (side) {
        case PinSide::LEFT:
            led_pos.x -= config.pin_width/2 + offset;
            break;
        case PinSide::RIGHT:
            led_pos.x += config.pin_width/2 + offset;
            break;
        case PinSide::TOP:
            led_pos.y -= config.pin_height/2 + offset;
            break;
        case PinSide::BOTTOM:
            led_pos.y += config.pin_height/2 + offset;
            break;
    }
    
    return led_pos;
}

ImVec2 ChipVisualization::get_label_position(ImVec2 pin_pos, const ChipPin& pin, PinSide side) const {
    const auto& config = get_visual_config();
    ImVec2 label_pos = pin_pos;
    float offset = config.label_offset;
    
    switch (side) {
        case PinSide::LEFT:
            label_pos.x -= config.pin_width/2 + offset;
            label_pos.y -= 6;
            break;
        case PinSide::RIGHT:
            label_pos.x += config.pin_width/2 + offset;
            label_pos.y -= 6;
            break;
        case PinSide::TOP:
            label_pos.y -= config.pin_height/2 + offset;
            label_pos.x -= 10;
            break;
        case PinSide::BOTTOM:
            label_pos.y += config.pin_height/2 + offset;
            label_pos.x -= 10;
            break;
    }
    
    return label_pos;
}

std::string ChipVisualization::format_pin_label(const ChipPin& pin) const {
    const auto& config = get_visual_config();
    const char* label_str = pin_label_to_string(pin.label);
    
    if (!pin.get_invert_logic()) {
        return std::string(label_str);
    }
    
    std::string formatted;
    switch (config.notation_style) {
        case PinNotationStyle::SLASH_PREFIX:
            formatted = "/" + std::string(label_str);
            break;
        case PinNotationStyle::HASH_SUFFIX:
            formatted = std::string(label_str) + "#";
            break;
        default:
            formatted = "/" + std::string(label_str);
            break;
    }
    
    return formatted;
}

// Stub implementations for remaining methods
void ChipVisualization::render_datasheet_grid(const ChipLayout& layout, ImVec2 chip_center) {}
void ChipVisualization::render_dimension_lines(const ChipLayout& layout, ImVec2 chip_center) {}
void ChipVisualization::render_pin_pitch_indicators(const ChipLayout& layout, ImVec2 chip_center) {}
void ChipVisualization::render_connection_diagram_pins(const ChipLayout& layout, ImVec2 chip_center, const std::vector<PinSignalState>& pin_states) {}
ImVec2 ChipVisualization::get_connection_diagram_pin_position(const ChipLayout& layout, ImVec2 chip_center, const ChipPin& pin) const { return chip_center; }

void ChipVisualization::render_settings_gui() {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Settings moved to global configuration!");
    ImGui::Text("Use the 'Chip Visualization Style' menu to configure settings.");
    ImGui::Text("All chip visualizations now share the same global style.");
}

const ChipPin* ChipVisualization::find_pin_by_number(const ChipLayout& layout, uint8_t pin_number) const {
    auto search_side = [pin_number](const std::vector<ChipPin>& pins) -> const ChipPin* {
        for (const auto& pin : pins) {
            if (pin.pin_number == pin_number) {
                return &pin;
            }
        }
        return nullptr;
    };
    
    if (auto* pin = search_side(layout.left_pins)) return pin;
    if (auto* pin = search_side(layout.right_pins)) return pin;
    if (auto* pin = search_side(layout.top_pins)) return pin;
    if (auto* pin = search_side(layout.bottom_pins)) return pin;
    if (auto* pin = search_side(layout.grid_pins)) return pin;
    
    return nullptr;
}

const ChipPin* ChipVisualization::find_pin_by_label(const ChipLayout& layout, const char* label) const {
    auto search_side = [label](const std::vector<ChipPin>& pins) -> const ChipPin* {
        for (const auto& pin : pins) {
            const char* pin_label_str = pin_label_to_string(pin.label);
            if (strcmp(pin_label_str, label) == 0) {
                return &pin;
            }
        }
        return nullptr;
    };
    
    if (auto* pin = search_side(layout.left_pins)) return pin;
    if (auto* pin = search_side(layout.right_pins)) return pin;
    if (auto* pin = search_side(layout.top_pins)) return pin;
    if (auto* pin = search_side(layout.bottom_pins)) return pin;
    if (auto* pin = search_side(layout.grid_pins)) return pin;
    
    return nullptr;
}

std::vector<const ChipPin*> ChipVisualization::find_pins_by_group(const ChipLayout& layout, const char* group_name) const {
    std::vector<const ChipPin*> result;
    
    auto search_side = [&](const std::vector<ChipPin>& pins) {
        for (const auto& pin : pins) {
            if (pin.get_group_name() && strcmp(pin.get_group_name(), group_name) == 0) {
                result.push_back(&pin);
            }
        }
    };
    
    search_side(layout.left_pins);
    search_side(layout.right_pins);
    search_side(layout.top_pins);
    search_side(layout.bottom_pins);
    search_side(layout.grid_pins);
    
    return result;
}

ImVec2 ChipVisualization::get_pin_position(const ChipLayout& layout, ImVec2 chip_center, const ChipPin& pin) const {
    auto find_in_side = [&pin](const std::vector<ChipPin>& pins) -> size_t {
        for (size_t i = 0; i < pins.size(); i++) {
            if (pins[i].pin_number == pin.pin_number) {
                return i;
            }
        }
        return SIZE_MAX;
    };
    
    size_t index = find_in_side(layout.left_pins);
    if (index != SIZE_MAX) {
        return calculate_pin_position(layout, chip_center, pin, index, PinSide::LEFT);
    }
    
    index = find_in_side(layout.right_pins);
    if (index != SIZE_MAX) {
        return calculate_pin_position(layout, chip_center, pin, index, PinSide::RIGHT);
    }
    
    index = find_in_side(layout.top_pins);
    if (index != SIZE_MAX) {
        return calculate_pin_position(layout, chip_center, pin, index, PinSide::TOP);
    }
    
    index = find_in_side(layout.bottom_pins);
    if (index != SIZE_MAX) {
        return calculate_pin_position(layout, chip_center, pin, index, PinSide::BOTTOM);
    }
    
    return chip_center;
}

ImVec2 ChipVisualization::get_recommended_size(const ChipLayout& layout) const {
    const auto& config = get_visual_config();
    float width = (scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f) + 200;
    float height = (scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f) + 100;
    
    if (config.show_chip_markings) {
        height += 60;
    }
    
    return {width, height};
}

void ChipVisualization::render_legend() {
    // Implementation omitted for brevity
}

ImVec2 ChipVisualization::calculate_bga_position(const ChipLayout& layout, ImVec2 chip_center, uint8_t row, uint8_t col) const {
    return chip_center; // Simplified
}

#endif // CERMU_HAS_GUI