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
    uint8_t* handler_table;
    alignas(64) read_callback_t read_callbacks[16];
    alignas(64) write_callback_t write_callbacks[16];
    alignas(64) uint8_t chip_select_map[32][256];
} system_8bit_t;


#endif SYSTEM_H
