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
        .chip_body_color = 0xFF2D2D30,      // Dark gray chip body
        .chip_border_color = 0xFF808080,    // Light gray border
        .pin_border_color = 0xFF000000,     // Black pin border
        .text_color = 0xFFFFFFFF,           // White text
        .active_text_color = 0xFF00FF00,    // Green text when active
        .pin_number_color = 0xFFA0A0A0,     // Light gray pin numbers
        .led_active_color = 0xFF00FF00,     // Green LED when active
        .led_inactive_color = 0xFF404040,   // Dark gray LED when inactive
        .notch_color = 0xFF404040,          // Dark gray notch
        
        .pin_width = 8.0f,
        .pin_height = 12.0f,
        .led_radius = 3.0f,
        .label_offset = 15.0f,
        .pin_spacing_factor = 1.2f,
        .chip_border_width = 2.0f,
        .pin_border_width = 1.0f,
        
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
    
    // Draw package name if enabled
    if (config_.show_package_name && layout_.package.package_name) {
        ImVec2 package_label_size;
        igCalcTextSize(&package_label_size, layout_.package.package_name, NULL, false, -1.0f);
        ImVec2 package_label_pos = {chip_center.x - package_label_size.x/2, chip_center.y + 15};
        ImDrawList_AddText_Vec2(draw_list, package_label_pos, config_.pin_number_color, layout_.package.package_name, NULL);
    }
}

void ChipVisualization::render_pins(ImVec2 chip_center, const std::vector<PinState>& pin_states) {
    // Render each side
    render_pin_side(chip_center, layout_.left_pins, pin_states, PinSide::LEFT);
    render_pin_side(chip_center, layout_.right_pins, pin_states, PinSide::RIGHT);
    render_pin_side(chip_center, layout_.top_pins, pin_states, PinSide::TOP);
    render_pin_side(chip_center, layout_.bottom_pins, pin_states, PinSide::BOTTOM);
}

void ChipVisualization::render_pin_side(ImVec2 chip_center, const std::vector<ChipPin>& pins, 
                                       const std::vector<PinState>& pin_states, PinSide side) {
    for (size_t i = 0; i < pins.size(); i++) {
        const ChipPin& pin = pins[i];
        
        // Find corresponding pin state (match by pin number)
        PinState state = {false, false, 0, false};
        for (const auto& ps : pin_states) {
            // This is a simple approach - in practice you'd want a better mapping system
            if (pin.pin_number <= pin_states.size()) {
                state = pin_states[pin.pin_number - 1];
                break;
            }
        }
        
        ImVec2 pin_pos = calculate_pin_position(chip_center, pin, i, side);
        render_single_pin(pin_pos, pin, state, side);
    }
}

void ChipVisualization::render_single_pin(ImVec2 pin_pos, const ChipPin& pin, const PinState& state, PinSide side) {
    ImDrawList* draw_list = igGetWindowDrawList();
    
    // Get pin type color
    uint32_t pin_color = get_pin_type_color(pin.type);
    
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
    
    // Draw pin label if enabled
    if (config_.show_pin_labels) {
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
}

ImVec2 ChipVisualization::calculate_pin_position(ImVec2 chip_center, const ChipPin& pin, size_t index_in_side, PinSide side) const {
    float chip_width = layout_.package.width;
    float chip_height = layout_.package.height;
    
    ImVec2 pin_pos = chip_center;
    
    switch (side) {
        case PinSide::LEFT: {
            pin_pos.x = chip_center.x - chip_width/2;
            float total_height = chip_height - 40; // Leave margin for notch
            float pin_spacing = total_height / std::max(1.0f, float(layout_.left_pins.size() - 1));
            pin_pos.y = chip_center.y - total_height/2 + index_in_side * pin_spacing;
            break;
        }
        case PinSide::RIGHT: {
            pin_pos.x = chip_center.x + chip_width/2;
            float total_height = chip_height - 40;
            float pin_spacing = total_height / std::max(1.0f, float(layout_.right_pins.size() - 1));
            pin_pos.y = chip_center.y - total_height/2 + index_in_side * pin_spacing;
            break;
        }
        case PinSide::TOP: {
            pin_pos.y = chip_center.y - chip_height/2;
            float total_width = chip_width - 40; // Leave margin
            float pin_spacing = total_width / std::max(1.0f, float(layout_.top_pins.size() - 1));
            pin_pos.x = chip_center.x - total_width/2 + index_in_side * pin_spacing;
            break;
        }
        case PinSide::BOTTOM: {
            pin_pos.y = chip_center.y + chip_height/2;
            float total_width = chip_width - 40;
            float pin_spacing = total_width / std::max(1.0f, float(layout_.bottom_pins.size() - 1));
            pin_pos.x = chip_center.x - total_width/2 + index_in_side * pin_spacing;
            break;
        }
    }
    
    return pin_pos;
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
    
    switch (side) {
        case PinSide::LEFT:
            label_pos.x -= config_.pin_width/2 + config_.label_offset + 30;
            label_pos.y -= 6;
            break;
        case PinSide::RIGHT:
            label_pos.x += config_.pin_width/2 + config_.label_offset;
            label_pos.y -= 6;
            break;
        case PinSide::TOP:
            label_pos.y -= config_.pin_height/2 + config_.label_offset + 8;
            label_pos.x -= 10; // Center approximately on pin
            break;
        case PinSide::BOTTOM:
            label_pos.y += config_.pin_height/2 + config_.label_offset;
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

// ============================================================================
// HELPER FUNCTIONS FOR STANDARD PACKAGE LAYOUTS
// ============================================================================

PinLayout create_dip40_layout() {
    PinLayout layout;
    
    layout.package = {
        .width = 200.0f,
        .height = 400.0f,
        .total_pins = 40,
        .has_notch = true,
        .package_name = "DIP-40"
    };
    
    // Left side pins (1-20, top to bottom)
    layout.left_pins = {
        {1,  "VSS",   PinType::POWER,     PinSide::LEFT, 0, false, false},
        {2,  "RDY",   PinType::CONTROL,   PinSide::LEFT, 0, false, true},
        {3,  "φ1",    PinType::CLOCK,     PinSide::LEFT, 0, false, false},
        {4,  "IRQ",   PinType::INTERRUPT, PinSide::LEFT, 0, true,  true},
        {5,  "NC",    PinType::SPECIAL,   PinSide::LEFT, 0, false, true},
        {6,  "NMI",   PinType::INTERRUPT, PinSide::LEFT, 0, true,  false},
        {7,  "SYNC",  PinType::CONTROL,   PinSide::LEFT, 0, false, false},
        {8,  "VCC",   PinType::POWER,     PinSide::LEFT, 0, false, false},
        {9,  "A0",    PinType::ADDRESS,   PinSide::LEFT, 0, false, false},
        {10, "A1",    PinType::ADDRESS,   PinSide::LEFT, 1, false, false},
        {11, "A2",    PinType::ADDRESS,   PinSide::LEFT, 2, false, false},
        {12, "A3",    PinType::ADDRESS,   PinSide::LEFT, 3, false, false},
        {13, "A4",    PinType::ADDRESS,   PinSide::LEFT, 4, false, false},
        {14, "A5",    PinType::ADDRESS,   PinSide::LEFT, 5, false, false},
        {15, "A6",    PinType::ADDRESS,   PinSide::LEFT, 6, false, false},
        {16, "A7",    PinType::ADDRESS,   PinSide::LEFT, 7, false, false},
        {17, "A8",    PinType::ADDRESS,   PinSide::LEFT, 8, false, false},
        {18, "A9",    PinType::ADDRESS,   PinSide::LEFT, 9, false, false},
        {19, "A10",   PinType::ADDRESS,   PinSide::LEFT, 10, false, false},
        {20, "A11",   PinType::ADDRESS,   PinSide::LEFT, 11, false, false}
    };
    
    // Right side pins (21-40, top to bottom in physical layout)
    layout.right_pins = {
        {21, "VSS",   PinType::POWER,     PinSide::RIGHT, 0, false, false},
        {22, "A12",   PinType::ADDRESS,   PinSide::RIGHT, 12, false, false},
        {23, "A13",   PinType::ADDRESS,   PinSide::RIGHT, 13, false, false},
        {24, "A14",   PinType::ADDRESS,   PinSide::RIGHT, 14, false, false},
        {25, "A15",   PinType::ADDRESS,   PinSide::RIGHT, 15, false, false},
        {26, "D7",    PinType::DATA,      PinSide::RIGHT, 7, false, false},
        {27, "D6",    PinType::DATA,      PinSide::RIGHT, 6, false, false},
        {28, "D5",    PinType::DATA,      PinSide::RIGHT, 5, false, false},
        {29, "D4",    PinType::DATA,      PinSide::RIGHT, 4, false, false},
        {30, "D3",    PinType::DATA,      PinSide::RIGHT, 3, false, false},
        {31, "D2",    PinType::DATA,      PinSide::RIGHT, 2, false, false},
        {32, "D1",    PinType::DATA,      PinSide::RIGHT, 1, false, false},
        {33, "D0",    PinType::DATA,      PinSide::RIGHT, 0, false, false},
        {34, "RW",    PinType::CONTROL,   PinSide::RIGHT, 0, false, false},
        {35, "NC",    PinType::SPECIAL,   PinSide::RIGHT, 0, false, true},
        {36, "BE",    PinType::CONTROL,   PinSide::RIGHT, 0, false, true},
        {37, "φ0",    PinType::CLOCK,     PinSide::RIGHT, 0, false, false},
        {38, "SO",    PinType::SPECIAL,   PinSide::RIGHT, 0, true,  true},
        {39, "φ2",    PinType::CLOCK,     PinSide::RIGHT, 0, false, false},
        {40, "RES",   PinType::INTERRUPT, PinSide::RIGHT, 0, true,  false}
    };
    
    // No top/bottom pins for DIP-40
    layout.top_pins = {};
    layout.bottom_pins = {};
    
    return layout;
}

PinLayout create_dip28_layout() {
    // Similar to DIP-40 but with fewer pins - implementation would be similar
    PinLayout layout;
    layout.package = {
        .width = 160.0f,
        .height = 280.0f,
        .total_pins = 28,
        .has_notch = true,
        .package_name = "DIP-28"
    };
    // Pin definitions would go here...
    return layout;
}

PinLayout create_dip24_layout() {
    PinLayout layout;
    layout.package = {
        .width = 140.0f,
        .height = 240.0f,
        .total_pins = 24,
        .has_notch = true,
        .package_name = "DIP-24"
    };
    // Pin definitions would go here...
    return layout;
}

PinLayout create_dip16_layout() {
    PinLayout layout;
    layout.package = {
        .width = 120.0f,
        .height = 160.0f,
        .total_pins = 16,
        .has_notch = true,
        .package_name = "DIP-16"
    };
    // Pin definitions would go here...
    return layout;
}

PinLayout create_plcc44_layout() {
    PinLayout layout;
    layout.package = {
        .width = 220.0f,
        .height = 220.0f,
        .total_pins = 44,
        .has_notch = false,
        .package_name = "PLCC-44"
    };
    
    // PLCC packages have pins on all 4 sides
    // Implementation would distribute pins across all sides
    // This is a placeholder - real implementation would have all pin definitions
    
    return layout;
}

PinLayout create_plcc68_layout() {
    PinLayout layout;
    layout.package = {
        .width = 260.0f,
        .height = 260.0f,
        .total_pins = 68,
        .has_notch = false,
        .package_name = "PLCC-68"
    };
    return layout;
}

PinLayout create_qfp64_layout() {
    PinLayout layout;
    layout.package = {
        .width = 200.0f,
        .height = 200.0f,
        .total_pins = 64,
        .has_notch = false,
        .package_name = "QFP-64"
    };
    return layout;
}

PinLayout create_qfp100_layout() {
    PinLayout layout;
    layout.package = {
        .width = 240.0f,
        .height = 240.0f,
        .total_pins = 100,
        .has_notch = false,
        .package_name = "QFP-100"
    };
    return layout;
}