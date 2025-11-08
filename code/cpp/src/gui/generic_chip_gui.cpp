#include "generic_chip_gui.h"
#include "imgui_interface.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Include ImGui only when available (conditional compilation)
#ifdef IMGUI_VERSION
#include <imgui.h>
#endif

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

chip_gui_config_t generic_chip_gui_get_default_config(const char* chip_name, const char* chip_type) {
    chip_gui_config_t config = {};
    
    config.chip_name = chip_name;
    config.chip_type = chip_type;
    
    // Initialize with default ChipVisualConfig
    config.visual_config = ChipVisualConfig::get_default();
    
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

uint32_t generic_chip_gui_get_pin_color(const chip_gui_config_t* config, const PinSignalState* pin_state) {
    if (!config || !pin_state || !pin_state->signal_valid) {
        return config->visual_config.led_inactive_color;
    }
    
    if (pin_state->high_impedance) {
        return 0xFF0080FF; // Blue for tristate
    }
    
    if (pin_state->signal_level) {
        return config->visual_config.led_active_color;
    } else {
        return config->visual_config.led_inactive_color;
    }
}

// ============================================================================
// RENDERING FUNCTIONS
// ============================================================================

void generic_chip_gui_render_layout(generic_chip_gui_t* gui,
                                   emulation_context_t* context,
                                   float width,
                                   float height) {
    if (!gui) return;
    
    // Update cached layout if needed
    if (!gui->layout_cached && gui->config.get_layout) {
        gui->cached_layout = gui->config.get_layout(gui->chip_instance);
        gui->layout_cached = true;
    }
    
    // Check if we have a valid layout
    if (!gui->layout_cached) {
#ifdef IMGUI_VERSION
        ImGui::Text("No chip layout available");
        ImGui::Text("get_layout callback is NULL");
#endif
        return;
    }
    
    // Get current bus state (use 0 if no context)
    bus_state_t current_bus_state = context ? emulation_context_get_bus_state(context) : 0;
    
    // Update cached pin states if bus state changed or not cached
    if (!gui->cached_pin_states || gui->last_bus_state != current_bus_state) {
        int total_pins = gui->cached_layout.get_total_pins();
        if (total_pins > 0) {
            if (gui->cached_pin_states) {
                free(gui->cached_pin_states);
            }
            gui->cached_pin_states = (PinSignalState*)calloc(total_pins, sizeof(PinSignalState));
            
            if (gui->config.get_pin_states && gui->cached_pin_states) {
                gui->config.get_pin_states(gui->chip_instance, &gui->cached_layout,
                                         current_bus_state, gui->cached_pin_states);
            }
        }
        gui->last_bus_state = current_bus_state;
    }
    
    // Calculate chip position and scale
    float scale = 1.0f; // Default scale
#ifdef IMGUI_VERSION
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    float chip_x = canvas_pos.x + width * 0.5f;
    float chip_y = canvas_pos.y + height * 0.5f;
#else
    float chip_x = width * 0.5f;
    float chip_y = height * 0.5f;
#endif
    
    // Render chip package
    if (gui->config.visual_config.show_package_name) {
        generic_chip_gui_render_dip_package(&gui->cached_layout, &gui->config, chip_x, chip_y, scale);
    }
    
    // Render chip markings
    if (gui->config.visual_config.show_chip_markings) {
        generic_chip_gui_render_chip_markings(&gui->cached_layout, &gui->config, chip_x, chip_y, scale);
    }
    
    // Render pins safely
    int pin_index = 0;
    PinSignalState* pin_states = gui->cached_pin_states;
    
    // Render left pins
    if (!gui->cached_layout.left_pins.empty()) {
        for (size_t i = 0; i < gui->cached_layout.left_pins.size(); i++) {
            const ChipPin* pin = &gui->cached_layout.left_pins[i];
            const PinSignalState* state = (pin_states && static_cast<size_t>(pin_index) < gui->cached_layout.get_total_pins()) ?
                                        &pin_states[pin_index] : NULL;
            generic_chip_gui_render_pin(pin, state, &gui->config, chip_x - 50, chip_y - 100 + i * 20, scale);
            pin_index++;
        }
    }
    
    // Render right pins
    if (!gui->cached_layout.right_pins.empty()) {
        for (size_t i = 0; i < gui->cached_layout.right_pins.size(); i++) {
            const ChipPin* pin = &gui->cached_layout.right_pins[i];
            const PinSignalState* state = (pin_states && static_cast<size_t>(pin_index) < gui->cached_layout.get_total_pins()) ?
                                        &pin_states[pin_index] : NULL;
            generic_chip_gui_render_pin(pin, state, &gui->config, chip_x + 50, chip_y - 100 + i * 20, scale);
            pin_index++;
        }
    }
}

void generic_chip_gui_render_debug_panel(generic_chip_gui_t* gui,
                                        emulation_context_t* context,
                                        const char* window_title,
                                        bool* show_window,
                                        void (*render_chip_specific_content)(void* chip)) {
    if (!gui || !show_window || !*show_window) return;
    
#ifdef IMGUI_VERSION
    // Set proper window size for first time opening
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin(window_title, show_window, 0)) {
        ImGui::End();
        return;
    }
    
    // Create two-column layout
    ImGui::Columns(2, "chip_debug_columns", true);
    ImGui::SetColumnWidth(0, 350); // Fixed width for chip visualization column
    
    // Left column: Chip visualization using ChipLayout
    ImGui::Text("Chip Layout");
    ImGui::Separator();
    
    ImVec2 avail_size = ImGui::GetContentRegionAvail();
    float layout_height = avail_size.y - 50; // Leave space for controls
    
    if (ImGui::BeginChild("chip_layout", ImVec2(avail_size.x, layout_height), true, 0)) {
        ImVec2 child_size = ImGui::GetContentRegionAvail();
        generic_chip_gui_render_layout(gui, context, child_size.x, child_size.y);
    }
    ImGui::EndChild();
    
    // Layout controls
    ImGui::Checkbox("Show Pin Numbers", &gui->config.visual_config.show_pin_numbers);
    ImGui::Checkbox("Show Pin Labels", &gui->config.visual_config.show_pin_labels);
    ImGui::Checkbox("Show LEDs", &gui->config.visual_config.show_led_indicators);
    
    // Move to right column
    ImGui::NextColumn();
    
    // Right column: Chip-specific debug content
    ImGui::Text("Debug Information");
    ImGui::Separator();
    
    if (render_chip_specific_content) {
        render_chip_specific_content(gui->chip_instance);
    }
    
    ImGui::Columns(1, NULL, false);
    ImGui::End();
#endif
}

void generic_chip_gui_render_settings_panel(generic_chip_gui_t* gui,
                                           const char* window_title,
                                           bool* show_window,
                                           void (*render_chip_specific_settings)(void* chip)) {
    if (!gui || !show_window || !*show_window) return;
    
#ifdef IMGUI_VERSION
    // Set proper window size for first time opening
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin(window_title, show_window, 0)) {
        ImGui::End();
        return;
    }
    
    ImGui::Text("Chip Visualization Settings");
    ImGui::Separator();
    
    // Visualization options
    ImGui::Checkbox("Show Package Name", &gui->config.visual_config.show_package_name);
    ImGui::Checkbox("Show Chip Markings", &gui->config.visual_config.show_chip_markings);
    ImGui::SliderFloat("Font Size", &gui->config.visual_config.font_size, 8.0f, 20.0f, "%.1f", 0);
    
    ImGui::Separator();
    
    // Chip-specific settings
    if (render_chip_specific_settings) {
        render_chip_specific_settings(gui->chip_instance);
    }
    
    ImGui::End();
#endif
}

// ============================================================================
// RENDERING HELPERS
// ============================================================================

void generic_chip_gui_render_dip_package(const ChipLayout* layout, const chip_gui_config_t* config, float x, float y, float scale) {
    if (!layout || !config) return;
    
#ifdef IMGUI_VERSION
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    uint32_t package_color = config->visual_config.chip_body_color;
    
    // Use proper hardware-accurate scaling (mils to pixels)
    float mil_to_pixel = scale * 0.1f; // Convert mils to pixels with proper scaling
    float width = layout->package.width * mil_to_pixel;
    float height = layout->package.height * mil_to_pixel;
    
    ImVec2 p1 = {x - width/2, y - height/2};
    ImVec2 p2 = {x + width/2, y + height/2};
    
    draw_list->AddRectFilled(p1, p2, package_color, 0.0f, 0);
    draw_list->AddRect(p1, p2, config->visual_config.chip_border_color, 0.0f, 0, config->visual_config.chip_border_width);
    
    // Add notch for orientation
    if (layout->package.marker == OrientationMarker::NOTCH) {
        ImVec2 notch_p1 = {x - 10*scale, y - height/2 - 5*scale};
        ImVec2 notch_p2 = {x + 10*scale, y - height/2};
        draw_list->AddRectFilled(notch_p1, notch_p2, 0xFF000000, 0.0f, 0); // Black notch
    }
#endif
}

void generic_chip_gui_render_pin(const ChipPin* pin, const PinSignalState* pin_state, const chip_gui_config_t* config, float x, float y, float scale) {
    if (!pin || !config) return;
    
#ifdef IMGUI_VERSION
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Get pin color
    uint32_t pin_color = pin_state ?
        generic_chip_gui_get_pin_color(config, pin_state) :
        config->visual_config.led_inactive_color;
    
    // Draw pin
    float pin_size = 8.0f * scale;
    ImVec2 pin_center = {x, y};
    draw_list->AddCircleFilled(pin_center, pin_size, pin_color, 8);
    
    // Draw pin number
    if (config->visual_config.show_pin_numbers) {
        char pin_num[8];
        snprintf(pin_num, sizeof(pin_num), "%d", pin->pin_number);
        ImVec2 text_pos = {x - 15*scale, y - 5*scale};
        draw_list->AddText(text_pos, config->visual_config.pin_number_color, pin_num);
    }
    
    // Draw pin label
    if (config->visual_config.show_pin_labels && pin->label != PinLabel::NC) {
        const char* label = pin_label_to_string(pin->label);
        ImVec2 text_pos = {x + 15*scale, y - 5*scale};
        draw_list->AddText(text_pos, config->visual_config.text_color, label);
    }
#endif
}

void generic_chip_gui_render_chip_markings(const ChipLayout* layout, const chip_gui_config_t* config, float x, float y, float scale) {
    if (!layout || !config) return;
    
#ifdef IMGUI_VERSION
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Render part number
    if (layout->markings.show_part_number && layout->markings.part_number) {
        ImVec2 text_pos = {x - 50*scale, y - 10*scale};
        draw_list->AddText(text_pos, config->visual_config.text_color, layout->markings.part_number);
    }
    
    // Render manufacturer
    if (layout->markings.show_manufacturer && layout->markings.manufacturer) {
        ImVec2 text_pos = {x - 50*scale, y + 10*scale};
        draw_list->AddText(text_pos, config->visual_config.text_color, layout->markings.manufacturer);
    }
#endif
}

// ============================================================================
// DEFAULT IMPLEMENTATIONS
// ============================================================================

ChipLayout generic_chip_gui_get_dip_layout(void* chip, int pin_count) {
    // Return a basic DIP layout - this would be overridden by chip-specific implementations
    ChipLayout layout = {};
    
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

void generic_chip_gui_get_basic_pin_states(void* chip, ChipLayout* layout, bus_state_t bus_state, PinSignalState* pin_states) {
    if (!layout || !pin_states) return;
    
    // Default implementation - all pins inactive
    int total_pins = layout->get_total_pins();
    for (int i = 0; i < total_pins; i++) {
        pin_states[i] = PinSignalState{
            .pin_number = static_cast<uint8_t>(i + 1),
            .signal_level = false,
            .drive_direction = false,
            .signal_value = 0,
            .high_impedance = true,
            .has_pullup = false,
            .has_pulldown = false,
            .signal_valid = true,
            .analog_voltage = 0.0f,
            .is_pwm = false,
            .pwm_duty_cycle = 0.0f
        };
    }
}