#include "ioport.h"
#include <string.h>

// ============================================================================
// I/O PORT IMPLEMENTATION
// ============================================================================

void ioport_init(ioport_t* port, uint8_t port_index, uint8_t width,
                 uint32_t initial_ddr, uint32_t initial_data, uint32_t initial_bus) {
    if (!port || width == 0 || width > 32) return;
    
    // Initialize the structure using C++ initialization
    *port = ioport_t{};
    
    // Set port configuration
    port->port_index = port_index;
    port->width = width;
    port->pin_mask = (width == 32) ? 0xFFFFFFFF : ((1U << width) - 1);
    
    // Initialize port registers (masked to implemented bits)
    port->ddr = initial_ddr & port->pin_mask;
    port->data = initial_data & port->pin_mask;
    port->last_bus = initial_bus;
}

void ioport_attach_interface(ioport_t* port, ioport_interface_t* interface) {
    if (!port || !interface) return;
    
    // Copy the interface structure
    port->interface = *interface;
}

uint32_t ioport_read(ioport_t* port, uint8_t address, uint32_t bus_data) {
    if (!port) return 0;
    
    // Update floating bus state
    port->last_bus = bus_data;
    
    switch (address) {
        case 0: // DDR register
            return port->ddr & port->pin_mask;
            
        case 1: { // PORT register
            uint32_t result = 0;
            
            // Get external input pin states if callback is available
            uint32_t external_pins = 0;
            if (port->interface.read_external_pins) {
                external_pins = port->interface.read_external_pins(
                    port->interface.context, port->port_index) & port->pin_mask;
            }
            
            // Build result bit by bit for optimal performance
            uint32_t ddr = port->ddr & port->pin_mask;
            uint32_t data = port->data & port->pin_mask;
            uint32_t mask = port->pin_mask;
            
            // Optimized bit mixing using bit manipulation tricks
            // For each bit: if DDR=1 use DATA, if DDR=0 use external input, floating uses last_bus
            uint32_t output_bits = data & ddr;                          // Output bits (DDR=1)
            uint32_t input_bits = external_pins & (~ddr) & mask;        // Input bits (DDR=0, pin exists)
            uint32_t floating_bits = port->last_bus & (~mask);          // Floating bits (pin doesn't exist)
            
            result = output_bits | input_bits | floating_bits;
            return result;
        }
        
        default:
            // Unknown address - return floating bus data
            return bus_data;
    }
}

void ioport_write(ioport_t* port, uint8_t address, uint32_t value, uint32_t bus_data) {
    if (!port) return;
    
    // Update floating bus state
    port->last_bus = bus_data;
    
    switch (address) {
        case 0: // DDR register
            {
                uint32_t old_ddr = port->ddr;
                port->ddr = value & port->pin_mask;
                
                // Notify of output pin changes if DDR changed and callback exists
                if (old_ddr != port->ddr && port->interface.output_pins_changed) {
                    port->interface.output_pins_changed(
                        port->interface.context, port->port_index, 
                        port->data & port->pin_mask, port->ddr);
                }
            }
            break;
            
        case 1: // PORT register
            {
                uint32_t old_data = port->data;
                port->data = value & port->pin_mask;
                
                // Notify of output pin changes if data changed and callback exists
                if (old_data != port->data && port->interface.output_pins_changed) {
                    port->interface.output_pins_changed(
                        port->interface.context, port->port_index, 
                        port->data, port->ddr);
                }
            }
            break;
            
        default:
            // Unknown address - ignore write
            break;
    }
}

uint32_t ioport_get_output_pins(const ioport_t* port) {
    if (!port) return 0;
    
    // Return only the bits that are configured as outputs
    return (port->data & port->ddr) & port->pin_mask;
}