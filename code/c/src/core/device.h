#ifndef AIEMUC_DEVICE_H
#define AIEMUC_DEVICE_H

#include <stdint.h>
#include <stdbool.h>

// Forward-declare the struct name
typedef struct device_descriptor_s device_descriptor_t;

// Now define it
struct device_descriptor_s {
    void* (*create)(device_descriptor_t* desc);
    void (*destroy)(void* device);
    void (*bus_attach)(void* device, void* bus);
    uint8_t (*read)(void* device, uint16_t address);
    void (*write)(void* device, uint16_t address, uint8_t value);
    void (*bank_change)(void* device, uint8_t bank);
};

typedef uint8_t (*device_read_func_t)(void* context, uint16_t);
typedef void (*device_write_func_t)(void* context, uint16_t, uint8_t);

// Device registry
typedef struct {
    void* device;
    device_descriptor_t* desc;
    void* rwcb_context; // Context for read and write callbacks. Often the deivce itself, sometimes a buffer or other structure.
    unsigned int size;
    uint16_t base_address;
    uint8_t device_id;
} device_entry_t;

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

#endif // AIEMUC_DEVICE_H