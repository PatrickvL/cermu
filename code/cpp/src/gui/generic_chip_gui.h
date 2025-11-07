#ifndef GENERIC_CHIP_GUI_H
#define GENERIC_CHIP_GUI_H

#include <stdint.h>
#include <stdbool.h>
#include "../core/chip_layout.h"
#include "../core/emulation_context.h"

// Include chip visualization for ChipVisualConfig
#ifdef __cplusplus
#include "chip_visualization.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct generic_chip_gui_t generic_chip_gui_t;

// Chip-specific callbacks for retrieving pin states and layouts
typedef ChipLayout (*get_chip_layout_func_t)(void* chip);
typedef void (*get_chip_pin_states_func_t)(void* chip, ChipLayout* layout, bus_state_t bus_state, PinSignalState* pin_states);

// Generic chip visualization configuration - now uses ChipVisualConfig for rendering
typedef struct {
    // Chip identification
    const char* chip_name;
    const char* chip_type;
    
    // Layout and visualization callbacks
    get_chip_layout_func_t get_layout;
    get_chip_pin_states_func_t get_pin_states;
    
    // Chip-specific display options (non-visual)
    float chip_scale;              // Scale factor for this specific chip
    
    // Legacy display options for compatibility - now mostly unused
    bool show_pin_numbers;
    bool show_pin_labels;
    bool show_pin_states;
    bool show_package_outline;
    bool show_chip_markings;
    
    // Legacy layout options for compatibility
    float pin_label_size;
    float pin_state_size;
    
    // Legacy colors for compatibility - should migrate to ChipVisualConfig
    uint32_t background_color;
    uint32_t package_color;
    uint32_t pin_color_inactive;
    uint32_t pin_color_active_high;
    uint32_t pin_color_active_low;
    uint32_t pin_color_tristate;
    uint32_t text_color;
    
    // Visual configuration reference (for future migration)
    #ifdef __cplusplus
    struct ChipVisualConfig* visual_config;  // Reference to shared visual config (optional)
    #else
    void* visual_config;                     // Reference to shared visual config (optional)
    #endif
    
} chip_gui_config_t;

// Generic chip GUI instance
typedef struct generic_chip_gui_t {
    void* chip_instance;
    chip_gui_config_t config;
    ChipLayout cached_layout;
    PinSignalState* cached_pin_states;
    bool layout_cached;
    bus_state_t last_bus_state;
} generic_chip_gui_t;

// ============================================================================
// GENERIC CHIP GUI FUNCTIONS
// ============================================================================

// Create and destroy generic chip GUI instances
generic_chip_gui_t* generic_chip_gui_create(void* chip_instance, const chip_gui_config_t* config);
void generic_chip_gui_destroy(generic_chip_gui_t* gui);

// Main rendering functions
void generic_chip_gui_render_layout(generic_chip_gui_t* gui, 
                                   emulation_context_t* context,
                                   float width, 
                                   float height);

void generic_chip_gui_render_debug_panel(generic_chip_gui_t* gui,
                                        emulation_context_t* context,
                                        const char* window_title,
                                        bool* show_window,
                                        void (*render_chip_specific_content)(void* chip));

void generic_chip_gui_render_settings_panel(generic_chip_gui_t* gui,
                                           const char* window_title,
                                           bool* show_window,
                                           void (*render_chip_specific_settings)(void* chip));

// Configuration management
void generic_chip_gui_update_config(generic_chip_gui_t* gui, const chip_gui_config_t* config);
chip_gui_config_t generic_chip_gui_get_default_config(const char* chip_name, const char* chip_type);

// Bus state and pin state management
void generic_chip_gui_update_bus_state(generic_chip_gui_t* gui, bus_state_t bus_state);
void generic_chip_gui_refresh_layout(generic_chip_gui_t* gui);

// ============================================================================
// CHIP-SPECIFIC LAYOUT HELPERS
// ============================================================================

// Default implementations for common chip types
ChipLayout generic_chip_gui_get_dip_layout(void* chip, int pin_count);
void generic_chip_gui_get_basic_pin_states(void* chip, ChipLayout* layout, bus_state_t bus_state, PinSignalState* pin_states);

// Pin state color mapping
uint32_t generic_chip_gui_get_pin_color(const chip_gui_config_t* config, const PinSignalState* pin_state);

// Layout rendering helpers
void generic_chip_gui_render_dip_package(const ChipLayout* layout, const chip_gui_config_t* config, float x, float y, float scale);
void generic_chip_gui_render_pin(const ChipPin* pin, const PinSignalState* pin_state, const chip_gui_config_t* config, float x, float y, float scale);
void generic_chip_gui_render_chip_markings(const ChipLayout* layout, const chip_gui_config_t* config, float x, float y, float scale);

#ifdef __cplusplus
}
#endif

#endif // GENERIC_CHIP_GUI_H