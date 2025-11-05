#ifndef EMULATION_CONTEXT_H
#define EMULATION_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>
#include "system_lines.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct emulation_context_t emulation_context_t;
typedef struct chip_entry_t chip_entry_t;

// Emulation run context that tracks system state for all chips
typedef struct emulation_context_t {
    // Last known bus state - accessible to all chips for GUI visualization
    bus_state_t last_bus_state;
    
    // System clock information
    uint64_t system_cycles;
    uint64_t cpu_cycles;
    double cpu_frequency;
    
    // Emulation control
    bool is_running;
    bool is_paused;
    bool step_mode;
    
    // Performance metrics
    double emulation_speed;  // Percentage of real hardware speed
    uint32_t frames_per_second;
    
    // System identification
    const char* system_name;
    const char* system_version;
    
    // Chip registry for GUI access
    chip_entry_t* chips[32];  // Maximum 32 chips per system
    uint8_t chip_count;
    
} emulation_context_t;

// Chip entry structure for context registration
typedef struct chip_entry_t {
    void* chip_instance;           // Pointer to the actual chip
    const char* chip_name;         // Human readable name
    const char* chip_type;         // Chip type identifier (e.g., "MOS6502", "MOS6526")
    uint16_t base_address;         // Base address in memory map
    uint16_t address_range;        // Size of address range
    
    // GUI callback functions
    void (*render_debug_window)(void* chip, bool* show_window);
    void (*render_settings_window)(void* chip, bool* show_window);
    
    // Bus state update callback (optional)
    void (*update_bus_state)(void* chip, bus_state_t bus_state);
    
} chip_entry_t;

// Emulation context management functions
emulation_context_t* emulation_context_create(const char* system_name, const char* system_version);
void emulation_context_destroy(emulation_context_t* context);

// Chip registration for GUI access
bool emulation_context_register_chip(emulation_context_t* context, 
                                   void* chip_instance,
                                   const char* chip_name,
                                   const char* chip_type,
                                   uint16_t base_address,
                                   uint16_t address_range,
                                   void (*render_debug_window)(void*, bool*),
                                   void (*render_settings_window)(void*, bool*),
                                   void (*update_bus_state)(void*, bus_state_t));

void emulation_context_unregister_chip(emulation_context_t* context, void* chip_instance);

// Bus state tracking
void emulation_context_update_bus_state(emulation_context_t* context, bus_state_t new_bus_state);
bus_state_t emulation_context_get_bus_state(const emulation_context_t* context);

// System state management  
void emulation_context_start(emulation_context_t* context);
void emulation_context_pause(emulation_context_t* context);
void emulation_context_stop(emulation_context_t* context);
void emulation_context_step(emulation_context_t* context);

// Performance tracking
void emulation_context_update_performance(emulation_context_t* context, 
                                         double speed_percentage, 
                                         uint32_t fps);

// Cycle tracking
void emulation_context_advance_cycles(emulation_context_t* context, uint64_t cpu_cycles_delta);

#ifdef __cplusplus
}
#endif

#endif // EMULATION_CONTEXT_H