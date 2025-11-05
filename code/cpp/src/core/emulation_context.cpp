#include "emulation_context.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// EMULATION CONTEXT MANAGEMENT
// ============================================================================

emulation_context_t* emulation_context_create(const char* system_name, const char* system_version) {
    emulation_context_t* context = (emulation_context_t*)calloc(1, sizeof(emulation_context_t));
    if (!context) return NULL;
    
    // Initialize bus state to default (all lines inactive)
    context->last_bus_state = 0;
    
    // Initialize system state
    context->system_cycles = 0;
    context->cpu_cycles = 0;
    context->cpu_frequency = 1000000.0; // Default 1MHz
    
    context->is_running = false;
    context->is_paused = false;
    context->step_mode = false;
    
    context->emulation_speed = 100.0;
    context->frames_per_second = 60;
    
    // Copy system identification strings
    if (system_name) {
        size_t name_len = strlen(system_name) + 1;
        char* name_copy = (char*)malloc(name_len);
        if (name_copy) {
            strcpy(name_copy, system_name);
            context->system_name = name_copy;
        }
    }
    
    if (system_version) {
        size_t version_len = strlen(system_version) + 1;
        char* version_copy = (char*)malloc(version_len);
        if (version_copy) {
            strcpy(version_copy, system_version);
            context->system_version = version_copy;
        }
    }
    
    // Initialize chip registry
    context->chip_count = 0;
    for (int i = 0; i < 32; i++) {
        context->chips[i] = NULL;
    }
    
    return context;
}

void emulation_context_destroy(emulation_context_t* context) {
    if (!context) return;
    
    // Free system identification strings
    if (context->system_name) {
        free((void*)context->system_name);
    }
    if (context->system_version) {
        free((void*)context->system_version);
    }
    
    // Free all registered chip entries
    for (int i = 0; i < context->chip_count; i++) {
        if (context->chips[i]) {
            // ChipEntry doesn't have chip_name or chip_type fields to free
            // The strings are handled by the ChipDescriptor if needed
            free(context->chips[i]);
        }
    }
    
    free(context);
}

// ============================================================================
// CHIP REGISTRATION
// ============================================================================

bool emulation_context_register_chip(emulation_context_t* context, 
                                   void* chip_instance,
                                   const char* chip_name,
                                   const char* chip_type,
                                   uint16_t base_address,
                                   uint16_t address_range,
                                   void (*render_debug_window)(void*, bool*),
                                   void (*render_settings_window)(void*, bool*),
                                   void (*update_bus_state)(void*, bus_state_t)) {
    if (!context || !chip_instance || context->chip_count >= 32) {
        return false;
    }
    
    // Create new chip entry
    chip_entry_t* entry = (chip_entry_t*)calloc(1, sizeof(chip_entry_t));
    if (!entry) return false;
    
    entry->chip = chip_instance;
    entry->base_address = base_address;
    // Note: ChipEntry doesn't have address_range, render_debug_window, render_settings_window, or update_bus_state
    // These are handled through the ChipDescriptor in entry->desc
    // For now, we'll simplify and not store these GUI-specific callbacks
    
    // Copy chip name
    if (chip_name) {
        size_t name_len = strlen(chip_name) + 1;
        char* name_copy = (char*)malloc(name_len);
        if (name_copy) {
            strcpy(name_copy, chip_name);
            // ChipEntry doesn't have chip_name - this would be in the ChipDescriptor description field
            // Skip storing name for now since it's mainly for GUI purposes
        }
    }
    
    // Copy chip type
    if (chip_type) {
        size_t type_len = strlen(chip_type) + 1;
        char* type_copy = (char*)malloc(type_len);
        if (type_copy) {
            strcpy(type_copy, chip_type);
            // ChipEntry doesn't have chip_type - this would be in the ChipDescriptor
            // Skip storing type for now since it's mainly for GUI purposes
        }
    }
    
    // Add to registry
    context->chips[context->chip_count] = entry;
    context->chip_count++;
    
    return true;
}

void emulation_context_unregister_chip(emulation_context_t* context, void* chip_instance) {
    if (!context || !chip_instance) return;
    
    // Find and remove the chip entry
    for (int i = 0; i < context->chip_count; i++) {
        if (context->chips[i] && context->chips[i]->chip == chip_instance) {
            // Free chip entry resources
            // ChipEntry doesn't have chip_name or chip_type fields to free
            // No cleanup needed for these fields
            free(context->chips[i]);
            
            // Shift remaining entries down
            for (int j = i; j < context->chip_count - 1; j++) {
                context->chips[j] = context->chips[j + 1];
            }
            context->chips[context->chip_count - 1] = NULL;
            context->chip_count--;
            break;
        }
    }
}

// ============================================================================
// BUS STATE TRACKING
// ============================================================================

void emulation_context_update_bus_state(emulation_context_t* context, bus_state_t new_bus_state) {
    if (!context) return;
    
    context->last_bus_state = new_bus_state;
    
    // Notify all registered chips of the bus state update
    for (int i = 0; i < context->chip_count; i++) {
        // ChipEntry doesn't have update_bus_state callback
        // This functionality would need to be handled differently if needed
        // For now, skip the bus state update functionality
    }
}

bus_state_t emulation_context_get_bus_state(const emulation_context_t* context) {
    return context ? context->last_bus_state : 0;
}

// ============================================================================
// SYSTEM STATE MANAGEMENT
// ============================================================================

void emulation_context_start(emulation_context_t* context) {
    if (!context) return;
    
    context->is_running = true;
    context->is_paused = false;
    context->step_mode = false;
}

void emulation_context_pause(emulation_context_t* context) {
    if (!context) return;
    
    context->is_paused = !context->is_paused;
}

void emulation_context_stop(emulation_context_t* context) {
    if (!context) return;
    
    context->is_running = false;
    context->is_paused = false;
    context->step_mode = false;
}

void emulation_context_step(emulation_context_t* context) {
    if (!context) return;
    
    context->is_running = true;
    context->is_paused = false;
    context->step_mode = true;
}

// ============================================================================
// PERFORMANCE TRACKING
// ============================================================================

void emulation_context_update_performance(emulation_context_t* context, 
                                         double speed_percentage, 
                                         uint32_t fps) {
    if (!context) return;
    
    context->emulation_speed = speed_percentage;
    context->frames_per_second = fps;
}

void emulation_context_advance_cycles(emulation_context_t* context, uint64_t cpu_cycles_delta) {
    if (!context) return;
    
    context->cpu_cycles += cpu_cycles_delta;
    context->system_cycles += cpu_cycles_delta; // Assume 1:1 for now, can be adjusted per system
}