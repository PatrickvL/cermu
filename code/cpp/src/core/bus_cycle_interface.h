#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Generic bus and cycle operations interface for system components.
 * Provides unified access to memory operations and system cycle advancement.
 * This allows components (especially CPUs) to be decoupled from specific
 * bus implementations while maintaining cycle-accurate timing.
 */
typedef struct {
    /**
     * User-provided context pointer passed to all callback functions.
     * Can point to the bus implementation, system state, or any other data.
     */
    void* context;

    /**
     * Callback to perform a bus read operation.
     * Should advance other system components (VIC, CIA, SID, etc.) by one cycle.
     * 
     * @param context User-provided context pointer
     * @param address 16-bit address to read from
     * @return Data read from the specified address
     */
    uint8_t (*bus_read_cycle)(void* context, uint16_t address);

    /**
     * Callback to perform a bus write operation.
     * Should advance other system components (VIC, CIA, SID, etc.) by one cycle.
     * 
     * @param context User-provided context pointer
     * @param address 16-bit address to write to
     * @param value 8-bit value to write
     */
    void (*bus_write_cycle)(void* context, uint16_t address, uint8_t value);

} bus_cycle_ops_t;


