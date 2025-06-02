#ifndef SYSTEM_H
#define SYSTEM_H

#include "aiemuc.h"
#include "device.h"

// Callback types
typedef struct {
    device_read_func_t func;
    void* context;
} read_callback_t;

typedef struct {
    device_write_func_t func;
    void* context;
} write_callback_t;

// Generic 8-bit system
typedef struct {
    device_entry_t devices[16];
    uint8_t device_count;
} system_8bit_t;


#endif // SYSTEM_H
