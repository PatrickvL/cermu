/*
 * chip_visualization_c.cpp - C wrapper implementation for chip visualization system
 */

#include "chip_visualization_c.h"
#include "chip_visualization.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <vector>

extern "C" {

void render_chip_visualization_c(const struct ChipLayout* layout, 
                                const struct PinState* pin_states, 
                                const char* header_title) {
    if (!layout || !pin_states || !header_title) return;
    
    // Create collapsing header
    if (igCollapsingHeader_BoolPtr(header_title, nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        // Create ChipVisualization instance
        ChipVisualization viz(*layout);
        
        // Convert C array to std::vector
        int total_pins = get_chip_layout_total_pins(layout);
        std::vector<PinState> pin_states_vec(pin_states, pin_states + total_pins);
        
        // Calculate chip center position
        ImVec2 cursor_pos;
        igGetCursorScreenPos(&cursor_pos);
        ImVec2 chip_center = {cursor_pos.x + 150, cursor_pos.y + 100}; // Offset from cursor
        
        // Render the chip
        viz.render(chip_center, pin_states_vec, nullptr);
        
        // Add some spacing for the rendered chip
        ImVec2 chip_size = viz.get_recommended_size();
        igDummy(chip_size);
        
        igUnindent(16.0f);
    }
}

int get_chip_layout_total_pins(const struct ChipLayout* layout) {
    if (!layout) return 0;
    
    return layout->left_pins.size() + layout->right_pins.size() + 
           layout->top_pins.size() + layout->bottom_pins.size() + 
           layout->grid_pins.size();
}

} // extern "C"