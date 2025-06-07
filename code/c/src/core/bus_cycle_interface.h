#ifndef BUS_CYCLE_OPS_H
#define BUS_CYCLE_OPS_H

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
     * Callback to perform a bus read operation.
     * Should only handle memory access, not advance system components.
     * 
     * @param context User-provided context pointer
     * @param address 16-bit address to read from
     * @return Data read from the specified address
     */
    uint8_t (*bus_read)(void* context, uint16_t address);

    /**
     * Callback to perform a bus write operation.
     * Should only handle memory access, not advance system components.
     * 
     * @param context User-provided context pointer
     * @param address 16-bit address to write to
     * @param value 8-bit value to write
     */
    void (*bus_write)(void* context, uint16_t address, uint8_t value);

    /**
     * Callback to execute one non-CPU cycle tick.
     * Called separately after bus operations and during RDY stall handling.
     * Should advance other system components (VIC, CIA, SID, etc.) by one cycle.
     * 
     * @param context User-provided context pointer
     */
    void (*cycle_tick)(void* context);

    /**
     * User-provided context pointer passed to all callback functions.
     * Can point to the bus implementation, system state, or any other data.
     */
    void* context;
} bus_cycle_ops_t;

#endif // BUS_CYCLE_OPS_H
