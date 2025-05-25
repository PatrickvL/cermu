#ifndef CPU_H
#define CPU_H

#include "c64.h"

// ============================================================================
// CPU STATE - 6502 registers and state
// ============================================================================
extern uint16_t cpu_pc;    // Program counter
extern uint8_t cpu_a;      // Accumulator
extern uint8_t cpu_sp, cpu_p; // Other registers
extern uint8_t opcode;
extern uint16_t addr_temp;

// CPU ready check - hardware accurate BA/RDY handling
#define CPU_READY() ((bus_state.bus_control & RDY_LINE) != 0)

// Wait for CPU ready with automatic stall handling
#define WAIT_READY_THEN_READ(addr, label) do { \
    label: \
    if (!CPU_READY()) { bus_cycle(); goto label; } \
    cpu_read_cycle(addr); \
} while(0)

// Direct threading dispatch macro
#define NEXT_INSTRUCTION(fetch_label) do { \
    if (unlikely(bus_state.control_lines & (IRQ_LINE | NMI_LINE))) { \
        goto handle_interrupt; \
    } \
    WAIT_READY_THEN_READ(cpu_pc++, fetch_label); \
    opcode = bus_state.data; \
    goto *instruction_table[opcode]; \
} while(0)

// CPU functions
void cpu_execute(void);

#endif // CPU_H