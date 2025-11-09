/*
 * chip_visualization.cpp - Generic chip visualization system implementation
 */

#include "chip_visualization.h"
#include "global_chip_style.h"
#include <algorithm>
#include <cstdio>

// Only compile ImGui-dependent code when ImGui is available
#ifdef IMGUI_VERSION

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
// CHIP VISUALIZATION CLASS IMPLEMENTATION
// ============================================================================

ChipVisualization::ChipVisualization(const ChipLayout& layout)
    : layout_(layout) {
}

const ChipVisualConfig& ChipVisualization::get_visual_config() const {
    return GetGlobalChipConfig();
}

void ChipVisualization::render(ImVec2 chip_center, const std::vector<PinSignalState>& pin_states, const char* chip_name) {
    // Draw datasheet grid if enabled (behind everything)
    const auto& config = get_visual_config();
    if (config.show_datasheet_grid) {
        render_datasheet_grid(chip_center);
    }
    
    render_chip_body(chip_center, chip_name);
    render_orientation_marker(chip_center);
    
    if (layout_.package.has_thermal_pad) {
        render_thermal_pad(chip_center);
    }
    
    if (config.show_chip_markings) {
        render_chip_markings(chip_center);
    }
    
    if (config.show_pin_groups) {
        render_pin_groups(chip_center);
    }
    
    // Render based on package type
    if (layout_.package.package_type == PackageType::BGA ||
        layout_.package.package_type == PackageType::LGA) {
        render_bga_grid(chip_center, pin_states);
    } else {
        render_pins(chip_center, pin_states);
    }
    
    // Draw dimension lines if enabled (on top)
    if (config.show_dimension_lines) {
        render_dimension_lines(chip_center);
    }
    
    // Draw pin pitch indicators if enabled
    if (config.show_pin_pitch_indicators) {
        render_pin_pitch_indicators(chip_center);
    }
}

void ChipVisualization::render_chip_body(ImVec2 chip_center, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
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
        std::string package_name_str = get_package_type_string(layout_.package.package_type);
        const char* package_name = package_name_str.c_str();
        ImVec2 package_label_size;
        package_label_size = ImGui::CalcTextSize(package_name);
        ImVec2 package_label_pos = {chip_center.x - package_label_size.x/2, chip_center.y + 15};
        draw_list->AddText(package_label_pos, config.pin_number_color,
                               package_name);
    }
}

void ChipVisualization::render_dip_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // Adjust rendering based on datasheet mode
    switch (config.datasheet_mode) {
        case DatasheetMode::EXTERNAL_LABELING:
            // Early 70s - external labeling, hand-drafted appearance, view-agnostic
            draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 1.5f, 0);
            draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 1.5f, 0, config.chip_border_width);
            
            // No view designation - this was view-agnostic, just a box with pins
            if (chip_name) {
                ImVec2 text_size = ImGui::CalcTextSize(chip_name);
                ImVec2 text_pos = {chip_center.x - text_size.x/2, chip_min.y - 25};
                draw_list->AddText(text_pos, config.text_color, chip_name);
            }
            break;
            
        case DatasheetMode::TOP_VIEW_80S:
            // 80s standard - clean CAD appearance with TOP VIEW designation
            draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 0.0f, 0);
            draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 0.0f, 0, config.chip_border_width);
            
            // Add "TOP VIEW" text in clean 80s style
            if (chip_name) {
                char top_view_text[64];
                snprintf(top_view_text, sizeof(top_view_text), "%s (TOP VIEW)", chip_name);
                ImVec2 text_size = ImGui::CalcTextSize(top_view_text);
                ImVec2 text_pos = {chip_center.x - text_size.x/2, chip_min.y - 20};
                draw_list->AddText(text_pos, config.text_color, top_view_text);
            }
            break;
            
        case DatasheetMode::BOTTOM_VIEW:
            // Rare 80s experiment - mirrored view from solder/pin side
            draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 0.0f, 0);
            draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 0.0f, 0, config.chip_border_width);
            
            // Add "BOTTOM VIEW" warning text
            if (chip_name) {
                char bottom_view_text[64];
                snprintf(bottom_view_text, sizeof(bottom_view_text), "%s (BOTTOM VIEW - MIRRORED)", chip_name);
                ImVec2 text_size = ImGui::CalcTextSize(bottom_view_text);
                ImVec2 text_pos = {chip_center.x - text_size.x/2, chip_min.y - 20};
                draw_list->AddText(text_pos, 0xFF0080FF, bottom_view_text); // Warning color
            }
            break;
            
        case DatasheetMode::FUNCTIONAL_BLOCK: {
            // Internal architecture diagram with functional units
            draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 0.0f, 0);
            draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 0.0f, 0, config.chip_border_width * 1.5f);
            
            // Add functional blocks inside (simplified)
            float block_width = chip_width * 0.3f;
            float block_height = chip_height * 0.2f;
            
            // ALU block
            ImVec2 alu_min = {chip_center.x - block_width/2, chip_center.y - block_height};
            ImVec2 alu_max = {chip_center.x + block_width/2, chip_center.y - block_height/2};
            draw_list->AddRect(alu_min, alu_max, config.pin_border_color, 0.0f, 0, 1.0f);
            draw_list->AddText({alu_min.x + 5, alu_min.y + 5}, config.text_color, "ALU");
            
            // Control block
            ImVec2 ctrl_min = {chip_center.x - block_width/2, chip_center.y + block_height/2};
            ImVec2 ctrl_max = {chip_center.x + block_width/2, chip_center.y + block_height};
            draw_list->AddRect(ctrl_min, ctrl_max, config.pin_border_color, 0.0f, 0, 1.0f);
            draw_list->AddText({ctrl_min.x + 5, ctrl_min.y + 5}, config.text_color, "CTRL");
            
            if (chip_name) {
                ImVec2 text_size = ImGui::CalcTextSize(chip_name);
                ImVec2 text_pos = {chip_center.x - text_size.x/2, chip_min.y - 25};
                draw_list->AddText(text_pos, config.text_color, chip_name);
            }
            break;
        }
            
        case DatasheetMode::CONNECTION_DIAGRAM: {
            // Schematic symbol - triangle or box shape for logic symbols
            ImVec2 tri_p1 = {chip_min.x, chip_center.y};
            ImVec2 tri_p2 = {chip_max.x, chip_min.y + chip_height * 0.3f};
            ImVec2 tri_p3 = {chip_max.x, chip_max.y - chip_height * 0.3f};
            
            draw_list->AddTriangleFilled(tri_p1, tri_p2, tri_p3, config.chip_body_color);
            draw_list->AddTriangle(tri_p1, tri_p2, tri_p3, config.chip_border_color, config.chip_border_width);
            
            if (chip_name) {
                ImVec2 text_size = ImGui::CalcTextSize(chip_name);
                ImVec2 text_pos = {chip_center.x - text_size.x/2, chip_center.y - text_size.y/2};
                draw_list->AddText(text_pos, config.text_color, chip_name);
            }
            break;
        }
            
        case DatasheetMode::PACKAGE_OUTLINE: {
            // 3D-ish mechanical drawing for manufacturing
            draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 2.0f, 0);
            draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 2.0f, 0, config.chip_border_width);
            
            // Add 3D depth effect
            ImVec2 depth_offset = {4.0f, -4.0f};
            ImVec2 depth_min = {chip_min.x + depth_offset.x, chip_min.y + depth_offset.y};
            ImVec2 depth_max = {chip_max.x + depth_offset.x, chip_max.y + depth_offset.y};
            
            // Draw depth lines
            draw_list->AddLine({chip_min.x, chip_min.y}, depth_min, config.pin_border_color, 1.0f);
            draw_list->AddLine({chip_max.x, chip_min.y}, {depth_max.x, depth_min.y}, config.pin_border_color, 1.0f);
            draw_list->AddLine({chip_max.x, chip_max.y}, depth_max, config.pin_border_color, 1.0f);
            draw_list->AddLine({chip_min.x, chip_max.y}, {depth_min.x, depth_max.y}, config.pin_border_color, 1.0f);
            
            // Draw back face
            draw_list->AddRect(depth_min, depth_max, config.pin_border_color, 2.0f, 0, 1.0f);
            
            if (chip_name) {
                char package_text[64];
                snprintf(package_text, sizeof(package_text), "%s PACKAGE OUTLINE", chip_name);
                ImVec2 text_size = ImGui::CalcTextSize(package_text);
                ImVec2 text_pos = {chip_center.x - text_size.x/2, chip_min.y - 25};
                draw_list->AddText(text_pos, config.text_color, package_text);
            }
            break;
        }
            
        default:
            // Modern style - standard rounded rectangle
            draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 2.0f, 0);
            draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 2.0f, 0, config.chip_border_width);
            break;
    }
}

void ChipVisualization::render_surface_mount_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // Surface mount packages are flatter looking
    draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 1.0f, 0);
    draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 1.0f, 0, config.chip_border_width);
}

void ChipVisualization::render_qfp_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // QFP packages are typically square with chamfered corner
    draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 0.0f, 0);
    draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 0.0f, 0, config.chip_border_width);
}

void ChipVisualization::render_bga_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // BGA packages show top view - square with marker
    draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 0.0f, 0);
    draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 0.0f, 0, config.chip_border_width * 1.5f);
}

void ChipVisualization::render_to_style(ImVec2 chip_center, float chip_width, float chip_height, const char* chip_name) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    // TO packages have distinctive shape with heat sink tab
    draw_list->AddRectFilled(chip_min, chip_max, config.chip_body_color, 3.0f, 0);
    draw_list->AddRect(chip_min, chip_max, config.chip_border_color, 3.0f, 0, config.chip_border_width);
    
    // Draw heat sink tab at top
    if (layout_.package.package_type == PackageType::TO220) {
        ImVec2 tab_min = {chip_center.x - chip_width/3, chip_min.y - 15};
        ImVec2 tab_max = {chip_center.x + chip_width/3, chip_min.y};
        draw_list->AddRectFilled(tab_min, tab_max, config.thermal_pad_color, 1.0f, 0);
        draw_list->AddRect(tab_min, tab_max, config.chip_border_color, 1.0f, 0, 1.0f);
    }
}

void ChipVisualization::render_orientation_marker(ImVec2 chip_center) {
    const auto& config = get_visual_config();
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
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                float chip_width = layout_.package.width;
                float chip_height = layout_.package.height;
                ImVec2 marker_pos = {chip_center.x - chip_width/2 + 15, chip_center.y - chip_height/2 + 15};
                draw_list->AddCircle(marker_pos, config.marker_size, config.marker_color, 12, 2.0f);
            }
            break;
        default:
            break;
    }
}

void ChipVisualization::draw_notch(ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    ImVec2 notch_center = {chip_center.x, chip_center.y - chip_height/2};
    draw_list->AddCircleFilled(notch_center, config.marker_size, config.notch_color, 12);
    draw_list->AddCircle(notch_center, config.marker_size, config.chip_border_color, 12, 1.0f);
}

void ChipVisualization::draw_dot_marker(ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    ImVec2 marker_pos = {chip_center.x - chip_width/2 + 15, chip_center.y - chip_height/2 + 15};
    draw_list->AddCircleFilled(marker_pos, config.marker_size * 0.6f, config.marker_color, 12);
}

void ChipVisualization::draw_chamfer(ImVec2 chip_center) {
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

void ChipVisualization::draw_bar_marker(ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    
    ImVec2 bar_min = {chip_center.x - chip_width/3, chip_center.y - chip_height/2 + 5};
    ImVec2 bar_max = {chip_center.x + chip_width/3, chip_center.y - chip_height/2 + 8};
    draw_list->AddRectFilled(bar_min, bar_max, config.marker_color, 0.0f, 0);
}

void ChipVisualization::draw_triangle_marker(ImVec2 chip_center) {
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

void ChipVisualization::render_thermal_pad(ImVec2 chip_center) {
    if (!layout_.package.has_thermal_pad) return;
    
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    float pad_size = layout_.package.thermal_pad_size * std::min(chip_width, chip_height);
    
    ImVec2 pad_min = {chip_center.x - pad_size/2, chip_center.y - pad_size/2};
    ImVec2 pad_max = {chip_center.x + pad_size/2, chip_center.y + pad_size/2};
    
    draw_list->AddRectFilled(pad_min, pad_max, config.thermal_pad_color, 0.0f, 0);
    draw_list->AddRect(pad_min, pad_max, config.pin_border_color, 0.0f, 0, 1.0f);
    
    // Add cross-hatch pattern
    for (int i = -3; i <= 3; i++) {
        float offset = i * pad_size / 8;
        ImVec2 line1_start = {chip_center.x + offset, pad_min.y};
        ImVec2 line1_end = {chip_center.x + offset, pad_max.y};
        draw_list->AddLine(line1_start, line1_end, config.pin_border_color, 0.5f);
        
        ImVec2 line2_start = {pad_min.x, chip_center.y + offset};
        ImVec2 line2_end = {pad_max.x, chip_center.y + offset};
        draw_list->AddLine(line2_start, line2_end, config.pin_border_color, 0.5f);
    }
}

void ChipVisualization::render_chip_markings(ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float y_offset = -40;
    float line_height = 14;
    
    auto draw_marking = [&](const char* text) {
        // Safety check: ensure text is not null and has content
        if (text != nullptr && text[0] != '\0') {
            ImVec2 size;
            size = ImGui::CalcTextSize(text);
            ImVec2 pos = {chip_center.x - size.x/2, chip_center.y + y_offset};
            draw_list->AddText(pos, config.pin_number_color, text);
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

void ChipVisualization::render_pins(ImVec2 chip_center, const std::vector<PinSignalState>& pin_states) {
    render_pin_side(chip_center, layout_.left_pins, pin_states, PinSide::LEFT);
    render_pin_side(chip_center, layout_.right_pins, pin_states, PinSide::RIGHT);
    render_pin_side(chip_center, layout_.top_pins, pin_states, PinSide::TOP);
    render_pin_side(chip_center, layout_.bottom_pins, pin_states, PinSide::BOTTOM);
}

void ChipVisualization::render_bga_grid(ImVec2 chip_center, const std::vector<PinSignalState>& pin_states) {
    const auto& config = get_visual_config();
    // Render BGA ball grid
    // Calculate grid dimensions
    uint8_t max_row = 0, max_col = 0;
    for (const auto& pin : layout_.grid_pins) {
        // Parse pin number to extract row/col
        // BGA pins typically numbered like A1, B2, etc.
    }
    
    // Draw grid of balls
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < layout_.grid_pins.size(); i++) {
        const auto& pin = layout_.grid_pins[i];
        
        // Extract row/col from pin number or use index
        uint8_t row = i / 10; // Simplified - real implementation would parse label
        uint8_t col = i % 10;
        
        ImVec2 ball_pos = calculate_bga_position(chip_center, row, col);
        
        PinSignalState state = {0, false, false, 0, false, false, false, true, 0.0f, false, 0.0f};
        if (pin.pin_number <= pin_states.size()) {
            state = pin_states[pin.pin_number - 1];
        }
        
        uint32_t ball_color = get_pin_type_color(pin.get_pin_type(), config.style);
        if (state.signal_level) {
            ball_color = config.led_active_color;
        }
        
        draw_list->AddCircleFilled(ball_pos, 3.0f, ball_color, 12);
        draw_list->AddCircle(ball_pos, 3.0f, config.pin_border_color, 12, 1.0f);
        
        // Draw label if enabled
        if (config.show_pin_labels) {
            const char* label_str = pin_label_to_string(pin.label);
            ImVec2 label_pos = {ball_pos.x - 8, ball_pos.y - 6};
            draw_list->AddText(label_pos, config.text_color, label_str);
        }
    }
}

void ChipVisualization::render_pin_side(ImVec2 chip_center, const std::vector<ChipPin>& pins, 
                                       const std::vector<PinSignalState>& pin_states, PinSide side) {
    for (size_t i = 0; i < pins.size(); i++) {
        const ChipPin& pin = pins[i];
        
        // Find corresponding pin state
        PinSignalState state = {0, false, false, 0, false, false, false, true, 0.0f, false, 0.0f};
        if (pin.pin_number > 0 && pin.pin_number <= pin_states.size()) {
            state = pin_states[pin.pin_number - 1];
        }
        
        ImVec2 pin_pos = calculate_pin_position(chip_center, pin, i, side);
        render_single_pin(pin_pos, pin, state, side);
    }
}

void ChipVisualization::render_single_pin(ImVec2 pin_pos, const ChipPin& pin, const PinSignalState& state, PinSide side) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Get pin type color
    uint32_t pin_color = get_pin_type_color(pin.get_pin_type(), config.style);
    
    // Modify color based on state
    if (state.high_impedance) {
        pin_color = (pin_color & 0x00FFFFFF) | 0x80000000; // Semi-transparent
    }
    
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
    
    // Draw PWM indicator if enabled and applicable
    if (config.show_pwm_indicators && state.is_pwm) {
         ImVec2 led_pos = get_led_position(pin_pos, side);
         float pwm_angle = state.pwm_duty_cycle * 2.0f * 3.14159f;
        draw_list->AddCircle(led_pos, config.led_radius + 2, 0xFF00FFFF, 12, 2.0f);
    }
    
    // Draw voltage level indicator for analog pins
    if (config.show_voltage_levels && pin.get_pin_type() == PinType::ANALOG && state.signal_valid) {
        ImVec2 led_pos = get_led_position(pin_pos, side);
        char voltage_str[16];
        snprintf(voltage_str, sizeof(voltage_str), "%.2fV", state.analog_voltage);
        ImVec2 voltage_pos = {led_pos.x - 15, led_pos.y + 10};
        draw_list->AddText(voltage_pos, config.text_color, voltage_str);
    }
    
    // Draw pin label if enabled
    if (config.show_pin_labels) {
        ImVec2 label_pos = get_label_position(pin_pos, pin, side);
        uint32_t text_color = state.signal_level ? config.active_text_color : config.text_color;
        
        std::string formatted_label = format_pin_label(pin);
        draw_list->AddText(label_pos, text_color, formatted_label.c_str());
        
        // Show alternate function if enabled
        if (config.show_alternate_functions && pin.alt_function) {
            ImVec2 alt_pos = label_pos;
            alt_pos.y += 12;
            char alt_text[64];
            snprintf(alt_text, sizeof(alt_text), "(%s)", pin.alt_function);
            draw_list->AddText(alt_pos, config.pin_number_color, alt_text);
        }
    }
    
    // Draw pin number if enabled
    if (config.show_pin_numbers) {
        char pin_num_str[4];
        snprintf(pin_num_str, sizeof(pin_num_str), "%d", pin.pin_number);
        
        // Historical datasheet pin number positioning
        bool numbers_inside = config.numbers_inside_package;
        
        ImVec2 pin_num_pos;
        switch (side) {
            case PinSide::LEFT:
                if (numbers_inside) {
                    // 80s style - numbers inside package
                    pin_num_pos = {pin_pos.x + config.pin_width/2 + 2, pin_pos.y - 6};
                } else {
                    // 70s style - numbers outside package, closer to pin
                    pin_num_pos = {pin_pos.x - config.pin_width/2 - 15, pin_pos.y - 6};
                }
                break;
            case PinSide::RIGHT:
                if (numbers_inside) {
                    // 80s style - numbers inside package
                    pin_num_pos = {pin_pos.x - config.pin_width/2 - 10, pin_pos.y - 6};
                } else {
                    // 70s style - numbers outside package
                    pin_num_pos = {pin_pos.x + config.pin_width/2 + 2, pin_pos.y - 6};
                }
                break;
            case PinSide::TOP:
                if (numbers_inside) {
                    // 80s style - numbers inside package
                    pin_num_pos = {pin_pos.x - 6, pin_pos.y + config.pin_height/2 + 2};
                } else {
                    // 70s style - numbers outside package
                    pin_num_pos = {pin_pos.x - 6, pin_pos.y - config.pin_height/2 - 15};
                }
                break;
            case PinSide::BOTTOM:
                if (numbers_inside) {
                    // 80s style - numbers inside package
                    pin_num_pos = {pin_pos.x - 6, pin_pos.y - config.pin_height/2 - 10};
                } else {
                    // 70s style - numbers outside package
                    pin_num_pos = {pin_pos.x - 6, pin_pos.y + config.pin_height/2 + 2};
                }
                break;
        }
        
        draw_list->AddText(pin_num_pos, config.pin_number_color, pin_num_str);
    }
    
    // Draw differential pair indicator
    if (pin.is_differential_pos || pin.is_differential_neg) {
        ImVec2 diff_pos = pin_pos;
        diff_pos.x += config.pin_width/2 + 1;
        const char* diff_marker = pin.is_differential_pos ? "+" : "-";
        draw_list->AddText(diff_pos, config.text_color, diff_marker);
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
    
    if (config.use_compact_layout) {
        offset *= 0.7f;
    }
    
    // Historical datasheet positioning
    bool labels_inside = config.labels_inside_package;
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    
    switch (side) {
        case PinSide::LEFT:
            if (labels_inside) {
                // 80s style - labels inside package (TOP VIEW)
                label_pos.x += config.pin_width/2 + 8;
                label_pos.y -= 6;
            } else {
                // 70s style - labels outside package
                label_pos.x -= config.pin_width/2 + offset + 30;
                label_pos.y -= 6;
            }
            break;
        case PinSide::RIGHT:
            if (labels_inside) {
                // 80s style - labels inside package
                label_pos.x -= config.pin_width/2 - 8;
                label_pos.y -= 6;
                // Right-align text for inside labels
            } else {
                // 70s style - labels outside package
                label_pos.x += config.pin_width/2 + offset;
                label_pos.y -= 6;
            }
            break;
        case PinSide::TOP:
            if (labels_inside) {
                // 80s style - labels inside package
                label_pos.y += config.pin_height/2 + 2;
                label_pos.x -= 10;
            } else {
                // 70s style - labels outside package
                label_pos.y -= config.pin_height/2 + offset + 8;
                label_pos.x -= 10;
            }
            break;
        case PinSide::BOTTOM:
            if (labels_inside) {
                // 80s style - labels inside package
                label_pos.y -= config.pin_height/2 - 2;
                label_pos.x -= 10;
            } else {
                // 70s style - labels outside package
                label_pos.y += config.pin_height/2 + offset;
                label_pos.x -= 10;
            }
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
        case PinNotationStyle::OVERLINE:
            // Unicode overline combining character
            formatted = std::string(label_str) + "\u0305";
            break;
        case PinNotationStyle::TILDE_PREFIX:
            formatted = "~" + std::string(label_str);
            break;
        case PinNotationStyle::HASH_SUFFIX:
            formatted = std::string(label_str) + "#";
            break;
        case PinNotationStyle::ASTERISK_SUFFIX:
            formatted = std::string(label_str) + "*";
            break;
        case PinNotationStyle::N_SUFFIX:
            formatted = std::string(label_str) + "_N";
            break;
        case PinNotationStyle::BAR_SUFFIX:
            formatted = std::string(label_str) + "_BAR";
            break;
        default:
            formatted = "/" + std::string(label_str);
            break;
    }
    
    return formatted;
}

ImVec2 ChipVisualization::get_recommended_size() const {
    const auto& config = get_visual_config();
    float width = (scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f) + 200; // Extra space for labels
    float height = (scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f) + 100;
    
    // Add extra space for chip markings
    if (config.show_chip_markings) {
        height += 60;
    }
    
    return {width, height};
}

void ChipVisualization::render_legend() {
    const auto& config = get_visual_config();
    if (ImGui::CollapsingHeader("Pin Type Legend", NULL, 0)) {
        ImGui::Indent(16.0f);
        
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
            uint32_t color = get_pin_type_color(entry.type, config.style);
            
            // Draw colored square
            ImVec2 cursor;
            cursor = ImGui::GetCursorScreenPos();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 square_max = {cursor.x + 12, cursor.y + 12};
            draw_list->AddRectFilled(cursor, square_max, color, 0.0f, 0);
            draw_list->AddRect(cursor, square_max, config.pin_border_color, 0.0f, 0, 1.0f);
            
            ImVec2 legend_dummy = {16, 12};
            ImGui::Dummy(legend_dummy);
            ImGui::SameLine(0, 4);
            ImGui::Text("%s: %s", entry.name, entry.description);
        }
        
        ImGui::Separator();
        ImGui::Text("Pin Notation Styles:");
        
        const char* notation_examples[] = {
            "/ prefix: /IRQ, /CS, /WE",
            "~ prefix: ~IRQ, ~CS, ~WE",
            "# suffix: IRQ#, CS#, WE#",
            "* suffix: IRQ*, CS*, WE*",
            "_N suffix: IRQ_N, CS_N, WE_N",
            "_BAR suffix: IRQ_BAR, CS_BAR"
        };
        
        for (const char* example : notation_examples) {
            ImGui::BulletText("%s", example);
        }
        
        ImGui::Separator();
        ImGui::Text("Orientation Markers:");
        ImGui::BulletText("Notch: U-shaped cutout at top center");
        ImGui::BulletText("Dot: Physical dimple near pin 1");
        ImGui::BulletText("Chamfer: Beveled corner near pin 1");
        ImGui::BulletText("Bar: Stripe indicating pin 1 side");
        ImGui::BulletText("Triangle: Pointer to pin 1");
        
        ImGui::Unindent(16.0f);
    }
    
    // Package type information
    if (ImGui::CollapsingHeader("Package Information", NULL, 0)) {
        ImGui::Indent(16.0f);
        
        std::string package_name_str = get_package_type_string(layout_.package.package_type);
        ImGui::Text("Package: %s", package_name_str.c_str());
        
        // Calculate total pins
        int total_pins = layout_.left_pins.size() + layout_.right_pins.size() + 
                        layout_.top_pins.size() + layout_.bottom_pins.size() + 
                        layout_.grid_pins.size();
        ImGui::Text("Total Pins: %d", total_pins);
        ImGui::Text("Pin Pitch: %.2f mm", layout_.package.pin_pitch);
        
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
        ImGui::Text("Type: %s", package_type_name);
        
        if (layout_.package.has_thermal_pad) {
            ImGui::Text("Thermal Pad: Yes (%.0f%%)", layout_.package.thermal_pad_size * 100);
        }
        
        ImGui::Unindent(16.0f);
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
            const char* pin_label_str = pin_label_to_string(pin.label);
            if (strcmp(pin_label_str, label) == 0) {
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
            if (pin.get_group_name() && strcmp(pin.get_group_name(), group_name) == 0) {
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
    // This method is deprecated - settings are now global
    // Display a message directing users to the global configuration
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Settings moved to global configuration!");
    ImGui::Text("Use the 'Chip Visualization Style' menu to configure settings.");
    ImGui::Text("All chip visualizations now share the same global style.");
}

// ============================================================================
// DATASHEET-SPECIFIC RENDERING FUNCTIONS
// ============================================================================

void ChipVisualization::render_datasheet_grid(ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    
    // Draw light grid lines for alignment (70s style)
    float grid_spacing = 10.0f;
    float grid_extend = 50.0f;
    
    ImVec2 grid_min = {chip_center.x - chip_width/2 - grid_extend, chip_center.y - chip_height/2 - grid_extend};
    ImVec2 grid_max = {chip_center.x + chip_width/2 + grid_extend, chip_center.y + chip_height/2 + grid_extend};
    
    // Vertical lines
    for (float x = grid_min.x; x <= grid_max.x; x += grid_spacing) {
        draw_list->AddLine({x, grid_min.y}, {x, grid_max.y}, config.datasheet_grid_color, 0.5f);
    }
    
    // Horizontal lines
    for (float y = grid_min.y; y <= grid_max.y; y += grid_spacing) {
        draw_list->AddLine({grid_min.x, y}, {grid_max.x, y}, config.datasheet_grid_color, 0.5f);
    }
}

void ChipVisualization::render_dimension_lines(ImVec2 chip_center) {
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    
    // Package width dimension line (below chip)
    float dim_offset = 30.0f;
    ImVec2 dim_start = {chip_center.x - chip_width/2, chip_center.y + chip_height/2 + dim_offset};
    ImVec2 dim_end = {chip_center.x + chip_width/2, chip_center.y + chip_height/2 + dim_offset};
    
    // Draw dimension line
    draw_list->AddLine(dim_start, dim_end, config.dimension_line_color, 1.0f);
    
    // Draw end arrows
    float arrow_size = 4.0f;
    ImVec2 arrow1_p1 = {dim_start.x, dim_start.y - arrow_size};
    ImVec2 arrow1_p2 = {dim_start.x, dim_start.y + arrow_size};
    ImVec2 arrow2_p1 = {dim_end.x, dim_end.y - arrow_size};
    ImVec2 arrow2_p2 = {dim_end.x, dim_end.y + arrow_size};
    
    draw_list->AddLine(arrow1_p1, arrow1_p2, config.dimension_line_color, 1.0f);
    draw_list->AddLine(arrow2_p1, arrow2_p2, config.dimension_line_color, 1.0f);
    
    // Add dimension text
    char dim_text[32];
    snprintf(dim_text, sizeof(dim_text), "%.1f mm", layout_.package.width / 1000.0f);
    ImVec2 text_size = ImGui::CalcTextSize(dim_text);
    ImVec2 text_pos = {chip_center.x - text_size.x/2, dim_start.y + 5};
    draw_list->AddText(text_pos, config.dimension_line_color, dim_text);
    
    // Package height dimension line (to the right of chip)
    ImVec2 dim_start_h = {chip_center.x + chip_width/2 + dim_offset, chip_center.y - chip_height/2};
    ImVec2 dim_end_h = {chip_center.x + chip_width/2 + dim_offset, chip_center.y + chip_height/2};
    
    draw_list->AddLine(dim_start_h, dim_end_h, config.dimension_line_color, 1.0f);
    
    // Height arrows
    ImVec2 arrow3_p1 = {dim_start_h.x - arrow_size, dim_start_h.y};
    ImVec2 arrow3_p2 = {dim_start_h.x + arrow_size, dim_start_h.y};
    ImVec2 arrow4_p1 = {dim_end_h.x - arrow_size, dim_end_h.y};
    ImVec2 arrow4_p2 = {dim_end_h.x + arrow_size, dim_end_h.y};
    
    draw_list->AddLine(arrow3_p1, arrow3_p2, config.dimension_line_color, 1.0f);
    draw_list->AddLine(arrow4_p1, arrow4_p2, config.dimension_line_color, 1.0f);
    
    // Height dimension text (rotated would be ideal, but simplified for now)
    snprintf(dim_text, sizeof(dim_text), "%.1f", layout_.package.height / 1000.0f);
    ImVec2 height_text_pos = {dim_start_h.x + 8, chip_center.y - 6};
    draw_list->AddText(height_text_pos, config.dimension_line_color, dim_text);
}

void ChipVisualization::render_pin_pitch_indicators(ImVec2 chip_center) {
    if (layout_.left_pins.size() < 2) return;  // Need at least 2 pins to show pitch
    
    const auto& config = get_visual_config();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float chip_width = scaled_chip_width_ > 0 ? scaled_chip_width_ : 120.0f;
    float chip_height = scaled_chip_height_ > 0 ? scaled_chip_height_ : 80.0f;
    
    // Calculate pin positions for first two pins on left side
    ImVec2 pin1_pos = calculate_pin_position(chip_center, layout_.left_pins[0], 0, PinSide::LEFT);
    ImVec2 pin2_pos = calculate_pin_position(chip_center, layout_.left_pins[1], 1, PinSide::LEFT);
    
    // Draw pin pitch dimension line (to the left of chip)
    float pitch_offset = 40.0f;
    ImVec2 pitch_start = {chip_center.x - chip_width/2 - pitch_offset, pin1_pos.y};
    ImVec2 pitch_end = {chip_center.x - chip_width/2 - pitch_offset, pin2_pos.y};
    
    draw_list->AddLine(pitch_start, pitch_end, config.dimension_line_color, 1.0f);
    
    // Add small tick marks
    float tick_size = 3.0f;
    draw_list->AddLine({pitch_start.x - tick_size, pitch_start.y},
                      {pitch_start.x + tick_size, pitch_start.y}, config.dimension_line_color, 1.0f);
    draw_list->AddLine({pitch_end.x - tick_size, pitch_end.y},
                      {pitch_end.x + tick_size, pitch_end.y}, config.dimension_line_color, 1.0f);
    
    // Add pitch measurement text
    char pitch_text[16];
    snprintf(pitch_text, sizeof(pitch_text), "%.1f", layout_.package.pin_pitch);
    ImVec2 pitch_text_pos = {pitch_start.x - 25, (pitch_start.y + pitch_end.y) / 2 - 6};
    draw_list->AddText(pitch_text_pos, config.dimension_line_color, pitch_text);
}

#endif // IMGUI_VERSION