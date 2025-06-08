#ifndef MOS6510_H
#define MOS6510_H

#include "../../../core/chip.h"
#include "../../../core/bus_cycle_interface.h"
#include "../../../core/control_lines_interface.h"
#include "../../../core/system_lines.h"
#include "../../../core/system.h"
#include "mos6510_pins.h"
#include "mos6510_io_interface.h"
#include <stdint.h>
#include <stdbool.h>

// Forward declaration to resolve circular dependencies
typedef struct mos6510_s mos6510_t;

// Macro utilities for generating unique labels
#define CONCAT_IMPL(a, b) a ## b
#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define UNIQUE_LABEL(prefix) CONCAT(prefix, __LINE__)

// ============================================================================
// MOS 6510 CPU EMULATION - Cycle-accurate with direct threading
// ============================================================================

// CPU state structure
struct mos6510_s {
    chip_descriptor_t* desc; // Pointer to chip descriptor (must be first)
      // === PERFORMANCE-OPTIMIZED INTERFACE STORAGE ===
    // Store interface structs by value for zero-indirection access    // Bus interface (stored by value for optimal performance)
    bus_cycle_ops_t bus_interface;
    
    // Control lines interface (stored by value for optimal performance) 
    control_lines_interface_t control_interface;
      // I/O port interface (stored by value for optimal performance)
    mos6510_io_port_interface_t io_interface;
    
    // === SHARED STATE POINTERS ===
    // These CANNOT be copied - must remain as pointers to shared system state
    system_lines_t* system_lines;  // Shared system-wide line state
      // === DIRECT RAM ACCESS (for zero page $0002-$00FF) ===
    // Direct RAM accessors to avoid circular dependency with bus interface
    access_callback_t ram_access;  // Consolidated RAM access interface
    
    // === CPU INTERNAL STATE ===
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
};

// Universal instruction dispatch using function pointers
// (Works well on all compilers - performance difference with computed goto is minimal)
typedef void (*mos6510_opcode_handler_t)(mos6510_t* cpu);

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

// ============================================================================
// MOS6510 ZERO PAGE I/O PORT ACCESSORS
// ============================================================================

// Compute effective port output: outputs from Data when DDR=1, else external data
static inline uint8_t mos6510_io_mask(mos6510_t* cpu, uint8_t value)
{
    uint8_t ddr = cpu->io_port[0];
    uint8_t data = cpu->io_port[1];
    return (data & ddr) | (value & ~ddr);
}

// MOS6510 zero page I/O port read (addresses $0000/$0001)
static inline uint8_t mos6510_ioport_read(mos6510_t* cpu, uint16_t addr) {
    if (addr == 0) {
        // Return Data Direction Register
        return cpu->io_port[0];
    } else {
        // Return port: outputs defined by DDR bits, inputs from external pins
        uint8_t external = cpu->io_interface.read_external_pins(cpu->io_interface.context);
        return mos6510_io_mask(cpu, external);
    }
}

// MOS6510 zero page I/O port write (addresses $0000/$0001)
static inline void mos6510_ioport_write(mos6510_t* cpu, uint16_t addr, uint8_t value) {
    // Update Data Direction / Data register
    cpu->io_port[addr] = value;
    
    // Notify system of output pin changes
    uint8_t ddr = cpu->io_port[0];
    uint8_t port_data = cpu->io_port[1];
    uint8_t effective_output = mos6510_io_mask(cpu, port_data);
    cpu->io_interface.output_pins_changed(cpu->io_interface.context, ddr, port_data, effective_output);
}

// Memory access functions using optimized direct callbacks
static inline uint8_t mos6510_read_cycle(mos6510_t* cpu, uint16_t addr) {
    // All addresses go through bus interface - banking system routes zero page to chip functions
    uint8_t result = cpu->bus_interface.bus_read(cpu->bus_interface.context, addr);
    
    // Execute one cycle on other non-CPU chips after the bus operation
    cpu->bus_interface.cycle_tick(cpu->bus_interface.context);
    
    return result;
}

static inline void mos6510_write_cycle(mos6510_t* cpu, uint16_t addr, uint8_t value) {
    // All addresses go through bus interface - banking system routes zero page to chip functions
    cpu->bus_interface.bus_write(cpu->bus_interface.context, addr, value);
    
    // Execute one cycle on other non-CPU chips after the bus operation
    cpu->bus_interface.cycle_tick(cpu->bus_interface.context);
}

// Forward declaration for functions used in macros
void mos6510_interrupt_handler(mos6510_t* cpu);
void c64_non_cpu_cycle(void* c64);  // c64_t* - forward declaration with opaque pointer

// CPU opcode dispatch function
static inline void mos6510_opcode_dispatch(mos6510_t* cpu, uint8_t opcode) {
    mos6510_opcode_handler_t handler = mos6510_opcode_handlers[opcode];
    handler(cpu);
}

// CPU ready check - hardware accurate BA/RDY handling using direct callback
#define CPU_READY(cpu) CPU_TEST_RDY(cpu)

// Wait for CPU ready with automatic stall handling using direct callback
#define CPU_INTRA_CYCLE(cpu) do { \
    UNIQUE_LABEL(cpu_ready_stall): \
    if (__builtin_expect(!CPU_READY(cpu), 0)) { \
        CPU_BUS_CYCLE(cpu); \
        goto UNIQUE_LABEL(cpu_ready_stall); \
    } \
} while(0)

#define CPU_NEXT_INSTRUCTION_DISPATCH(cpu) do { \
    uint8_t opcode = mos6510_read_cycle(cpu, cpu->pc++); \
    mos6510_opcode_dispatch(cpu, opcode); \
} while(0)

#define CPU_NEXT_INSTRUCTION(cpu) do { \
    if (__builtin_expect(CPU_TEST_IRQ(cpu) || CPU_TEST_NMI(cpu), 0)) { \
        mos6510_interrupt_handler(cpu); \
        return; \
    } \
    CPU_INTRA_CYCLE(cpu); \
    CPU_NEXT_INSTRUCTION_DISPATCH(cpu); \
    return; \
} while(0)

#define CPU_OPCODE_FOOTER(cpu) CPU_NEXT_INSTRUCTION(cpu)

// ============================================================================
// PERFORMANCE-OPTIMIZED MACROS FOR CODE DEDUPLICATION
// ============================================================================

// CPU_CONTROL_LINES - Get control lines with zero-indirection access
#define CPU_CONTROL_LINES(cpu) \
    ((cpu)->control_interface.get_lines((cpu)->control_interface.context))

// Test specific control lines using lightweight macros
#define CPU_TEST_IRQ(cpu) (CPU_CONTROL_LINES(cpu) & MOS6510_MASK_IRQ)
#define CPU_TEST_NMI(cpu) (CPU_CONTROL_LINES(cpu) & MOS6510_MASK_NMI)
#define CPU_TEST_RDY(cpu) (CPU_CONTROL_LINES(cpu) & MOS6510_MASK_RDY)

// System lines access macros for direct system state operations
#define CPU_SYSTEM_LINES_TEST(cpu, mask) SYS_LINES_TEST((cpu)->system_lines, mask)
#define CPU_SYSTEM_LINES_SET(cpu, mask) SYS_LINES_SET((cpu)->system_lines, mask)
#define CPU_SYSTEM_LINES_CLEAR(cpu, mask) SYS_LINES_CLEAR((cpu)->system_lines, mask)

// Bus cycle operations - call bus operation then cycle tick
#define CPU_BUS_CYCLE(cpu) \
    ((cpu)->bus_interface.cycle_tick((cpu)->bus_interface.context))



// Flag operations (inline for performance)
static inline void mos6510_set_flag(mos6510_t* cpu, uint8_t flag, bool condition) {
    if (condition) cpu->p |= flag;
    else cpu->p &= ~flag;
}

static inline bool cpu_get_flag(mos6510_t* cpu, uint8_t flag) {
    return (cpu->p & flag) != 0;
}

static inline void mos6510_set_zn(mos6510_t* cpu, uint8_t value) {
    mos6510_set_flag(cpu, FLAG_Z, value == 0);
    mos6510_set_flag(cpu, FLAG_N, value & 0x80);
}

// Stack operations
static inline void mos6510_push(mos6510_t* cpu, uint8_t data) {
    mos6510_write_cycle(cpu, 0x0100 + cpu->sp, data);
    cpu->sp--;
}

static inline uint8_t mos6510_pop(mos6510_t* cpu) {
    cpu->sp++;
    return mos6510_read_cycle(cpu, 0x0100 + cpu->sp);
}

// BRK/IRQ common sequence - handles the interrupt setup portion
static inline void mos6510_interrupt_sequence(mos6510_t* cpu, uint8_t status_flags, uint16_t vector_addr) {
    // Push PC and status unconditionally (skip RDY checks)
    mos6510_push(cpu, (cpu->pc >> 8) & 0xFF);
    mos6510_push(cpu, cpu->pc & 0xFF);
    mos6510_push(cpu, status_flags);
    // Set interrupt disable
    cpu->p |= FLAG_I;
    // Read vector low and high without RDY checks
    uint8_t pc_lo = mos6510_read_cycle(cpu, vector_addr);
    uint8_t pc_hi = mos6510_read_cycle(cpu, vector_addr + 1);
    cpu->pc = (pc_hi << 8) | pc_lo;
    // Note : callers will dispatch the next instruction
}


// ============================================================================
// ADDRESSING MODE HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Immediate addressing - returns the immediate value
static inline uint8_t addr_imm(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    return mos6510_read_cycle(cpu, cpu->pc++);
}

// Zero page addressing - sets address and returns fetched value
static inline uint8_t addr_zp(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    cpu->address = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    return mos6510_read_cycle(cpu, cpu->address);
}

// Zero page,X addressing - sets address and returns fetched value
static inline uint8_t addr_zpx(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t base = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu);
    return mos6510_read_cycle(cpu, cpu->address);
}

// Zero page,Y addressing - sets address and returns fetched value
static inline uint8_t addr_zpy(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t base = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->y) & 0xFF;
    CPU_INTRA_CYCLE(cpu);
    return mos6510_read_cycle(cpu, cpu->address);
}

// Absolute addressing - sets address and returns fetched value
static inline uint8_t addr_abs(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu);
    return mos6510_read_cycle(cpu, cpu->address);
}

// Absolute,X addressing - sets address and returns fetched value
static inline uint8_t addr_absx(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu->address = base + cpu->x;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu->address & 0xFF00)) {
        CPU_INTRA_CYCLE(cpu);
        (void)mos6510_read_cycle(cpu, base); // Dummy read
    }
    CPU_INTRA_CYCLE(cpu);
    return mos6510_read_cycle(cpu, cpu->address);
}

// Absolute,Y addressing - sets address and returns fetched value
static inline uint8_t addr_absy(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu->address = base + cpu->y;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu->address & 0xFF00)) {
        CPU_INTRA_CYCLE(cpu);
        (void)mos6510_read_cycle(cpu, base); // Dummy read
    }
    CPU_INTRA_CYCLE(cpu);
    return mos6510_read_cycle(cpu, cpu->address);
}

// (Zero page,X) - Indexed Indirect addressing
static inline uint8_t addr_zpx_ind(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t base = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    uint8_t zp_addr = (base + cpu->x) & 0xFF;
    
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, zp_addr);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    cpu->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu);
    return mos6510_read_cycle(cpu, cpu->address);
}

// (Zero page),Y - Indirect Indexed addressing
static inline uint8_t addr_zp_ind_y(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t zp_addr = mos6510_read_cycle(cpu, cpu->pc++);

    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, zp_addr);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu->address = base + cpu->y;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu->address & 0xFF00)) {
        CPU_INTRA_CYCLE(cpu);
        (void)mos6510_read_cycle(cpu, base); // Dummy read
    }
    CPU_INTRA_CYCLE(cpu);
    return mos6510_read_cycle(cpu, cpu->address);
}

// ============================================================================
// CPU OPERATION HELPER FUNCTIONS (inline for performance)
// ============================================================================

// ADC - Add with Carry
static inline void op_adc(mos6510_t* cpu, uint8_t value) {
    if (cpu->p & FLAG_D) {
        // Decimal mode - BCD arithmetic
        uint8_t carry_in = (cpu->p & FLAG_C) ? 1 : 0;

        // Split into low and high nibbles for BCD
        uint8_t a_low = cpu->a & 0x0F;
        uint8_t a_high = (cpu->a >> 4) & 0x0F;
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
        cpu->p &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);

        if (0 == (uint8_t)(cpu->a + value + carry_in)) {
            cpu->p |= FLAG_Z;
        } else if ((high_sum & 0x08) != 0) {
            cpu->p |= FLAG_N;
        }

        if ((~(cpu->a ^ value) & (cpu->a ^ (high_sum << 4)) & 0x80) != 0) {
            cpu->p |= FLAG_V;
        }

        if (high_sum > 15) {
            cpu->p |= FLAG_C;
        }
        
        cpu->a = result;
    } else {    
        uint16_t temp = cpu->a + value + (cpu_get_flag(cpu, FLAG_C) ? 1 : 0);
        mos6510_set_flag(cpu, FLAG_C, temp > 255);
        mos6510_set_flag(cpu, FLAG_V, (~(cpu->a ^ value) & (cpu->a ^ temp)) & 0x80);
        cpu->a = temp & 0xFF;
        mos6510_set_zn(cpu, cpu->a);
    }
}

// AND - Logical AND
static inline void op_and(mos6510_t* cpu, uint8_t value) {
    cpu->a &= value;
    mos6510_set_zn(cpu, cpu->a);
}

// ASL - Arithmetic Shift Left
static inline uint8_t op_asl(mos6510_t* cpu, uint8_t value) {
    mos6510_set_flag(cpu, FLAG_C, value & 0x80);
    value <<= 1;
    mos6510_set_zn(cpu, value);
    return value;
}

// BIT - Bit Test
static inline void op_bit(mos6510_t* cpu, uint8_t value) {
    mos6510_set_flag(cpu, FLAG_Z, (cpu->a & value) == 0);
    mos6510_set_flag(cpu, FLAG_V, value & FLAG_V);
    mos6510_set_flag(cpu, FLAG_N, value & FLAG_N);
}

// CMP - Compare
static inline void op_cmp(mos6510_t* cpu, uint8_t value) {
    uint16_t temp = cpu->a - value;
    mos6510_set_flag(cpu, FLAG_C, cpu->a >= value);
    mos6510_set_zn(cpu, temp & 0xFF);
}

// CPX - Compare X Register
static inline void op_cpx(mos6510_t* cpu, uint8_t value) {
    uint16_t temp = cpu->x - value;
    mos6510_set_flag(cpu, FLAG_C, cpu->x >= value);
    mos6510_set_zn(cpu, temp & 0xFF);
}

// CPY - Compare Y Register
static inline void op_cpy(mos6510_t* cpu, uint8_t value) {
    uint16_t temp = cpu->y - value;
    mos6510_set_flag(cpu, FLAG_C, cpu->y >= value);
    mos6510_set_zn(cpu, temp & 0xFF);
}

// DEC - Decrement
static inline uint8_t op_dec(mos6510_t* cpu, uint8_t value) {
    value--;
    mos6510_set_zn(cpu, value);
    return value;
}

// EOR - Exclusive OR
static inline void op_eor(mos6510_t* cpu, uint8_t value) {
    cpu->a ^= value;
    mos6510_set_zn(cpu, cpu->a);
}

// INC - Increment
static inline uint8_t op_inc(mos6510_t* cpu, uint8_t value) {
    value++;
    mos6510_set_zn(cpu, value);
    return value;
}

// LDA - Load Accumulator
static inline void op_lda(mos6510_t* cpu, uint8_t value) {
    cpu->a = value;
    mos6510_set_zn(cpu, cpu->a);
}

// LDX - Load X Register
static inline void op_ldx(mos6510_t* cpu, uint8_t value) {
    cpu->x = value;
    mos6510_set_zn(cpu, cpu->x);
}

// LDY - Load Y Register
static inline void op_ldy(mos6510_t* cpu, uint8_t value) {
    cpu->y = value;
    mos6510_set_zn(cpu, cpu->y);
}

// LSR - Logical Shift Right
static inline uint8_t op_lsr(mos6510_t* cpu, uint8_t value) {
    mos6510_set_flag(cpu, FLAG_C, value & 0x01);
    value >>= 1;
    mos6510_set_zn(cpu, value);
    return value;
}

// ORA - Logical Inclusive OR
static inline void op_ora(mos6510_t* cpu, uint8_t value) {
    cpu->a |= value;
    mos6510_set_zn(cpu, cpu->a);
}

// ROL - Rotate Left
static inline uint8_t op_rol(mos6510_t* cpu, uint8_t value) {
    uint8_t temp = (value << 1) | (cpu_get_flag(cpu, FLAG_C) ? 1 : 0);
    mos6510_set_flag(cpu, FLAG_C, value & 0x80);
    mos6510_set_zn(cpu, temp);
    return temp;
}

// ROR - Rotate Right
static inline uint8_t op_ror(mos6510_t* cpu, uint8_t value) {
    uint8_t temp = (value >> 1) | (cpu_get_flag(cpu, FLAG_C) ? 0x80 : 0);
    mos6510_set_flag(cpu, FLAG_C, value & 0x01);
    mos6510_set_zn(cpu, temp);
    return temp;
}

// SBC - Subtract with Carry
static inline void op_sbc(mos6510_t* cpu, uint8_t value) {
    uint16_t temp = cpu->a - value - (cpu_get_flag(cpu, FLAG_C) ? 0 : 1);
    mos6510_set_flag(cpu, FLAG_C, temp < 0x100);
    mos6510_set_flag(cpu, FLAG_V, ((cpu->a ^ value) & (cpu->a ^ temp)) & 0x80);
    cpu->a = temp & 0xFF;
    mos6510_set_zn(cpu, cpu->a);
}

// Generic arithmetic operation helper - combines addressing mode with operation
static inline void mos6510_arithmetic_helper(mos6510_t* cpu, uint8_t (*addr_func)(mos6510_t*), void (*op_func)(mos6510_t*, uint8_t)) {
    uint8_t value = addr_func(cpu);
    op_func(cpu, value);
    CPU_OPCODE_FOOTER(cpu);
}

static inline void mos6510_flag_clear_helper(mos6510_t* cpu, uint8_t flag) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    cpu->p &= ~flag;
    CPU_OPCODE_FOOTER(cpu);
}

static inline void mos6510_flag_set_helper(mos6510_t* cpu, uint8_t flag) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    cpu->p |= flag;
    CPU_OPCODE_FOOTER(cpu);
}

// Generic load operation helper - combines addressing mode with load operation
static inline void mos6510_load_helper(mos6510_t* cpu, uint8_t (*addr_func)(mos6510_t*), void (*op_func)(mos6510_t*, uint8_t)) {
    uint8_t value = addr_func(cpu);
    op_func(cpu, value);
    CPU_OPCODE_FOOTER(cpu);
}

// ============================================================================
// STORE ADDRESS HELPER FUNCTIONS (inline for performance)  
// ============================================================================

// Zero page addressing for stores - sets address only
static inline void addr_zp_store(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    cpu->address = mos6510_read_cycle(cpu, cpu->pc++);
}

// Zero page,X addressing for stores - sets address only  
static inline void addr_zpx_store(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t base = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->x) & 0xFF;
}

// Zero page,Y addressing for stores - sets address only
static inline void addr_zpy_store(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t base = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    cpu->address = (base + cpu->y) & 0xFF;
}

// Absolute addressing for stores - sets address only
static inline void addr_abs_store(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
}

// Absolute,X addressing for stores - sets address only (with dummy read)
static inline void addr_absx_store(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address + cpu->x); // Dummy read
    cpu->address += cpu->x;
}

// Absolute,Y addressing for stores - sets address only (with dummy read)
static inline void addr_absy_store(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address + cpu->y); // Dummy read
    cpu->address += cpu->y;
}

// (Zero page,X) addressing for stores - sets address only
static inline void addr_zpx_ind_store(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t base = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, base); // Dummy read
    uint8_t zp_addr = (base + cpu->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, zp_addr);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    cpu->address = (addr_hi << 8) | addr_lo;
}

// (Zero page),Y addressing for stores - sets address only (with dummy read)
static inline void addr_zp_ind_y_store(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t zp_addr = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, zp_addr);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    cpu->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address + cpu->y); // Dummy read
    cpu->address += cpu->y;
}

// Generic store operation helper - combines addressing mode with store value
static inline void mos6510_store_helper(mos6510_t* cpu, void (*addr_func)(mos6510_t*), uint8_t value) {
    addr_func(cpu);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value);
    CPU_OPCODE_FOOTER(cpu);
}

// ============================================================================
// CONTROL FLOW HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Generic branch helper - handles all branch instruction logic
static inline void mos6510_branch_helper(mos6510_t* cpu, bool condition) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t rel_addr = mos6510_read_cycle(cpu, cpu->pc++);
    if (condition) {
        // Branch taken
        CPU_INTRA_CYCLE(cpu);
        (void)mos6510_read_cycle(cpu, cpu->pc); // Dummy read
        uint16_t new_pc = cpu->pc + (int8_t)rel_addr;
        if ((cpu->pc ^ new_pc) & 0xFF00) {
            // Page crossed - extra cycle
            CPU_INTRA_CYCLE(cpu);
            (void)mos6510_read_cycle(cpu, (cpu->pc & 0xFF00) | (new_pc & 0xFF));
        }
        cpu->pc = new_pc;
    }
    CPU_OPCODE_FOOTER(cpu);
}

// Stack push with timing control
static inline void mos6510_push_with_wait(mos6510_t* cpu, uint8_t data) {
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, 0x0100 + cpu->sp, data);
    cpu->sp--;
}

// Stack pop with timing control  
static inline uint8_t mos6510_pop_with_wait(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    cpu->sp++;
    return mos6510_read_cycle(cpu, 0x0100 + cpu->sp);
}

// ============================================================================
// READ-MODIFY-WRITE HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Accumulator read-modify-write operations (2 cycles)
static inline void mos6510_rmw_accumulator(mos6510_t* cpu, uint8_t (*operation)(mos6510_t*, uint8_t)) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    cpu->a = operation(cpu, cpu->a);
    CPU_OPCODE_FOOTER(cpu);
}

// Zero page read-modify-write operations
static inline void mos6510_rmw_zero_page(mos6510_t* cpu, uint8_t (*operation)(mos6510_t*, uint8_t)) {
    CPU_INTRA_CYCLE(cpu);
    cpu->address = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t value = mos6510_read_cycle(cpu, cpu->address);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value);
    CPU_OPCODE_FOOTER(cpu);
}

// Zero page,X read-modify-write operations
static inline void mos6510_rmw_zero_page_x(mos6510_t* cpu, uint8_t (*operation)(mos6510_t*, uint8_t)) {
    CPU_INTRA_CYCLE(cpu);
    cpu->address = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address); // Dummy read
    cpu->address = (cpu->address + cpu->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu);
    uint8_t value = mos6510_read_cycle(cpu, cpu->address);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value);
    CPU_OPCODE_FOOTER(cpu);
}

// Absolute read-modify-write operations
static inline void mos6510_rmw_absolute(mos6510_t* cpu, uint8_t (*operation)(mos6510_t*, uint8_t)) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = addr_lo;
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu);
    uint8_t value = mos6510_read_cycle(cpu, cpu->address);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value);
    CPU_OPCODE_FOOTER(cpu);
}

// Absolute,X read-modify-write operations
static inline void mos6510_rmw_absolute_x(mos6510_t* cpu, uint8_t (*operation)(mos6510_t*, uint8_t)) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, (cpu->address & 0xFF00) | ((cpu->address + cpu->x) & 0xFF)); // Dummy read
    cpu->address += cpu->x;
    CPU_INTRA_CYCLE(cpu);
    uint8_t value = mos6510_read_cycle(cpu, cpu->address);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value); // Write original value
    value = operation(cpu, value);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value);
    CPU_OPCODE_FOOTER(cpu);
}

// ============================================================================
// REGISTER OPERATION HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Register increment/decrement with flags
static inline void mos6510_register_inc_dec(mos6510_t* cpu, uint8_t* reg, int delta) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    *reg += (uint8_t)delta;
    mos6510_set_zn(cpu, *reg);
    CPU_OPCODE_FOOTER(cpu);
}

// Register transfer with flags (when flags should be set)
static inline void mos6510_register_transfer_with_flags(mos6510_t* cpu, uint8_t* dest, uint8_t src) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    *dest = src;
    mos6510_set_zn(cpu, *dest);
    CPU_OPCODE_FOOTER(cpu);
}

// Register transfer without flags (e.g., TXS)
static inline void mos6510_register_transfer_no_flags(mos6510_t* cpu, uint8_t* dest, uint8_t src) {
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->pc);  // Dummy read
    *dest = src;
    CPU_OPCODE_FOOTER(cpu);
}

// Memory increment/decrement operations
static inline void mos6510_memory_inc_dec(mos6510_t* cpu, uint8_t fetched, int delta) {
    uint8_t result = fetched + (uint8_t)delta;
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, result);
    mos6510_set_zn(cpu, result);
    CPU_OPCODE_FOOTER(cpu);
}

// ============================================================================
// ILLEGAL INSTRUCTION HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Read-modify-write + register operation combo (SLO, RLA, RRA, SRE)
static inline void mos6510_illegal_rmw_combo(mos6510_t* cpu, uint8_t value, 
                                          uint8_t (*rmw_op)(mos6510_t*, uint8_t),
                                          void (*reg_op)(mos6510_t*, uint8_t)) {
    uint8_t result = rmw_op(cpu, value);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, result);
    reg_op(cpu, result);
    CPU_OPCODE_FOOTER(cpu);
}

// INC/DEC + register operation combo (DCP, ISC)
static inline void mos6510_illegal_inc_dec_combo(mos6510_t* cpu, uint8_t value, 
                                              int delta, void (*reg_op)(mos6510_t*, uint8_t)) {
    value += (uint8_t)delta;
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value);
    reg_op(cpu, value);
    CPU_OPCODE_FOOTER(cpu);
}

// Load both A and X (LAX variants)
static inline void mos6510_load_a_and_x(mos6510_t* cpu, uint8_t value) {
    cpu->a = value;
    cpu->x = value;
    mos6510_set_zn(cpu, value);
    CPU_OPCODE_FOOTER(cpu);
}

// Store A & X (SAX variants)
static inline void mos6510_store_a_and_x(mos6510_t* cpu, void (*addr_func)(mos6510_t*, uint8_t)) {
    addr_func(cpu, cpu->a & cpu->x);
    CPU_OPCODE_FOOTER(cpu);
}

// Complex store with high byte manipulation (AHX, SHX, SHY, TAS)
static inline void mos6510_complex_store(mos6510_t* cpu, uint8_t value) {
    value &= ((cpu->address >> 8) + 1);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, value);
    CPU_OPCODE_FOOTER(cpu);
}

// Immediate mode accumulator operations (ALR, ANC, ARR, AXS, XAA)
static inline void mos6510_immediate_accumulator_op(mos6510_t* cpu, void (*operation)(mos6510_t*, uint8_t)) {
    uint8_t value = addr_imm(cpu);
    operation(cpu, value);
    CPU_OPCODE_FOOTER(cpu);
}

// ============================================================================
// ILLEGAL INSTRUCTION SPECIFIC OPERATIONS (inline for performance)
// ============================================================================

// ALR operation: AND then LSR
static inline void op_alr(mos6510_t* cpu, uint8_t value) {
    cpu->a &= value;
    mos6510_set_flag(cpu, FLAG_C, cpu->a & 0x01);
    cpu->a >>= 1;
    mos6510_set_zn(cpu, cpu->a);
}

// ANC operation: AND then copy N to C
static inline void op_anc(mos6510_t* cpu, uint8_t value) {
    cpu->a &= value;
    mos6510_set_zn(cpu, cpu->a);
    mos6510_set_flag(cpu, FLAG_C, cpu->a & 0x80);
}

// ARR operation: AND then ROR with special V flag behavior
static inline void op_arr(mos6510_t* cpu, uint8_t value) {
    cpu->a &= value;
    uint8_t old_carry = cpu_get_flag(cpu, FLAG_C) ? 1 : 0;
    mos6510_set_flag(cpu, FLAG_C, cpu->a & 0x01);
    cpu->a = (cpu->a >> 1) | (old_carry << 7);
    mos6510_set_zn(cpu, cpu->a);
    // V flag behavior is complex for ARR
    mos6510_set_flag(cpu, FLAG_V, ((cpu->a >> 6) ^ (cpu->a >> 5)) & 1);
}

// AXS operation: (A & X) - immediate, store in X
static inline void op_axs(mos6510_t* cpu, uint8_t value) {
    uint8_t temp = cpu->a & cpu->x;
    uint16_t result = temp - value;
    mos6510_set_flag(cpu, FLAG_C, result < 0x100);
    cpu->x = result & 0xFF;
    mos6510_set_zn(cpu, cpu->x);
}

// XAA operation: Transfer X to A, then AND with immediate
static inline void op_xaa(mos6510_t* cpu, uint8_t value) {
    cpu->a = cpu->x;
    cpu->a &= value;
    mos6510_set_zn(cpu, cpu->a);
}

// SLO register operation: ORA with result
static inline void op_slo_reg(mos6510_t* cpu, uint8_t value) {
    cpu->a |= value;
    mos6510_set_zn(cpu, cpu->a);
}

// RLA register operation: AND with result
static inline void op_rla_reg(mos6510_t* cpu, uint8_t value) {
    cpu->a &= value;
    mos6510_set_zn(cpu, cpu->a);
}

// SRE register operation: EOR with result
static inline void op_sre_reg(mos6510_t* cpu, uint8_t value) {
    cpu->a ^= value;
    mos6510_set_zn(cpu, cpu->a);
}

// CPU core functions
void mos6510_init(mos6510_t* cpu);
void mos6510_reset(mos6510_t* cpu);
bool mos6510_step(mos6510_t* cpu);
void mos6510_execute(mos6510_t* cpu);
bool mos6510_is_intercepting(void);
void mos6510_nmi(mos6510_t* cpu);
void mos6510_irq(mos6510_t* cpu, uint8_t status);

// Chip descriptor
extern chip_descriptor_t mos6510_descriptor;

// Performance-optimized interface attachment functions
void mos6510_attach_bus_interface(mos6510_t* cpu, const bus_cycle_ops_t* bus_interface);
void mos6510_attach_control_lines_interface(mos6510_t* cpu, const control_lines_interface_t* control_interface);
void mos6510_attach_io_interface(mos6510_t* cpu, const mos6510_io_port_interface_t* io_interface);
void mos6510_attach_system_lines(mos6510_t* cpu, system_lines_t* system_lines);
void mos6510_attach_ram(mos6510_t* cpu, void* ram_context, 
                        uint8_t (*ram_read)(void*, uint16_t), 
                        void (*ram_write)(void*, uint16_t, uint8_t));

#endif // MOS6510_H