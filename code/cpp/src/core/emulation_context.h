#ifndef EMULATION_CONTEXT_H
#define EMULATION_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>
#include "system_lines.h"
#include "chip.h"  // Include chip.h to use ChipEntry

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct emulation_context_t;

// Emulation run context that tracks system state for all chips
struct emulation_context_t {
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
    
    // Chip registry for GUI access - use ChipEntry from chip.h
    ChipEntry* chips[32];  // Maximum 32 chips per system
    uint8_t chip_count;
    
};

// Note: We now use ChipEntry from chip.h instead of defining our own chip_entry_t

// Emulation context management functions
struct emulation_context_t* emulation_context_create(const char* system_name, const char* system_version);
void emulation_context_destroy(struct emulation_context_t* context);

// Chip registration for GUI access
bool emulation_context_register_chip(struct emulation_context_t* context,
                                   void* chip_instance,
                                   const char* chip_name,
                                   const char* chip_type,
                                   uint16_t base_address,
                                   uint16_t address_range,
                                   void (*render_debug_window)(void*, bool*),
                                   void (*render_settings_window)(void*, bool*),
                                   void (*update_bus_state)(void*, bus_state_t));

void emulation_context_unregister_chip(struct emulation_context_t* context, void* chip_instance);

// Bus state tracking
void emulation_context_update_bus_state(struct emulation_context_t* context, bus_state_t new_bus_state);
bus_state_t emulation_context_get_bus_state(const struct emulation_context_t* context);

// System state management
void emulation_context_start(struct emulation_context_t* context);
void emulation_context_pause(struct emulation_context_t* context);
void emulation_context_stop(struct emulation_context_t* context);
void emulation_context_step(struct emulation_context_t* context);

// Performance tracking
void emulation_context_update_performance(struct emulation_context_t* context,
                                         double speed_percentage,
                                         uint32_t fps);

// Cycle tracking
void emulation_context_advance_cycles(struct emulation_context_t* context, uint64_t cpu_cycles_delta);

#ifdef __cplusplus
}
#endif

#endif // EMULATION_CONTEXT_H