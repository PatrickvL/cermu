#include "device.h"
#include <stddef.h>

// Implementation of device callback getter functions
device_read_func_t get_device_read_callback(struct device_s* dev) {
    if (dev) {
        const device_t* device_desc = (const device_t*)dev;
        if (device_desc && device_desc->r8) {
            return device_desc->r8;
        }
    }
    return NULL;
}

device_write_func_t get_device_write_callback(struct device_s* dev) {
    if (dev) {
        const device_t* device_desc = (const device_t*)dev;
        if (device_desc && device_desc->w8) {
            return device_desc->w8;
        }
    }
    return NULL;
}