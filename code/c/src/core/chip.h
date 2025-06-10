#ifndef AIEMUC_CHIP_H
#define AIEMUC_CHIP_H

#include <stdint.h>
#include <stdbool.h>

// Forward-declare the struct name
typedef struct chip_descriptor_s chip_descriptor_t;

// Now define it
struct chip_descriptor_s {
    const char* description;            // Human-readable description for debugging
    void* (*create)(chip_descriptor_t* desc);
    void (*destroy)(void* chip);
    void (*bus_attach)(void* chip, void* bus);
    uint8_t (*read)(void* chip, uint16_t address);
    void (*write)(void* chip, uint16_t address, uint8_t value);
    void (*bank_change)(void* chip, uint8_t bank);
    void* (*get_rwcb_context)(void* chip); // Optional callback to set custom rwcb_context
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    void (*render_debug_window)(void* chip, bool* show_window); // Optional GUI debug window callback
    void (*render_settings_window)(void* chip, bool* show_window); // Optional GUI settings window callback
#endif
};

typedef uint8_t (*chip_read_func_t)(void* context, uint16_t);
typedef void (*chip_write_func_t)(void* context, uint16_t, uint8_t);

// Chip registry
typedef struct {
    void* chip;
    chip_descriptor_t* desc;
    void* rwcb_context; // Context for read and write callbacks. Often the chip itself, sometimes a buffer or other structure.
    unsigned int size;
    uint16_t base_address;
    uint8_t chip_id;
} chip_entry_t;

// ============================================================================
// GENERIC STUB FUNCTIONS
// ============================================================================

/**
 * Generic stub read function for unattached callbacks.
 * Returns 0x00 for any read operation.
 * Use this to eliminate null checks in high-frequency code paths.
 */
uint8_t generic_stub_read(void* context, uint16_t address);

/**
 * Generic stub write function for unattached callbacks.
 * Does nothing for any write operation.
 * Use this to eliminate null checks in high-frequency code paths.
 */
void generic_stub_write(void* context, uint16_t address, uint8_t value);

#endif // AIEMUC_CHIP_H