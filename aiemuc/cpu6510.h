#ifndef CPU6510_H
#define CPU6510_H

#include "c64.h"
#include "bus.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// MOS 6510 CPU EMULATION - Cycle-accurate with direct threading
// ============================================================================

// CPU state structure
typedef struct {
    // CPU Registers  
    uint16_t pc;        // Program Counter
    uint8_t a;          // Accumulator
    uint8_t x;          // X Index Register
    uint8_t y;          // Y Index Register
    uint8_t sp;         // Stack Pointer
    uint8_t p;          // Processor Status Register

    // Internal CPU state for cycle-accurate emulation
    uint8_t opcode;     // Current instruction opcode
    uint8_t lo, hi;     // Address calculation helpers
    uint16_t addr_abs;  // Absolute address for current instruction
    uint8_t fetched;    // Fetched data for current instruction
    uint8_t temp;       // Temporary storage
    //uint8_t cycles;     // Remaining cycles for current instruction
    uint64_t total_cycles; // Total CPU cycles executed
    //bool page_crossed;  // Page boundary crossed flag
    
    // Direct threading state
    //const void* next_cycle;  // Next cycle function pointer   
} cpu6510_state_t;

// Global CPU state
extern cpu6510_state_t cpu;

// MOS6510 Status Register Flags
#define FLAG_C  0x01    // Carry
#define FLAG_Z  0x02    // Zero
#define FLAG_I  0x04    // Interrupt Disable
#define FLAG_D  0x08    // Decimal Mode
#define FLAG_B  0x10    // Break Command
#define FLAG_U  0x20    // Unused (always 1)
#define FLAG_V  0x40    // Overflow
#define FLAG_N  0x80    // Negative

// Universal instruction dispatch using function pointers
// (Works well on all compilers - performance difference with computed goto is minimal)
typedef void (*instruction_func_t)(void);
#define NEXT_INSTRUCTION(fetch_label) do { \
    if (unlikely(bus_state.control_lines & (IRQ_LINE | NMI_LINE))) { \
        handle_interrupt_func(); \
        return; \
    } \
    WAIT_READY_THEN_READ(cpu.pc++, fetch_label); \
    cpu.opcode = bus_state.data; \
    instruction_table[cpu.opcode](); \
    return; \
} while(0)

// Flag operations (inline for performance)
static inline void cpu_set_flag(uint8_t flag, bool condition) {
    if (condition) cpu.p |= flag;
    else cpu.p &= ~flag;
}

static inline bool cpu_get_flag(uint8_t flag) {
    return (cpu.p & flag) != 0;
}

static inline void cpu_set_zn(uint8_t value) {
    cpu_set_flag(FLAG_Z, value == 0);
    cpu_set_flag(FLAG_N, value & 0x80);
}

// Stack operations
static inline void cpu_push(uint8_t data) {
    cpu_write_cycle(0x0100 + cpu.sp, data);
    cpu.sp--;
}

static inline uint8_t cpu_pop(void) {
    cpu.sp++;
    cpu_read_cycle(0x0100 + cpu.sp);
    return bus_state.data;
}

// CPU core functions
void cpu6510_init(void);
void cpu6510_reset(void);
bool cpu6510_step(void);
void cpu6510_execute(void);
void cpu6510_irq(void);
void cpu6510_nmi(void);

// Instruction setup
void cpu6510_setup_opcode_table(void);

#endif // CPU6510_H