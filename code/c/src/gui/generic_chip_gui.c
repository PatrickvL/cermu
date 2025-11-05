#include "generic_chip_gui.h"
#include "cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

chip_gui_config_t generic_chip_gui_get_default_config(const char* chip_name, const char* chip_type) {
    chip_gui_config_t config = {0};
    
    config.chip_name = chip_name;
    config.chip_type = chip_type;
    
    // Default display options
    config.show_pin_numbers = true;
    config.show_pin_labels = true;
    config.show_pin_states = true;
    config.show_package_outline = true;
    config.show_chip_markings = true;
    
    // Default layout options
    config.chip_scale = 1.0f;
    config.pin_label_size = 12.0f;
    config.pin_state_size = 8.0f;
    
    // Default colors (RGBA in hex format)
    config.background_color = 0xFF1E1E1E;     // Dark gray
    config.package_color = 0xFF3A3A3A;        // Medium gray
    config.pin_color_inactive = 0xFF606060;   // Gray
    config.pin_color_active_high = 0xFF00FF00; // Green
    config.pin_color_active_low = 0xFFFF0000;  // Red
    config.pin_color_tristate = 0xFF0080FF;   // Blue
    config.text_color = 0xFFFFFFFF;           // White
    
    return config;
}

// ============================================================================
// CHIP GUI MANAGEMENT
// ============================================================================

generic_chip_gui_t* generic_chip_gui_create(void* chip_instance, const chip_gui_config_t* config) {
    if (!chip_instance || !config) return NULL;
    
    generic_chip_gui_t* gui = (generic_chip_gui_t*)calloc(1, sizeof(generic_chip_gui_t));
    if (!gui) return NULL;
    
    gui->chip_instance = chip_instance;
    gui->config = *config;
    gui->layout_cached = false;
    gui->last_bus_state = 0;
    gui->cached_pin_states = NULL;
    
    return gui;
}

void generic_chip_gui_destroy(generic_chip_gui_t* gui) {
    if (!gui) return;
    
    if (gui->cached_pin_states) {
        free(gui->cached_pin_states);
    }
    
    free(gui);
}

void generic_chip_gui_update_config(generic_chip_gui_t* gui, const chip_gui_config_t* config) {
    if (!gui || !config) return;
    
    gui->config = *config;
}

// ============================================================================
// BUS STATE MANAGEMENT
// ============================================================================

void generic_chip_gui_update_bus_state(generic_chip_gui_t* gui, bus_state_t bus_state) {
    if (!gui) return;
    
    gui->last_bus_state = bus_state;
    
    // Invalidate cached pin states if bus state changed
    if (gui->cached_pin_states) {
        free(gui->cached_pin_states);
        gui->cached_pin_states = NULL;
    }
}

void generic_chip_gui_refresh_layout(generic_chip_gui_t* gui) {
    if (!gui) return;
    
    gui->layout_cached = false;
    if (gui->cached_pin_states) {
        free(gui->cached_pin_states);
        gui->cached_pin_states = NULL;
    }
}

// ============================================================================
// COLOR MANAGEMENT
// ============================================================================

uint32_t generic_chip_gui_get_pin_color(const chip_gui_config_t* config, const PinState* pin_state) {
    if (!config || !pin_state || !pin_state->is_valid) {
        return config->pin_color_inactive;
    }
    
    if (pin_state->is_tristate) {
        return config->pin_color_tristate;
    }
    
    if (pin_state->is_active) {
        return config->pin_color_active_high;
    } else {
        return config->pin_color_active_low;
    }
}

// ============================================================================
// RENDERING FUNCTIONS
// ============================================================================

void generic_chip_gui_render_layout(generic_chip_gui_t* gui, 
                                   emulation_context_t* context,
                                   float width, 
                                   float height) {
    if (!gui || !context) return;
    
    // Get current bus state
    bus_state_t current_bus_state = emulation_context_get_bus_state(context);
    
    // Update cached layout if needed
    if (!gui->layout_cached && gui->config.get_layout) {
        gui->cached_layout = gui->config.get_layout(gui->chip_instance);
        gui->layout_cached = true;
    }
    
    // Update cached pin states if bus state changed or not cached
    if (!gui->cached_pin_states || gui->last_bus_state != current_bus_state) {
        int total_pins = gui->cached_layout.get_total_pins();
        if (total_pins > 0) {
            if (gui->cached_pin_states) {
                free(gui->cached_pin_states);
            }
            gui->cached_pin_states = (PinState*)calloc(total_pins, sizeof(PinState));
            
            if (gui->config.get_pin_states) {
                gui->config.get_pin_states(gui->chip_instance, &gui->cached_layout, 
                                         current_bus_state, gui->cached_pin_states);
            }
        }
        gui->last_bus_state = current_bus_state;
    }
    
    // Get drawing context
    ImDrawList* draw_list = igGetWindowDrawList();
    ImVec2 canvas_pos;
    igGetCursorScreenPos(&canvas_pos);
    
    // Calculate chip position and scale
    float scale = gui->config.chip_scale;
    float chip_x = canvas_pos.x + width * 0.5f;
    float chip_y = canvas_pos.y + height * 0.5f;
    
    // Render chip package
    if (gui->config.show_package_outline) {
        generic_chip_gui_render_dip_package(&gui->cached_layout, &gui->config, chip_x, chip_y, scale);
    }
    
    // Render chip markings
    if (gui->config.show_chip_markings) {
        generic_chip_gui_render_chip_markings(&gui->cached_layout, &gui->config, chip_x, chip_y, scale);
    }
    
    // Render pins
    int pin_index = 0;
    PinState* pin_states = gui->cached_pin_states;
    
    // Render left pins
    for (size_t i = 0; i < gui->cached_layout.left_pins.size(); i++) {
        const ChipPin* pin = &gui->cached_layout.left_pins.data()[i];
        const PinState* state = pin_states ? &pin_states[pin_index] : NULL;
        generic_chip_gui_render_pin(pin, state, &gui->config, chip_x - 50, chip_y - 100 + i * 20, scale);
        pin_index++;
    }
    
    // Render right pins
    for (size_t i = 0; i < gui->cached_layout.right_pins.size(); i++) {
        const ChipPin* pin = &gui->cached_layout.right_pins.data()[i];
        const PinState* state = pin_states ? &pin_states[pin_index] : NULL;
        generic_chip_gui_render_pin(pin, state, &gui->config, chip_x + 50, chip_y - 100 + i * 20, scale);
        pin_index++;
    }
}

void generic_chip_gui_render_debug_panel(generic_chip_gui_t* gui,
                                        emulation_context_t* context,
                                        const char* window_title,
                                        bool* show_window,
                                        void (*render_chip_specific_content)(void* chip)) {
    if (!gui || !show_window || !*show_window) return;
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }
    
    // Create two-column layout
    igColumns(2, "chip_debug_columns", true);
    
    // Left column: Chip visualization using ChipLayout
    igText("Chip Layout");
    igSeparator();
    
    ImVec2 avail_size;
    igGetContentRegionAvail(&avail_size);
    float layout_height = avail_size.y - 50; // Leave space for controls
    
    if (igBeginChild_Str("chip_layout", (ImVec2){avail_size.x, layout_height}, true, 0)) {
        ImVec2 child_size;
        igGetContentRegionAvail(&child_size);
        generic_chip_gui_render_layout(gui, context, child_size.x, child_size.y);
    }
    igEndChild();
    
    // Layout controls
    igCheckbox("Show Pin Numbers", &gui->config.show_pin_numbers);
    igCheckbox("Show Pin Labels", &gui->config.show_pin_labels);
    igCheckbox("Show Pin States", &gui->config.show_pin_states);
    
    // Move to right column
    igNextColumn();
    
    // Right column: Chip-specific debug content
    igText("Debug Information");
    igSeparator();
    
    if (render_chip_specific_content) {
        render_chip_specific_content(gui->chip_instance);
    }
    
    igColumns(1, NULL, false);
    igEnd();
}

void generic_chip_gui_render_settings_panel(generic_chip_gui_t* gui,
                                           const char* window_title,
                                           bool* show_window,
                                           void (*render_chip_specific_settings)(void* chip)) {
    if (!gui || !show_window || !*show_window) return;
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }
    
    igText("Chip Visualization Settings");
    igSeparator();
    
    // Visualization options
    igCheckbox("Show Package Outline", &gui->config.show_package_outline);
    igCheckbox("Show Chip Markings", &gui->config.show_chip_markings);
    igSliderFloat("Chip Scale", &gui->config.chip_scale, 0.5f, 2.0f, "%.1f", 0);
    igSliderFloat("Pin Label Size", &gui->config.pin_label_size, 8.0f, 20.0f, "%.1f", 0);
    
    igSeparator();
    
    // Chip-specific settings
    if (render_chip_specific_settings) {
        render_chip_specific_settings(gui->chip_instance);
    }
    
    igEnd();
}

// ============================================================================
// RENDERING HELPERS
// ============================================================================

void generic_chip_gui_render_dip_package(const ChipLayout* layout, const chip_gui_config_t* config, float x, float y, float scale) {
    if (!layout || !config) return;
    
    ImDrawList* draw_list = igGetWindowDrawList();
    uint32_t package_color = config->package_color;
    
    // Simple rectangle for DIP package
    float width = layout->package.width * scale;
    float height = layout->package.height * scale;
    
    ImVec2 p1 = {x - width/2, y - height/2};
    ImVec2 p2 = {x + width/2, y + height/2};
    
    ImDrawList_AddRectFilled(draw_list, p1, p2, package_color, 0.0f, 0);
    ImDrawList_AddRect(draw_list, p1, p2, config->text_color, 0.0f, 0, 2.0f);
    
    // Add notch for orientation
    if (layout->package.marker == OrientationMarker::NOTCH) {
        ImVec2 notch_p1 = {x - 10*scale, y - height/2 - 5*scale};
        ImVec2 notch_p2 = {x + 10*scale, y - height/2};
        ImDrawList_AddRectFilled(draw_list, notch_p1, notch_p2, config->background_color, 0.0f, 0);
    }
}

void generic_chip_gui_render_pin(const ChipPin* pin, const PinState* pin_state, const chip_gui_config_t* config, float x, float y, float scale) {
    if (!pin || !config) return;
    
    ImDrawList* draw_list = igGetWindowDrawList();
    
    // Get pin color
    uint32_t pin_color = pin_state ? 
        generic_chip_gui_get_pin_color(config, pin_state) : 
        config->pin_color_inactive;
    
    // Draw pin
    float pin_size = 8.0f * scale;
    ImVec2 pin_center = {x, y};
    ImDrawList_AddCircleFilled(draw_list, pin_center, pin_size, pin_color, 8);
    
    // Draw pin number
    if (config->show_pin_numbers) {
        char pin_num[8];
        snprintf(pin_num, sizeof(pin_num), "%d", pin->pin_number);
        ImVec2 text_pos = {x - 15*scale, y - 5*scale};
        ImDrawList_AddText_Vec2(draw_list, text_pos, config->text_color, pin_num, NULL);
    }
    
    // Draw pin label
    if (config->show_pin_labels && pin->label != PinLabel::NC) {
        const char* label = pin_label_to_string(pin->label);
        ImVec2 text_pos = {x + 15*scale, y - 5*scale};
        ImDrawList_AddText_Vec2(draw_list, text_pos, config->text_color, label, NULL);
    }
}

void generic_chip_gui_render_chip_markings(const ChipLayout* layout, const chip_gui_config_t* config, float x, float y, float scale) {
    if (!layout || !config) return;
    
    ImDrawList* draw_list = igGetWindowDrawList();
    
    // Render part number
    if (layout->markings.show_part_number && layout->markings.part_number) {
        ImVec2 text_pos = {x - 50*scale, y - 10*scale};
        ImDrawList_AddText_Vec2(draw_list, text_pos, config->text_color, layout->markings.part_number, NULL);
    }
    
    // Render manufacturer
    if (layout->markings.show_manufacturer && layout->markings.manufacturer) {
        ImVec2 text_pos = {x - 50*scale, y + 10*scale};
        ImDrawList_AddText_Vec2(draw_list, text_pos, config->text_color, layout->markings.manufacturer, NULL);
    }
}

// ============================================================================
// DEFAULT IMPLEMENTATIONS
// ============================================================================

ChipLayout generic_chip_gui_get_dip_layout(void* chip, int pin_count) {
    // Return a basic DIP layout - this would be overridden by chip-specific implementations
    ChipLayout layout = {0};
    
    switch (pin_count) {
        case 40:
            layout = create_dip40_layout();
            break;
        case 28:
            layout = create_dip28_layout();
            break;
        default:
            layout = create_dip40_layout(); // Default fallback
            break;
    }
    
    return layout;
}

void generic_chip_gui_get_basic_pin_states(void* chip, ChipLayout* layout, bus_state_t bus_state, PinState* pin_states) {
    if (!layout || !pin_states) return;
    
    // Default implementation - all pins inactive
    int total_pins = layout->get_total_pins();
    for (int i = 0; i < total_pins; i++) {
        pin_states[i] = (PinState){false, false, 0, false, true};
    }
}