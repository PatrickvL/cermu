#ifndef AIEMUC_CHIP_H
#define AIEMUC_CHIP_H

#include "aiemuc.h"  // Compiler compatibility macros
#include <stdint.h>
#include <stdbool.h>
#include "system_lines.h"

// Forward-declare the struct name
typedef struct chip_descriptor_s chip_descriptor_t;

// Unified callback type for both read and write operations using bus_state_t pattern
typedef bus_state_t (*chip_callback_t)(void* chip, bus_state_t bus_state);

// Now define it
struct chip_descriptor_s {
    const char* description;  // Human-readable description for debugging
    void* (*create)(chip_descriptor_t* desc);
    void (*destroy)(void* chip);
    void (*bus_attach)(void* chip, void* bus);
    bus_state_t (*read)(void* chip, bus_state_t bus_state);
    bus_state_t (*write)(void* chip, bus_state_t bus_state);
    void (*bank_change)(void* chip, uint8_t bank);
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    void (*render_debug_window)(void* chip, bool* show_window); // Optional GUI debug window callback
    void (*render_settings_window)(void* chip, bool* show_window); // Optional GUI settings window callback
#endif
};

// Chip registry
typedef struct {
    void* chip;
    chip_descriptor_t* desc;
    unsigned int size;
    uint16_t base_address;
    uint8_t chip_id;
} chip_entry_t;

#endif // AIEMUC_CHIP_H