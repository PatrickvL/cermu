#ifndef MOS6510_IO_INTERFACE_H
#define MOS6510_IO_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration (defined in mos6510.h)
struct mos6510_s;

/**
 * MOS6510 I/O Port Interface for CPU built-in I/O ports at $0000/$0001.
 * 
 * The MOS6510 CPU has two built-in I/O ports:
 * - $0000: DDR (Data Direction Register) - controls direction of each bit
 * - $0001: Port Data - actual I/O port data
 * 
 * This interface allows the system to provide external data for input pins
 * and be notified when output pins change values.
 */
typedef struct {
    /**
     * Callback to read external data lines for input pins.
     * Called when CPU reads from port $0001 to get external pin values.
     * 
     * The CPU will apply DDR masking internally:
     * - Bits set as output (DDR=1) will use internal port data
     * - Bits set as input (DDR=0) will use value returned by this callback
     * 
     * @param context User-provided context pointer
     * @return 8-bit value representing external pin states
     */
    uint8_t (*read_external_pins)(void* context);

    /**
     * Callback to notify system when output pins change.
     * Called when CPU writes to port $0001 or $0000 (DDR).
     * 
     * The effective_output value is already DDR-masked:
     * - Bits set as output (DDR=1) contain the actual output values
     * - Bits set as input (DDR=0) are set to 0
     * 
     * @param context User-provided context pointer
     * @param ddr Current Data Direction Register value (1=output, 0=input)
     * @param port_data Current Port Data Register value
     * @param effective_output DDR-masked output: (port_data & ddr)
     */
    void (*output_pins_changed)(void* context, uint8_t ddr, uint8_t port_data, uint8_t effective_output);

    /**
     * User-provided context pointer passed to all callback functions.
     * Can point to the system state or any other data needed by callbacks.
     */
    void* context;
} mos6510_io_port_interface_t;

#endif // MOS6510_IO_INTERFACE_H
