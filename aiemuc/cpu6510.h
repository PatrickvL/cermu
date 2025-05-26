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
    uint8_t addr_rel;   // Relative address for branch instructions
    uint8_t fetched;    // Fetched data for current instruction
    uint8_t temp;       // Temporary storage
    //uint8_t cycles;     // Remaining cycles for current instruction
    uint64_t total_cycles; // Total CPU cycles executed
    //bool page_crossed;  // Page boundary crossed flag
    
    // Direct threading state
    //const void* next_cycle;  // Next cycle function pointer   
} cpu6510_state_t;

// Universal instruction dispatch using function pointers
// (Works well on all compilers - performance difference with computed goto is minimal)
typedef void (*instruction_func_t)(void);

// Global CPU state
extern cpu6510_state_t cpu;

// Global instruction table
extern instruction_func_t instruction_table[256];

// MOS6510 Status Register Flags
#define FLAG_C  0x01    // Carry
#define FLAG_Z  0x02    // Zero
#define FLAG_I  0x04    // Interrupt Disable
#define FLAG_D  0x08    // Decimal Mode
#define FLAG_B  0x10    // Break Command
#define FLAG_U  0x20    // Unused (always 1)
#define FLAG_V  0x40    // Overflow
#define FLAG_N  0x80    // Negative

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

// ============================================================================
// CPU OPERATION HELPER FUNCTIONS (inline for performance)
// ============================================================================

// ADC - Add with Carry
static inline void op_adc(uint8_t value) {
    uint16_t temp = cpu.a + value + (cpu_get_flag(FLAG_C) ? 1 : 0);
    cpu_set_flag(FLAG_C, temp > 255);
    cpu_set_flag(FLAG_V, (~(cpu.a ^ value) & (cpu.a ^ temp)) & 0x80);
    cpu.a = temp & 0xFF;
    cpu_set_zn(cpu.a);
}

// AND - Logical AND
static inline void op_and(uint8_t value) {
    cpu.a &= value;
    cpu_set_zn(cpu.a);
}

// ASL - Arithmetic Shift Left
static inline uint8_t op_asl(uint8_t value) {
    cpu_set_flag(FLAG_C, value & 0x80);
    value <<= 1;
    cpu_set_zn(value);
    return value;
}

// BIT - Bit Test
static inline void op_bit(uint8_t value) {
    cpu_set_flag(FLAG_Z, (cpu.a & value) == 0);
    cpu_set_flag(FLAG_V, value & FLAG_V);
    cpu_set_flag(FLAG_N, value & FLAG_N);
}

// CMP - Compare
static inline void op_cmp(uint8_t value) {
    uint16_t temp = cpu.a - value;
    cpu_set_flag(FLAG_C, cpu.a >= value);
    cpu_set_zn(temp & 0xFF);
}

// CPX - Compare X Register
static inline void op_cpx(uint8_t value) {
    uint16_t temp = cpu.x - value;
    cpu_set_flag(FLAG_C, cpu.x >= value);
    cpu_set_zn(temp & 0xFF);
}

// CPY - Compare Y Register
static inline void op_cpy(uint8_t value) {
    uint16_t temp = cpu.y - value;
    cpu_set_flag(FLAG_C, cpu.y >= value);
    cpu_set_zn(temp & 0xFF);
}

// DEC - Decrement
static inline uint8_t op_dec(uint8_t value) {
    value--;
    cpu_set_zn(value);
    return value;
}

// EOR - Exclusive OR
static inline void op_eor(uint8_t value) {
    cpu.a ^= value;
    cpu_set_zn(cpu.a);
}

// INC - Increment
static inline uint8_t op_inc(uint8_t value) {
    value++;
    cpu_set_zn(value);
    return value;
}

// LDA - Load Accumulator
static inline void op_lda(uint8_t value) {
    cpu.a = value;
    cpu_set_zn(cpu.a);
}

// LDX - Load X Register
static inline void op_ldx(uint8_t value) {
    cpu.x = value;
    cpu_set_zn(cpu.x);
}

// LDY - Load Y Register
static inline void op_ldy(uint8_t value) {
    cpu.y = value;
    cpu_set_zn(cpu.y);
}

// LSR - Logical Shift Right
static inline uint8_t op_lsr(uint8_t value) {
    cpu_set_flag(FLAG_C, value & 0x01);
    value >>= 1;
    cpu_set_zn(value);
    return value;
}

// ORA - Logical Inclusive OR
static inline void op_ora(uint8_t value) {
    cpu.a |= value;
    cpu_set_zn(cpu.a);
}

// ROL - Rotate Left
static inline uint8_t op_rol(uint8_t value) {
    uint8_t temp = (value << 1) | (cpu_get_flag(FLAG_C) ? 1 : 0);
    cpu_set_flag(FLAG_C, value & 0x80);
    cpu_set_zn(temp);
    return temp;
}

// ROR - Rotate Right
static inline uint8_t op_ror(uint8_t value) {
    uint8_t temp = (value >> 1) | (cpu_get_flag(FLAG_C) ? 0x80 : 0);
    cpu_set_flag(FLAG_C, value & 0x01);
    cpu_set_zn(temp);
    return temp;
}

// SBC - Subtract with Carry
static inline void op_sbc(uint8_t value) {
    uint16_t temp = cpu.a - value - (cpu_get_flag(FLAG_C) ? 0 : 1);
    cpu_set_flag(FLAG_C, temp < 0x100);
    cpu_set_flag(FLAG_V, ((cpu.a ^ value) & (cpu.a ^ temp)) & 0x80);
    cpu.a = temp & 0xFF;
    cpu_set_zn(cpu.a);
}

// Forward declarations for functions defined in main file
void handle_interrupt_func(void);

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