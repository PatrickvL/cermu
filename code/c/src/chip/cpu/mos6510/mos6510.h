#ifndef MOS6510_H
#define MOS6510_H

#include "../../../core/device.h"
#include "../../../systems/c64/c64_bus.h"
#include <stdint.h>
#include <stdbool.h>

// Macro utilities for generating unique labels
#define CONCAT_IMPL(a, b) a ## b
#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define UNIQUE_LABEL(prefix) CONCAT(prefix, __LINE__)

// ============================================================================
// MOS 6510 CPU EMULATION - Cycle-accurate with direct threading
// ============================================================================

// CPU state structure
typedef struct {
    device_descriptor_t* desc; // Pointer to device descriptor (must be first)
      // Device attachments
    c64_bus_t* c64_bus; // Required for c64_non_cpu_cycle() TODO : Do that without referring to the c64 system
    
    // Internal CPU state for cycle-accurate emulation
    uint16_t address;   // Address for current instruction (used for both absolute and relative)
    
    // I/O Ports
    uint8_t io_port[2]; // 0:DDR, 1:Port

    // CPU Registers
    uint16_t pc;        // Program Counter
    uint8_t a;          // Accumulator
    uint8_t x;          // X Index Register
    uint8_t y;          // Y Index Register
    uint8_t sp;         // Stack Pointer
    uint8_t p;          // Processor Status Register
} mos6510_t;

// Universal instruction dispatch using function pointers
// (Works well on all compilers - performance difference with computed goto is minimal)
typedef void (*mos6510_opcode_handler_t)(mos6510_t* cpu_dev);

// Global instruction table
extern mos6510_opcode_handler_t mos6510_opcode_handlers[256];


// --- Interception support: replace handlers with stubs until next opcode ---
/**
 * Begin intercepting the next opcode fetch.  All 256 handlers will be
 * replaced with an internal stub that restores the original table on its
 * first invocation.
 */
void mos6510_start_intercept(void);

/**
 * Cancel interception and restore the original handler table immediately.
 */
void mos6510_stop_intercept(void);

// Interception support: monkey-patch opcode table with stubs until next opcode
void mos6510_start_intercept(void);
void mos6510_stop_intercept(void);

// MOS6510 Status Register Flags
#define FLAG_C  0x01    // Carry
#define FLAG_Z  0x02    // Zero
#define FLAG_I  0x04    // Interrupt Disable
#define FLAG_D  0x08    // Decimal Mode
#define FLAG_B  0x10    // Break Command
#define FLAG_U  0x20    // Unused (always 1)
#define FLAG_V  0x40    // Overflow
#define FLAG_N  0x80    // Negative

// ============================================================================
// I/O PORT EMULATION - Direct handling in CPU read/write cycles
// ============================================================================

// PLA functions
void switch_cpu_mode(uint8_t mode);

// Memory access functions
static inline uint8_t mos6510_read_cycle(mos6510_t* cpu_dev, uint16_t addr) {
    return c64_bus_read_cycle(cpu_dev->c64_bus, addr);
}

static inline void mos6510_write_cycle(mos6510_t* cpu_dev, uint16_t addr, uint8_t value) {
    c64_bus_write_cycle(cpu_dev->c64_bus, addr, value);
}

// Forward declaration for functions used in macros
void mos6510_interrupt_handler(mos6510_t* cpu_dev);
void c64_non_cpu_cycle(void* c64);  // c64_t* - forward declaration with opaque pointer

// CPU opcode dispatch function
static inline void mos6510_opcode_dispatch(mos6510_t* cpu, uint8_t opcode) {
    mos6510_opcode_handler_t handler = mos6510_opcode_handlers[opcode];
    handler(cpu);
}

// Shield off where the cpu control lines remos6581e (might we want to change this later)
#define CPU_CONTROL_LINES(cpu_dev) ((cpu_dev)->c64_bus->control_lines)

// CPU ready check - hardware accurate BA/RDY handling
#define CPU_READY(cpu_dev) ((CPU_CONTROL_LINES(cpu_dev) & RDY_LINE) != 0)

// Wait for CPU ready with automatic stall handling
#define CPU_INTRA_CYCLE(cpu_dev) do { \
    UNIQUE_LABEL(cpu_ready_stall): \
    if (!CPU_READY(cpu_dev)) { c64_non_cpu_cycle(cpu_dev->c64_bus->c64); goto UNIQUE_LABEL(cpu_ready_stall); } \
} while(0)

#define CPU_NEXT_INSTRUCTION_DISPATCH(cpu_dev) do { \
    uint8_t opcode = mos6510_read_cycle(cpu_dev, cpu_dev->pc++); \
    mos6510_opcode_dispatch(cpu_dev, opcode); \
} while(0)

#define CPU_NEXT_INSTRUCTION(cpu_dev) do { \
    if (unlikely(CPU_CONTROL_LINES(cpu_dev) & (IRQ_LINE | NMI_LINE))) { \
        mos6510_interrupt_handler(cpu_dev); \
        return; \
    } \
    CPU_INTRA_CYCLE(cpu_dev); \
    CPU_NEXT_INSTRUCTION_DISPATCH(cpu_dev); \
    return; \
} while(0)

#define CPU_OPCODE_FOOTER(cpu_dev) CPU_NEXT_INSTRUCTION(cpu_dev)

// Flag operations (inline for performance)
static inline void mos6510_set_flag(mos6510_t* cpu_dev, uint8_t flag, bool condition) {
    if (condition) cpu_dev->p |= flag;
    else cpu_dev->p &= ~flag;
}

static inline bool cpu_get_flag(mos6510_t* cpu_dev, uint8_t flag) {
    return (cpu_dev->p & flag) != 0;
}

static inline void mos6510_set_zn(mos6510_t* cpu_dev, uint8_t value) {
    mos6510_set_flag(cpu_dev, FLAG_Z, value == 0);
    mos6510_set_flag(cpu_dev, FLAG_N, value & 0x80);
}

// Stack operations
static inline void mos6510_push(mos6510_t* cpu_dev, uint8_t data) {
    mos6510_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, data);
    cpu_dev->sp--;
}

static inline uint8_t mos6510_pop(mos6510_t* cpu_dev) {
    cpu_dev->sp++;
    return mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
}

// BRK/IRQ common sequence - handles the interrupt setup portion
static inline void mos6510_interrupt_sequence(mos6510_t* cpu_dev, uint8_t status_flags, uint16_t vector_addr) {
    // Push PC and status unconditionally (skip RDY checks)
    mos6510_push(cpu_dev, (cpu_dev->pc >> 8) & 0xFF);
    mos6510_push(cpu_dev, cpu_dev->pc & 0xFF);
    mos6510_push(cpu_dev, status_flags);
    // Set interrupt disable
    cpu_dev->p |= FLAG_I;
    // Read vector low and high without RDY checks
    uint8_t pc_lo = mos6510_read_cycle(cpu_dev, vector_addr);
    uint8_t pc_hi = mos6510_read_cycle(cpu_dev, vector_addr + 1);
    cpu_dev->pc = (pc_hi << 8) | pc_lo;
    // Note : callers will dispatch the next instruction
}


// ============================================================================
// ADDRESSING MODE HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Immediate addressing - returns the immediate value
static inline uint8_t addr_imm(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    return mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
}

// Zero page addressing - sets address and returns fetched value
static inline uint8_t addr_zp(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    return mos6510_read_cycle(cpu_dev, cpu_dev->address);
}

// Zero page,X addressing - sets address and returns fetched value
static inline uint8_t addr_zpx(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t base = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, base); // Dummy read
    cpu_dev->address = (base + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    return mos6510_read_cycle(cpu_dev, cpu_dev->address);
}

// Zero page,Y addressing - sets address and returns fetched value
static inline uint8_t addr_zpy(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t base = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, base); // Dummy read
    cpu_dev->address = (base + cpu_dev->y) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    return mos6510_read_cycle(cpu_dev, cpu_dev->address);
}

// Absolute addressing - sets address and returns fetched value
static inline uint8_t addr_abs(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    return mos6510_read_cycle(cpu_dev, cpu_dev->address);
}

// Absolute,X addressing - sets address and returns fetched value
static inline uint8_t addr_absx(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu_dev->address = base + cpu_dev->x;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu_dev->address & 0xFF00)) {
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, base); // Dummy read
    }
    CPU_INTRA_CYCLE(cpu_dev);
    return mos6510_read_cycle(cpu_dev, cpu_dev->address);
}

// Absolute,Y addressing - sets address and returns fetched value
static inline uint8_t addr_absy(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu_dev->address = base + cpu_dev->y;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu_dev->address & 0xFF00)) {
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, base); // Dummy read
    }
    CPU_INTRA_CYCLE(cpu_dev);
    return mos6510_read_cycle(cpu_dev, cpu_dev->address);
}

// (Zero page,X) - Indexed Indirect addressing
static inline uint8_t addr_zpx_ind(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t base = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, base); // Dummy read
    uint8_t zp_addr = (base + cpu_dev->x) & 0xFF;
    
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, zp_addr);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    return mos6510_read_cycle(cpu_dev, cpu_dev->address);
}

// (Zero page),Y - Indirect Indexed addressing
static inline uint8_t addr_zp_ind_y(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t zp_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);

    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, zp_addr);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu_dev->address = base + cpu_dev->y;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu_dev->address & 0xFF00)) {
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, base); // Dummy read
    }
    CPU_INTRA_CYCLE(cpu_dev);
    return mos6510_read_cycle(cpu_dev, cpu_dev->address);
}

// ============================================================================
// CPU OPERATION HELPER FUNCTIONS (inline for performance)
// ============================================================================

// ADC - Add with Carry
static inline void op_adc(mos6510_t* cpu_dev, uint8_t value) {
    if (cpu_dev->p & FLAG_D) {
        // Decimal mode - BCD arithmetic
        uint8_t carry_in = (cpu_dev->p & FLAG_C) ? 1 : 0;

        // Split into low and high nibbles for BCD
        uint8_t a_low = cpu_dev->a & 0x0F;
        uint8_t a_high = (cpu_dev->a >> 4) & 0x0F;
        uint8_t v_low = value & 0x0F;
        uint8_t v_high = (value >> 4) & 0x0F;
        
        // Add low nibbles
        uint16_t low_sum = a_low + v_low + carry_in;
        if (low_sum > 9) {
            low_sum += 6;  // BCD adjustment
        }
        
        // Add high nibbles with carry from low
        uint16_t high_sum = a_high + v_high + (low_sum > 15 ? 1 : 0);
        if (high_sum > 9) {
            high_sum += 6;  // BCD adjustment
        }
        
        // Combine result
        uint8_t result = ((high_sum & 0x0F) << 4) | (low_sum & 0x0F);
        
        // Set flags
        cpu_dev->p &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);

        if (0 == (uint8_t)(cpu_dev->a + value + carry_in)) {
            cpu_dev->p |= FLAG_Z;
        } else if ((high_sum & 0x08) != 0) {
            cpu_dev->p |= FLAG_N;
        }

        if ((~(cpu_dev->a ^ value) & (cpu_dev->a ^ (high_sum << 4)) & 0x80) != 0) {
            cpu_dev->p |= FLAG_V;
        }

        if (high_sum > 15) {
            cpu_dev->p |= FLAG_C;
        }
        
        cpu_dev->a = result;
    } else {    
        uint16_t temp = cpu_dev->a + value + (cpu_get_flag(cpu_dev, FLAG_C) ? 1 : 0);
        mos6510_set_flag(cpu_dev, FLAG_C, temp > 255);
        mos6510_set_flag(cpu_dev, FLAG_V, (~(cpu_dev->a ^ value) & (cpu_dev->a ^ temp)) & 0x80);
        cpu_dev->a = temp & 0xFF;
        mos6510_set_zn(cpu_dev, cpu_dev->a);
    }
}

// AND - Logical AND
static inline void op_and(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a &= value;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
}

// ASL - Arithmetic Shift Left
static inline uint8_t op_asl(mos6510_t* cpu_dev, uint8_t value) {
    mos6510_set_flag(cpu_dev, FLAG_C, value & 0x80);
    value <<= 1;
    mos6510_set_zn(cpu_dev, value);
    return value;
}

// BIT - Bit Test
static inline void op_bit(mos6510_t* cpu_dev, uint8_t value) {
    mos6510_set_flag(cpu_dev, FLAG_Z, (cpu_dev->a & value) == 0);
    mos6510_set_flag(cpu_dev, FLAG_V, value & FLAG_V);
    mos6510_set_flag(cpu_dev, FLAG_N, value & FLAG_N);
}

// CMP - Compare
static inline void op_cmp(mos6510_t* cpu_dev, uint8_t value) {
    uint16_t temp = cpu_dev->a - value;
    mos6510_set_flag(cpu_dev, FLAG_C, cpu_dev->a >= value);
    mos6510_set_zn(cpu_dev, temp & 0xFF);
}

// CPX - Compare X Register
static inline void op_cpx(mos6510_t* cpu_dev, uint8_t value) {
    uint16_t temp = cpu_dev->x - value;
    mos6510_set_flag(cpu_dev, FLAG_C, cpu_dev->x >= value);
    mos6510_set_zn(cpu_dev, temp & 0xFF);
}

// CPY - Compare Y Register
static inline void op_cpy(mos6510_t* cpu_dev, uint8_t value) {
    uint16_t temp = cpu_dev->y - value;
    mos6510_set_flag(cpu_dev, FLAG_C, cpu_dev->y >= value);
    mos6510_set_zn(cpu_dev, temp & 0xFF);
}

// DEC - Decrement
static inline uint8_t op_dec(mos6510_t* cpu_dev, uint8_t value) {
    value--;
    mos6510_set_zn(cpu_dev, value);
    return value;
}

// EOR - Exclusive OR
static inline void op_eor(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a ^= value;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
}

// INC - Increment
static inline uint8_t op_inc(mos6510_t* cpu_dev, uint8_t value) {
    value++;
    mos6510_set_zn(cpu_dev, value);
    return value;
}

// LDA - Load Accumulator
static inline void op_lda(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a = value;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
}

// LDX - Load X Register
static inline void op_ldx(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->x = value;
    mos6510_set_zn(cpu_dev, cpu_dev->x);
}

// LDY - Load Y Register
static inline void op_ldy(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->y = value;
    mos6510_set_zn(cpu_dev, cpu_dev->y);
}

// LSR - Logical Shift Right
static inline uint8_t op_lsr(mos6510_t* cpu_dev, uint8_t value) {
    mos6510_set_flag(cpu_dev, FLAG_C, value & 0x01);
    value >>= 1;
    mos6510_set_zn(cpu_dev, value);
    return value;
}

// ORA - Logical Inclusive OR
static inline void op_ora(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a |= value;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
}

// ROL - Rotate Left
static inline uint8_t op_rol(mos6510_t* cpu_dev, uint8_t value) {
    uint8_t temp = (value << 1) | (cpu_get_flag(cpu_dev, FLAG_C) ? 1 : 0);
    mos6510_set_flag(cpu_dev, FLAG_C, value & 0x80);
    mos6510_set_zn(cpu_dev, temp);
    return temp;
}

// ROR - Rotate Right
static inline uint8_t op_ror(mos6510_t* cpu_dev, uint8_t value) {
    uint8_t temp = (value >> 1) | (cpu_get_flag(cpu_dev, FLAG_C) ? 0x80 : 0);
    mos6510_set_flag(cpu_dev, FLAG_C, value & 0x01);
    mos6510_set_zn(cpu_dev, temp);
    return temp;
}

// SBC - Subtract with Carry
static inline void op_sbc(mos6510_t* cpu_dev, uint8_t value) {
    uint16_t temp = cpu_dev->a - value - (cpu_get_flag(cpu_dev, FLAG_C) ? 0 : 1);
    mos6510_set_flag(cpu_dev, FLAG_C, temp < 0x100);
    mos6510_set_flag(cpu_dev, FLAG_V, ((cpu_dev->a ^ value) & (cpu_dev->a ^ temp)) & 0x80);
    cpu_dev->a = temp & 0xFF;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
}

// Generic arithmetic operation helper - combines addressing mode with operation
static inline void mos6510_arithmetic_helper(mos6510_t* cpu_dev, uint8_t (*addr_func)(mos6510_t*), void (*op_func)(mos6510_t*, uint8_t)) {
    uint8_t value = addr_func(cpu_dev);
    op_func(cpu_dev, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

static inline void mos6510_flag_clear_helper(mos6510_t* cpu_dev, uint8_t flag) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p &= ~flag;
    CPU_OPCODE_FOOTER(cpu_dev);
}

static inline void mos6510_flag_set_helper(mos6510_t* cpu_dev, uint8_t flag) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->p |= flag;
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Generic load operation helper - combines addressing mode with load operation
static inline void mos6510_load_helper(mos6510_t* cpu_dev, uint8_t (*addr_func)(mos6510_t*), void (*op_func)(mos6510_t*, uint8_t)) {
    uint8_t value = addr_func(cpu_dev);
    op_func(cpu_dev, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// ============================================================================
// STORE ADDRESS HELPER FUNCTIONS (inline for performance)  
// ============================================================================

// Zero page addressing for stores - sets address only
static inline void addr_zp_store(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
}

// Zero page,X addressing for stores - sets address only  
static inline void addr_zpx_store(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t base = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, base); // Dummy read
    cpu_dev->address = (base + cpu_dev->x) & 0xFF;
}

// Zero page,Y addressing for stores - sets address only
static inline void addr_zpy_store(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t base = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, base); // Dummy read
    cpu_dev->address = (base + cpu_dev->y) & 0xFF;
}

// Absolute addressing for stores - sets address only
static inline void addr_abs_store(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
}

// Absolute,X addressing for stores - sets address only (with dummy read)
static inline void addr_absx_store(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->x); // Dummy read
    cpu_dev->address += cpu_dev->x;
}

// Absolute,Y addressing for stores - sets address only (with dummy read)
static inline void addr_absy_store(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->y); // Dummy read
    cpu_dev->address += cpu_dev->y;
}

// (Zero page,X) addressing for stores - sets address only
static inline void addr_zpx_ind_store(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t base = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, base); // Dummy read
    uint8_t zp_addr = (base + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, zp_addr);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
}

// (Zero page),Y addressing for stores - sets address only (with dummy read)
static inline void addr_zp_ind_y_store(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t zp_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, zp_addr);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address + cpu_dev->y); // Dummy read
    cpu_dev->address += cpu_dev->y;
}

// Generic store operation helper - combines addressing mode with store value
static inline void mos6510_store_helper(mos6510_t* cpu_dev, void (*addr_func)(mos6510_t*), uint8_t value) {
    addr_func(cpu_dev);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// ============================================================================
// CONTROL FLOW HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Generic branch helper - handles all branch instruction logic
static inline void mos6510_branch_helper(mos6510_t* cpu_dev, bool condition) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t rel_addr = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    if (condition) {
        // Branch taken
        CPU_INTRA_CYCLE(cpu_dev);
        (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc); // Dummy read
        uint16_t new_pc = cpu_dev->pc + (int8_t)rel_addr;
        if ((cpu_dev->pc ^ new_pc) & 0xFF00) {
            // Page crossed - extra cycle
            CPU_INTRA_CYCLE(cpu_dev);
            (void)mos6510_read_cycle(cpu_dev, (cpu_dev->pc & 0xFF00) | (new_pc & 0xFF));
        }
        cpu_dev->pc = new_pc;
    }
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Stack push with timing control
static inline void mos6510_push_with_wait(mos6510_t* cpu_dev, uint8_t data) {
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, data);
    cpu_dev->sp--;
}

// Stack pop with timing control  
static inline uint8_t mos6510_pop_with_wait(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->sp++;
    return mos6510_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
}

// ============================================================================
// READ-MODIFY-WRITE HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Accumulator read-modify-write operations (2 cycles)
static inline void mos6510_rmw_accumulator(mos6510_t* cpu_dev, uint8_t (*operation)(mos6510_t*, uint8_t)) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    cpu_dev->a = operation(cpu_dev, cpu_dev->a);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Zero page read-modify-write operations
static inline void mos6510_rmw_zero_page(mos6510_t* cpu_dev, uint8_t (*operation)(mos6510_t*, uint8_t)) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value); // Write original value
    value = operation(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Zero page,X read-modify-write operations
static inline void mos6510_rmw_zero_page_x(mos6510_t* cpu_dev, uint8_t (*operation)(mos6510_t*, uint8_t)) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address); // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value); // Write original value
    value = operation(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Absolute read-modify-write operations
static inline void mos6510_rmw_absolute(mos6510_t* cpu_dev, uint8_t (*operation)(mos6510_t*, uint8_t)) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value); // Write original value
    value = operation(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Absolute,X read-modify-write operations
static inline void mos6510_rmw_absolute_x(mos6510_t* cpu_dev, uint8_t (*operation)(mos6510_t*, uint8_t)) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, (cpu_dev->address & 0xFF00) | ((cpu_dev->address + cpu_dev->x) & 0xFF)); // Dummy read
    cpu_dev->address += cpu_dev->x;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t value = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value); // Write original value
    value = operation(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// ============================================================================
// REGISTER OPERATION HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Register increment/decrement with flags
static inline void mos6510_register_inc_dec(mos6510_t* cpu_dev, uint8_t* reg, int delta) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    *reg += (uint8_t)delta;
    mos6510_set_zn(cpu_dev, *reg);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Register transfer with flags (when flags should be set)
static inline void mos6510_register_transfer_with_flags(mos6510_t* cpu_dev, uint8_t* dest, uint8_t src) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    *dest = src;
    mos6510_set_zn(cpu_dev, *dest);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Register transfer without flags (e.g., TXS)
static inline void mos6510_register_transfer_no_flags(mos6510_t* cpu_dev, uint8_t* dest, uint8_t src) {
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->pc);  // Dummy read
    *dest = src;
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Memory increment/decrement operations
static inline void mos6510_memory_inc_dec(mos6510_t* cpu_dev, uint8_t fetched, int delta) {
    uint8_t result = fetched + (uint8_t)delta;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, result);
    mos6510_set_zn(cpu_dev, result);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// ============================================================================
// ILLEGAL INSTRUCTION HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Read-modify-write + register operation combo (SLO, RLA, RRA, SRE)
static inline void mos6510_illegal_rmw_combo(mos6510_t* cpu_dev, uint8_t value, 
                                          uint8_t (*rmw_op)(mos6510_t*, uint8_t),
                                          void (*reg_op)(mos6510_t*, uint8_t)) {
    uint8_t result = rmw_op(cpu_dev, value);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, result);
    reg_op(cpu_dev, result);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// INC/DEC + register operation combo (DCP, ISC)
static inline void mos6510_illegal_inc_dec_combo(mos6510_t* cpu_dev, uint8_t value, 
                                              int delta, void (*reg_op)(mos6510_t*, uint8_t)) {
    value += (uint8_t)delta;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    reg_op(cpu_dev, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Load both A and X (LAX variants)
static inline void mos6510_load_a_and_x(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a = value;
    cpu_dev->x = value;
    mos6510_set_zn(cpu_dev, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Store A & X (SAX variants)
static inline void mos6510_store_a_and_x(mos6510_t* cpu_dev, void (*addr_func)(mos6510_t*, uint8_t)) {
    addr_func(cpu_dev, cpu_dev->a & cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Complex store with high byte manipulation (AHX, SHX, SHY, TAS)
static inline void mos6510_complex_store(mos6510_t* cpu_dev, uint8_t value) {
    value &= ((cpu_dev->address >> 8) + 1);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// Immediate mode accumulator operations (ALR, ANC, ARR, AXS, XAA)
static inline void mos6510_immediate_accumulator_op(mos6510_t* cpu_dev, void (*operation)(mos6510_t*, uint8_t)) {
    uint8_t value = addr_imm(cpu_dev);
    operation(cpu_dev, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// ============================================================================
// ILLEGAL INSTRUCTION SPECIFIC OPERATIONS (inline for performance)
// ============================================================================

// ALR operation: AND then LSR
static inline void op_alr(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a &= value;
    mos6510_set_flag(cpu_dev, FLAG_C, cpu_dev->a & 0x01);
    cpu_dev->a >>= 1;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
}

// ANC operation: AND then copy N to C
static inline void op_anc(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a &= value;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    mos6510_set_flag(cpu_dev, FLAG_C, cpu_dev->a & 0x80);
}

// ARR operation: AND then ROR with special V flag behavior
static inline void op_arr(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a &= value;
    uint8_t old_carry = cpu_get_flag(cpu_dev, FLAG_C) ? 1 : 0;
    mos6510_set_flag(cpu_dev, FLAG_C, cpu_dev->a & 0x01);
    cpu_dev->a = (cpu_dev->a >> 1) | (old_carry << 7);
    mos6510_set_zn(cpu_dev, cpu_dev->a);
    // V flag behavior is complex for ARR
    mos6510_set_flag(cpu_dev, FLAG_V, ((cpu_dev->a >> 6) ^ (cpu_dev->a >> 5)) & 1);
}

// AXS operation: (A & X) - immediate, store in X
static inline void op_axs(mos6510_t* cpu_dev, uint8_t value) {
    uint8_t temp = cpu_dev->a & cpu_dev->x;
    uint16_t result = temp - value;
    mos6510_set_flag(cpu_dev, FLAG_C, result < 0x100);
    cpu_dev->x = result & 0xFF;
    mos6510_set_zn(cpu_dev, cpu_dev->x);
}

// XAA operation: Transfer X to A, then AND with immediate
static inline void op_xaa(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a = cpu_dev->x;
    cpu_dev->a &= value;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
}

// SLO register operation: ORA with result
static inline void op_slo_reg(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a |= value;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
}

// RLA register operation: AND with result
static inline void op_rla_reg(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a &= value;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
}

// SRE register operation: EOR with result
static inline void op_sre_reg(mos6510_t* cpu_dev, uint8_t value) {
    cpu_dev->a ^= value;
    mos6510_set_zn(cpu_dev, cpu_dev->a);
}

// CPU core functions
void mos6510_init(mos6510_t* cpu_dev);
void mos6510_reset(mos6510_t* cpu_dev);
bool mos6510_step(mos6510_t* cpu_dev);
void mos6510_execute(mos6510_t* cpu_dev);
bool mos6510_is_intercepting(void);
void mos6510_nmi(mos6510_t* cpu_dev);
void mos6510_irq(mos6510_t* cpu_dev, uint8_t status);

// Device attachments
void mos6510_attach_bus(mos6510_t* cpu_dev, c64_bus_t* bus_state);

#endif // MOS6510_H