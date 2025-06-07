#ifndef C64_BUS_ADAPTER_H
#define C64_BUS_ADAPTER_H

#include "../../core/bus_cycle_interface.h"
#include "../../chip/cpu/mos6510/mos6510_io_interface.h"
#include "../../core/control_lines_interface.h"
#include "c64_bus.h"

/**
 * Creates a bus interface adapter for the existing C64 bus.
 * This allows the C64 bus to work with the refactored MOS6510 CPU.
 * 
 * @param c64_bus Pointer to the existing C64 bus implementation
 * @return Bus interface structure configured for the C64 bus
 */
bus_cycle_ops_t c64_bus_create_adapter(c64_bus_t* c64_bus);

/**
 * Creates a control lines interface adapter for the existing C64 bus.
 * This allows any chip to access the shared control lines.
 * 
 * @param c64_bus Pointer to the existing C64 bus implementation
 * @return Control lines interface structure configured for the C64 bus
 */
control_lines_interface_t c64_control_lines_create_adapter(c64_bus_t* c64_bus);

/**
 * Creates an I/O port interface adapter for the MOS6510 CPU's built-in I/O ports.
 * This handles the CPU's I/O ports at addresses $0000 and $0001.
 * 
 * @param c64_bus Pointer to the existing C64 bus implementation
 * @return I/O port interface structure configured for the C64 system
 */
mos6510_io_port_interface_t c64_io_port_create_adapter(c64_bus_t* c64_bus);

#endif // C64_BUS_ADAPTER_H
