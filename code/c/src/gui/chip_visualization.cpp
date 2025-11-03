/*
 * chip_visualization.cpp - Generic chip visualization system implementation
 */

#include "chip_visualization.h"
#include <algorithm>
#include <cstdio>

// ============================================================================
// COLOR DEFINITIONS AND VISUAL CONFIGURATION
// ============================================================================

uint32_t get_pin_type_color(PinType type) {
    // Colors in ABGR format for ImGui
    switch (type) {
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

ChipVisualConfig ChipVisualConfig::get_default() {
    return {
        .chip_body_color = 0xFF1A1A1A,      // Darker chip body for better contrast
        .chip_border_color = 0xFFCCCCCC,    // Light border
        .pin_border_color = 0xFFCCCCCC,     // Light pin border
        .text_color = 0xFFFFFFFF,           // White text
        .active_text_color = 0xFF00FF00,    // Green text when active
        .pin_number_color = 0xFFE0E0E0,     // Brighter pin numbers
        .led_active_color = 0xFF00FF00,     // Green LED when active
        .led_inactive_color = 0xFF004000,   // Darker green LED when inactive
        .notch_color = 0xFF606060,          // Lighter notch
        
        .pin_width = 10.0f,                 // Wider pins for better visibility
        .pin_height = 16.0f,                // Taller pins
        .led_size = 3.0f,                   // LED size diameter in pixels
        .label_offset = 18.0f,              // More space for labels
        .pin_spacing_factor = 1.2f,
        .chip_border_width = 2.0f,
        .pin_border_width = 1.5f,           // Thicker pin borders
        
        .show_pin_numbers = true,
        .show_pin_labels = true,
        .show_led_indicators = true,
        .show_package_name = true
    };
}

// ============================================================================
// CHIP VISUALIZATION CLASS IMPLEMENTATION
// ============================================================================

ChipVisualization::ChipVisualization(const PinLayout& layout, const ChipVisualConfig& config)
    : layout_(layout), config_(config) {
}

void ChipVisualization::render(ImVec2 chip_center, const std::vector<PinState>& pin_states, const char* chip_name) {
    render_chip_body(chip_center, chip_name);
    render_pins(chip_center, pin_states);
}

void ChipVisualization::render_chip_body(ImVec2 chip_center, const char* chip_name) {
    ImDrawList* draw_list = igGetWindowDrawList();
    
    float chip_width = layout_.package.width;
    float chip_height = layout_.package.height;
    
    // Draw chip body rectangle
    ImVec2 chip_min = {chip_center.x - chip_width/2, chip_center.y - chip_height/2};
    ImVec2 chip_max = {chip_center.x + chip_width/2, chip_center.y + chip_height/2};
    
    ImDrawList_AddRectFilled(draw_list, chip_min, chip_max, config_.chip_body_color, 0.0f, 0);
    ImDrawList_AddRect(draw_list, chip_min, chip_max, config_.chip_border_color, 0.0f, 0, config_.chip_border_width);
    
    // Draw pin 1 notch if package has one
    if (layout_.package.has_notch) {
        float notch_size = 8.0f;
        ImVec2 notch_center = {chip_center.x, chip_min.y};
        ImDrawList_AddCircleFilled(draw_list, notch_center, notch_size, config_.notch_color, 12);
        ImDrawList_AddCircle(draw_list, notch_center, notch_size, config_.chip_border_color, 12, 1.0f);
    }
    
    // Draw chip name if provided
    if (chip_name) {
        ImVec2 label_size;
        igCalcTextSize(&label_size, chip_name, NULL, false, -1.0f);
        ImVec2 label_pos = {chip_center.x - label_size.x/2, chip_center.y - 10};
        ImDrawList_AddText_Vec2(draw_list, label_pos, config_.text_color, chip_name, NULL);
    }
    
    // Calculate total pins and individual side counts for debugging
    size_t total_pins = layout_.left_pins.size() + layout_.right_pins.size() + 
                       layout_.top_pins.size() + layout_.bottom_pins.size();
    
    // Draw vendor and chip information if enabled
    if (config_.show_package_name) {
        // Draw vendor name
        if (layout_.package.vendor) {
            ImVec2 vendor_label_size;
            igCalcTextSize(&vendor_label_size, layout_.package.vendor, NULL, false, -1.0f);
            ImVec2 vendor_label_pos = {chip_center.x - vendor_label_size.x/2, chip_center.y + 15};
            ImDrawList_AddText_Vec2(draw_list, vendor_label_pos, config_.pin_number_color, layout_.package.vendor, NULL);
        }
        
        // Draw chip ID below vendor
        if (layout_.package.chip_id) {
            ImVec2 chip_id_size;
            igCalcTextSize(&chip_id_size, layout_.package.chip_id, NULL, false, -1.0f);
            ImVec2 chip_id_pos = {chip_center.x - chip_id_size.x/2, chip_center.y + 30};
            ImDrawList_AddText_Vec2(draw_list, chip_id_pos, config_.text_color, layout_.package.chip_id, NULL);
        }
        
        // Package name has been removed - could derive "DIP-40" from pin count if needed
    }
    
    // Draw debug pin count information
    if (total_pins > 0) {
        char pin_count_text[128];
        snprintf(pin_count_text, sizeof(pin_count_text), 
                 "Pins: %zu total (L:%zu R:%zu T:%zu B:%zu)", 
                 total_pins, layout_.left_pins.size(), layout_.right_pins.size(),
                 layout_.top_pins.size(), layout_.bottom_pins.size());
        
        ImVec2 pin_count_size;
        igCalcTextSize(&pin_count_size, pin_count_text, NULL, false, -1.0f);
        ImVec2 pin_count_pos = {chip_center.x - pin_count_size.x/2, chip_center.y + 65};
        ImDrawList_AddText_Vec2(draw_list, pin_count_pos, 0xFF808080, pin_count_text, NULL); // Gray text
    }
}

void ChipVisualization::render_pins(ImVec2 chip_center, const std::vector<PinState>& pin_states) {
    // Debug info - uncomment to debug pin rendering issues
    // printf("Rendering pins: Left=%zu, Right=%zu, Top=%zu, Bottom=%zu, States=%zu\n", 
    //        layout_.left_pins.size(), layout_.right_pins.size(), 
    //        layout_.top_pins.size(), layout_.bottom_pins.size(), pin_states.size());
    
    // Render each side
    if (!layout_.left_pins.empty()) {
        render_pin_side(chip_center, layout_.left_pins, pin_states, PinSide::LEFT);
    }
    if (!layout_.right_pins.empty()) {
        render_pin_side(chip_center, layout_.right_pins, pin_states, PinSide::RIGHT);
    }
    if (!layout_.top_pins.empty()) {
        render_pin_side(chip_center, layout_.top_pins, pin_states, PinSide::TOP);
    }
    if (!layout_.bottom_pins.empty()) {
        render_pin_side(chip_center, layout_.bottom_pins, pin_states, PinSide::BOTTOM);
    }
}

void ChipVisualization::render_pin_side(ImVec2 chip_center, const std::vector<ChipPin>& pins, 
                                       const std::vector<PinState>& pin_states, PinSide side) {
    for (size_t i = 0; i < pins.size(); i++) {
        const ChipPin& pin = pins[i];
        
        // Find corresponding pin state (match by pin number)
        PinState state = {false, false, 0, false, true}; // Default state
        
        // Look for matching pin state by pin number (1-indexed)
        if (pin.pin_number > 0 && pin.pin_number <= pin_states.size()) {
            state = pin_states[pin.pin_number - 1];
        }
        
        ImVec2 pin_pos = calculate_pin_position(chip_center, pin, i, side);
        render_single_pin(pin_pos, pin, state, side);
    }
}

void ChipVisualization::render_single_pin(ImVec2 pin_pos, const ChipPin& pin, const PinState& state, PinSide side) {
    ImDrawList* draw_list = igGetWindowDrawList();
    if (!draw_list) return;
    
    // Debug: Force visible pins for testing
    // printf("Drawing pin %d (%s) at (%.1f, %.1f)\n", pin.pin_number, pin.label, pin_pos.x, pin_pos.y);
    
    // Get pin type color
    uint32_t pin_color = get_pin_type_color(pin.type);
    
    // Simple pin rectangle - centered on pin position
    ImVec2 pin_min = {pin_pos.x - config_.pin_width/2, pin_pos.y - config_.pin_height/2};
    ImVec2 pin_max = {pin_pos.x + config_.pin_width/2, pin_pos.y + config_.pin_height/2};
    
    // Draw pin rectangle with stronger colors for visibility
    ImDrawList_AddRectFilled(draw_list, pin_min, pin_max, pin_color, 0.0f, 0);
    ImDrawList_AddRect(draw_list, pin_min, pin_max, config_.pin_border_color, 0.0f, 0, config_.pin_border_width);
    
    // Draw LED indicator if enabled - make it more prominent like in the reference
    if (config_.show_led_indicators) {
        ImVec2 led_pos = get_led_position(pin_pos, side);
        uint32_t led_color = state.is_active ? config_.led_active_color : config_.led_inactive_color;
        
        // Draw LED as a small square instead of circle
        float led_size = config_.led_size;
        ImVec2 led_min = {led_pos.x - led_size, led_pos.y - led_size};
        ImVec2 led_max = {led_pos.x + led_size, led_pos.y + led_size};
        
        ImDrawList_AddRectFilled(draw_list, led_min, led_max, led_color, 0.0f, 0);
        ImDrawList_AddRect(draw_list, led_min, led_max, config_.pin_border_color, 0.0f, 0, 1.5f);
    }
    
    // Draw pin label if enabled
    if (config_.show_pin_labels && pin.label && strlen(pin.label) > 0) {
        ImVec2 label_pos = get_label_position(pin_pos, pin, side);
        uint32_t text_color = state.is_active ? config_.active_text_color : config_.text_color;
        
        // Format label with invert prefix if needed
        char formatted_label[32];
        if (pin.invert_logic) {
            snprintf(formatted_label, sizeof(formatted_label), "/%s", pin.label);
        } else {
            strncpy(formatted_label, pin.label, sizeof(formatted_label) - 1);
            formatted_label[sizeof(formatted_label) - 1] = '\0';
        }
        
        ImDrawList_AddText_Vec2(draw_list, label_pos, text_color, formatted_label, NULL);
    }
    
    // Draw pin number if enabled
    if (config_.show_pin_numbers && pin.pin_number > 0) {
        char pin_num_str[4];
        snprintf(pin_num_str, sizeof(pin_num_str), "%d", pin.pin_number);
        
        ImVec2 pin_num_pos = {0.0f, 0.0f};  // Initialize to suppress warning
        switch (side) {
            case PinSide::LEFT:
                pin_num_pos = {pin_pos.x + config_.pin_width/2 + 2, pin_pos.y - 6};
                break;
            case PinSide::RIGHT:
                pin_num_pos = {pin_pos.x - config_.pin_width/2 - 12, pin_pos.y - 6};
                break;
            case PinSide::TOP:
                pin_num_pos = {pin_pos.x - 6, pin_pos.y + config_.pin_height/2 + 2};
                break;
            case PinSide::BOTTOM:
                pin_num_pos = {pin_pos.x - 6, pin_pos.y - config_.pin_height/2 - 12};
                break;
        }
        
        ImDrawList_AddText_Vec2(draw_list, pin_num_pos, config_.pin_number_color, pin_num_str, NULL);
    }
}

ImVec2 ChipVisualization::calculate_pin_position(ImVec2 chip_center, const ChipPin& pin, size_t index_in_side, PinSide side) const {
    float chip_width = layout_.package.width;
    float chip_height = layout_.package.height;
    
    ImVec2 pin_pos = chip_center;
    
    switch (side) {
        case PinSide::LEFT: {
            pin_pos.x = chip_center.x - chip_width/2; // Left edge of chip
            size_t pin_count = layout_.left_pins.size();
            if (pin_count > 1) {
                // Distribute pins evenly along the left side with some margin
                float start_y = chip_center.y - chip_height/2 + 20; // 20px margin from top
                float end_y = chip_center.y + chip_height/2 - 20;   // 20px margin from bottom
                float total_spacing = end_y - start_y;
                pin_pos.y = start_y + (total_spacing / (pin_count - 1)) * index_in_side;
            } else {
                pin_pos.y = chip_center.y;
            }
            break;
        }
        case PinSide::RIGHT: {
            pin_pos.x = chip_center.x + chip_width/2; // Right edge of chip
            size_t pin_count = layout_.right_pins.size();
            if (pin_count > 1) {
                float start_y = chip_center.y - chip_height/2 + 20;
                float end_y = chip_center.y + chip_height/2 - 20;
                float total_spacing = end_y - start_y;
                pin_pos.y = start_y + (total_spacing / (pin_count - 1)) * index_in_side;
            } else {
                pin_pos.y = chip_center.y;
            }
            break;
        }
        case PinSide::TOP: {
            pin_pos.y = chip_center.y - chip_height/2; // Top edge of chip
            size_t pin_count = layout_.top_pins.size();
            if (pin_count > 1) {
                float start_x = chip_center.x - chip_width/2 + 20; // 20px margin from left
                float end_x = chip_center.x + chip_width/2 - 20;   // 20px margin from right
                float total_spacing = end_x - start_x;
                pin_pos.x = start_x + (total_spacing / (pin_count - 1)) * index_in_side;
            } else {
                pin_pos.x = chip_center.x;
            }
            break;
        }
        case PinSide::BOTTOM: {
            pin_pos.y = chip_center.y + chip_height/2; // Bottom edge of chip
            size_t pin_count = layout_.bottom_pins.size();
            if (pin_count > 1) {
                float start_x = chip_center.x - chip_width/2 + 20;
                float end_x = chip_center.x + chip_width/2 - 20;
                float total_spacing = end_x - start_x;
                pin_pos.x = start_x + (total_spacing / (pin_count - 1)) * index_in_side;
            } else {
                pin_pos.x = chip_center.x;
            }
            break;
        }
    }
    
    return pin_pos;
}

ImVec2 ChipVisualization::get_led_position(ImVec2 pin_pos, PinSide side) const {
    ImVec2 led_pos = pin_pos;
    float offset = 15.0f; // Simple offset from pin center
    
    switch (side) {
        case PinSide::LEFT:
            led_pos.x -= offset;
            break;
        case PinSide::RIGHT:
            led_pos.x += offset;
            break;
        case PinSide::TOP:
            led_pos.y -= offset;
            break;
        case PinSide::BOTTOM:
            led_pos.y += offset;
            break;
    }
    
    return led_pos;
}

ImVec2 ChipVisualization::get_label_position(ImVec2 pin_pos, const ChipPin& pin, PinSide side) const {
    ImVec2 label_pos = pin_pos;
    float offset = 25.0f; // Simple offset for labels
    
    switch (side) {
        case PinSide::LEFT:
            label_pos.x -= offset;
            label_pos.y -= 6;
            break;
        case PinSide::RIGHT:
            label_pos.x += offset;
            label_pos.y -= 6;
            break;
        case PinSide::TOP:
            label_pos.y -= offset;
            label_pos.x -= 10; // Center approximately on pin
            break;
        case PinSide::BOTTOM:
            label_pos.y += offset;
            label_pos.x -= 10; // Center approximately on pin
            break;
    }
    
    return label_pos;
}

void ChipVisualization::render_legend() {
    if (igCollapsingHeader_BoolPtr("Pin Legend", NULL, 0)) {
        igIndent(16.0f);
        
        struct LegendEntry {
            PinType type;
            const char* name;
            const char* description;
        };
        
        static const LegendEntry legend[] = {
            {PinType::POWER,     "Power",     "VCC, VSS (power supply)"},
            {PinType::CLOCK,     "Clock",     "Clock signals (φ0, φ1, φ2)"},
            {PinType::ADDRESS,   "Address",   "Address bus (A0-A23)"},
            {PinType::DATA,      "Data",      "Data bus (D0-D15)"},
            {PinType::CONTROL,   "Control",   "Control signals (RW, SYNC, etc.)"},
            {PinType::INTERRUPT, "Interrupt", "Interrupt lines (IRQ, NMI, etc.)"},
            {PinType::SPECIAL,   "Special",   "Special purpose signals"},
            {PinType::IO_PORT,   "I/O Port",  "I/O port lines"}
        };
        
        for (const auto& entry : legend) {
            uint32_t color = get_pin_type_color(entry.type);
            
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
        igText("Pin Notation:");
        igBulletText("/ prefix indicates active-low signal");
        igBulletText("Example: /IRQ = active-low interrupt");
        
        igUnindent(16.0f);
    }
}

const ChipPin* ChipVisualization::find_pin_by_number(uint8_t pin_number) const {
    // Search all sides
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
    
    return nullptr;
}

ImVec2 ChipVisualization::get_pin_position(ImVec2 chip_center, const ChipPin& pin) const {
    // Find the pin in the appropriate side and get its index
    auto find_in_side = [&pin](const std::vector<ChipPin>& pins) -> size_t {
        for (size_t i = 0; i < pins.size(); i++) {
            if (pins[i].pin_number == pin.pin_number) {
                return i;
            }
        }
        return SIZE_MAX; // Not found
    };
    
    // Check each side - derive PinSide from which vector contains the pin
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
    
    return chip_center; // Fallback
}