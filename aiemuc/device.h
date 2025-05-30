#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration
struct device_s;

// Function pointer typedefs for cleaner code
typedef uint8_t (*device_read_func_t)(struct device_s* dev, uint16_t address);
typedef void (*device_write_func_t)(struct device_s* dev, uint16_t address, uint8_t data);

// TODO : Convert this into a VMT-like feature
// Generic device structure - all devices inherit from this
typedef struct {
    void (*init)(struct device_s* dev);    // Device initialization
    void (*cleanup)(struct device_s* dev); // Device cleanup (optional)
    device_read_func_t r8;                 // Read 8-bit callback - returns data
    device_write_func_t w8;                // Write 8-bit callback
    void (*cycle)(struct device_s* dev);   // Device cycle (timers, logic)
} device_t;

// Device callback struct with separate device pointers for read and write
typedef struct {
    device_read_func_t read;                 // Returns data with device pointer and address
    device_write_func_t write;               // Write with device pointer, address and data
    struct device_s* read_device;            // Device for read operations
    struct device_s* write_device;           // Device for write operations (often RAM for ROM areas)
} device_callbacks_t;

// Generic device instance
typedef struct device_s {
    device_t callbacks;
} device_instance_t;

// ============================================================================
// SAFE DEVICE FUNCTION CALLERS
// ============================================================================

// Safe device function callers that check for function pointer assignment
static inline void device_init(struct device_s* dev) {
    if (dev) {
        const device_t* device_desc = *(const device_t**)dev;
        if (device_desc && device_desc->init) {
            device_desc->init(dev);
        }
    }
}

static inline void device_cycle(struct device_s* dev) {
    if (dev) {
        const device_t* device_desc = *(const device_t**)dev;
        if (device_desc && device_desc->cycle) {
            device_desc->cycle(dev);
        }
    }
}

static inline void device_cleanup(struct device_s* dev) {
    if (dev) {
        const device_t* device_desc = *(const device_t**)dev;
        if (device_desc && device_desc->cleanup) {
            device_desc->cleanup(dev);
        }
    }
}

static inline uint8_t device_read(struct device_s* dev, uint16_t address) {
    if (dev) {
        const device_t* device_desc = *(const device_t**)dev;
        if (device_desc && device_desc->r8) {
            return device_desc->r8(dev, address);
        }
    }
    return 0xFF; // Default value for unmapped reads
}

static inline void device_write(struct device_s* dev, uint16_t address, uint8_t data) {
    if (dev) {
        const device_t* device_desc = *(const device_t**)dev;
        if (device_desc && device_desc->w8) {
            device_desc->w8(dev, address, data);
        }
    }
}

// Helper functions for extracting callbacks from devices
device_read_func_t get_device_read_callback(struct device_s* dev);
device_write_func_t get_device_write_callback(struct device_s* dev);

#endif // DEVICE_H