#pragma once


#include <stdint.h>
#include <stdbool.h>

// External pin read callback - called when reading input pins
// Returns the actual state of external pins (hardware inputs)
// Only the bits specified by pin_mask are valid in the returned value
typedef uint32_t (*ioport_read_pins_t)(void* context, uint8_t port_index);

// Output pins change callback - called when output pins change
// Notifies external hardware of output pin changes
// Only the bits specified by DDR are outputs, others are floating/inputs
typedef void (*ioport_pins_changed_t)(void* context, uint8_t port_index, uint32_t new_value, uint32_t ddr);

// I/O Port Interface for system integration
typedef struct {
    void* context;                        // User context for callbacks
    ioport_read_pins_t read_external_pins; // Read external pin states
    ioport_pins_changed_t output_pins_changed; // Notify of output changes
} ioport_interface_t;

// I/O Port Device
// This represents a generic microprocessor I/O port with data direction control
// Supports 1-32 bits with floating bus behavior for unused pins
typedef struct {
    // Port registers (32-bit for maximum flexibility)
    uint32_t ddr;        // Data Direction Register (1=output, 0=input)
    uint32_t data;       // Port Data Register (output values)
    uint32_t last_bus;   // Last bus state for floating pins (bus retention)
    
    // External interface for system integration
    ioport_interface_t interface;
    
    // Port configuration
    uint8_t port_index;  // Port number (0, 1, etc.) for multi-port devices
    uint8_t width;       // Number of implemented bits (1-32)
    uint32_t pin_mask;   // Mask of implemented pins (derived from width)
} ioport_t;

// ============================================================================
// I/O PORT FUNCTIONS
// ============================================================================

/**
 * Initialize an I/O port device.
 * 
 * @param port Pointer to the I/O port structure
 * @param port_index Port number (0, 1, etc.)
 * @param width Number of implemented bits (1-32)
 * @param initial_ddr Initial Data Direction Register value
 * @param initial_data Initial Port Data Register value
 * @param initial_bus Initial bus state for floating pins
 */
void ioport_init(ioport_t* port, uint8_t port_index, uint8_t width,
                 uint32_t initial_ddr, uint32_t initial_data, uint32_t initial_bus);

/**
 * Attach external interface to the I/O port.
 * 
 * @param port Pointer to the I/O port structure
 * @param interface Pointer to the interface structure
 */
void ioport_attach_interface(ioport_t* port, ioport_interface_t* interface);

/**
 * Read from the I/O port (simulates CPU reading the port).
 * - If reading DDR: returns Data Direction Register
 * - If reading PORT: returns combination of output data, external input pins, and floating bus data
 * 
 * @param port Pointer to the I/O port structure
 * @param address Address within port (0=DDR, 1=DATA, etc.)
 * @param bus_data Current bus data state (for floating pins)
 * @return The value read from the port
 */
uint32_t ioport_read(ioport_t* port, uint8_t address, uint32_t bus_data);

/**
 * Write to the I/O port (simulates CPU writing to the port).
 * - If writing DDR: updates Data Direction Register
 * - If writing PORT: updates Port Data Register and notifies external hardware
 * 
 * @param port Pointer to the I/O port structure
 * @param address Address within port (0=DDR, 1=DATA, etc.)
 * @param value Value to write
 * @param bus_data Current bus data state (for floating pins retention)
 */
void ioport_write(ioport_t* port, uint8_t address, uint32_t value, uint32_t bus_data);

/**
 * Get the current effective output value of the port.
 * This is the value that external hardware would see on the output pins.
 * 
 * @param port Pointer to the I/O port structure
 * @return Current output pin states (considering DDR)
 */
uint32_t ioport_get_output_pins(const ioport_t* port);

/**
 * Get the current Data Direction Register value.
 * 
 * @param port Pointer to the I/O port structure
 * @return Current DDR value
 */
static inline uint32_t ioport_get_ddr(const ioport_t* port) {
    return port->ddr & port->pin_mask;
}

/**
 * Get the current Port Data Register value.
 * 
 * @param port Pointer to the I/O port structure
 * @return Current data register value
 */
static inline uint32_t ioport_get_data(const ioport_t* port) {
    return port->data & port->pin_mask;
}

/**
 * Get the port width (number of implemented bits).
 * 
 * @param port Pointer to the I/O port structure
 * @return Number of implemented bits (1-32)
 */
static inline uint8_t ioport_get_width(const ioport_t* port) {
    return port->width;
}

/**
 * Update the floating bus state (for external bus state changes).
 * This should be called when the system bus state changes to maintain
 * proper floating pin behavior.
 * 
 * @param port Pointer to the I/O port structure
 * @param bus_data New bus data state
 */
static inline void ioport_update_bus_state(ioport_t* port, uint32_t bus_data) {
    port->last_bus = bus_data;
}

