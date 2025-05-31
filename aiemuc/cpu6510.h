#ifndef CPU6510_H
#define CPU6510_H

#include "device.h"
#include "bus.h"
#include "ram.h"
#include <stdint.h>
#include <stdbool.h>

// Compiler optimization hints
#ifdef _MSC_VER
#define likely(x)   (x)
#define unlikely(x) (x)
#else
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// ============================================================================
// MOS 6510 CPU EMULATION - Cycle-accurate with direct threading
// ============================================================================

// CPU state structure
typedef struct {
    const device_t* device;  // Pointer to device descriptor (must be first)
    
    // CPU Registers
    uint16_t pc;        // Program Counter
    uint8_t a;          // Accumulator
    uint8_t x;          // X Index Register
    uint8_t y;          // Y Index Register
    uint8_t sp;         // Stack Pointer
    uint8_t p;          // Processor Status Register

    // Internal CPU state for cycle-accurate emulation
    uint16_t address;   // Address for current instruction (used for both absolute and relative)
    uint8_t io_port[2]; // 0:DDR, 1:Port
    
    // Device attachments
    bus_state_t* bus;   // Pointer to attached bus
} cpu6510_state_t;

// Universal instruction dispatch using function pointers
// (Works well on all compilers - performance difference with computed goto is minimal)
typedef void (*instruction_func_t)(cpu6510_state_t* cpu_dev);

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

// ============================================================================
// I/O PORT EMULATION - Direct handling in CPU read/write cycles
// ============================================================================

// PLA functions
void switch_cpu_mode(uint8_t mode);

// Memory access functions
uint8_t cpu_read_cycle(cpu6510_state_t* cpu_dev, uint16_t addr);
void cpu_write_cycle(cpu6510_state_t* cpu_dev, uint16_t addr, uint8_t value);

// Bus cycle function
void c64_non_cpu_cycles(void);

// CPU ready check - hardware accurate BA/RDY handling
#define CPU_READY(cpu_dev) (((cpu_dev)->bus->control_lines & RDY_LINE) != 0)

// Wait for CPU ready with automatic stall handling
#define CPU_READY_OR_STALL(cpu_dev, label) do { \
    label: \
    if (!CPU_READY(cpu_dev)) { c64_non_cpu_cycles(); goto label; } \
} while(0)

#define WAIT_READY_THEN_WRITE(cpu_dev, addr, data, label) do { \
    label: \
    if (!CPU_READY(cpu_dev)) { c64_non_cpu_cycles(); goto label; } \
    cpu_write_cycle(cpu_dev, addr, data); \
} while(0)

#define NEXT_INSTRUCTION(cpu_dev, fetch_label) do { \
    if (unlikely(cpu_dev->bus->control_lines & (IRQ_LINE | NMI_LINE))) { \
        handle_interrupt_func(cpu_dev); \
        return; \
    } \
    CPU_READY_OR_STALL(cpu_dev, fetch_label); \
    uint8_t opcode = cpu_read_cycle(cpu_dev, cpu_dev->pc++); \
    instruction_table[opcode](cpu_dev); \
    return; \
} while(0)

// Flag operations (inline for performance)
static inline void cpu_set_flag(cpu6510_state_t* cpu_dev, uint8_t flag, bool condition) {
    if (condition) cpu_dev->p |= flag;
    else cpu_dev->p &= ~flag;
}

static inline bool cpu_get_flag(cpu6510_state_t* cpu_dev, uint8_t flag) {
    return (cpu_dev->p & flag) != 0;
}

static inline void cpu_set_zn(cpu6510_state_t* cpu_dev, uint8_t value) {
    cpu_set_flag(cpu_dev, FLAG_Z, value == 0);
    cpu_set_flag(cpu_dev, FLAG_N, value & 0x80);
}

// Stack operations
static inline void cpu_push(cpu6510_state_t* cpu_dev, uint8_t data) {
    cpu_write_cycle(cpu_dev, 0x0100 + cpu_dev->sp, data);
    cpu_dev->sp--;
}

static inline uint8_t cpu_pop(cpu6510_state_t* cpu_dev) {
    cpu_dev->sp++;
    return cpu_read_cycle(cpu_dev, 0x0100 + cpu_dev->sp);
}

// ============================================================================
// ADDRESSING MODE HELPER FUNCTIONS (inline for performance)
// ============================================================================

// Immediate addressing - returns the immediate value
static inline uint8_t addr_imm(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, imm_wait);
    return cpu_read_cycle(cpu_dev, cpu_dev->pc++);
}

// Zero page addressing - sets address and returns fetched value
static inline uint8_t addr_zp(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, zp_addr_wait);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, zp_data_wait);
    return cpu_read_cycle(cpu_dev, cpu_dev->address);
}

// Zero page,X addressing - sets address and returns fetched value
static inline uint8_t addr_zpx(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, zpx_addr_wait);
    uint8_t base = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, zpx_dummy_wait);
    (void)cpu_read_cycle(cpu_dev, base); // Dummy read
    cpu_dev->address = (base + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, zpx_data_wait);
    return cpu_read_cycle(cpu_dev, cpu_dev->address);
}

// Zero page,Y addressing - sets address and returns fetched value
static inline uint8_t addr_zpy(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, zpy_addr_wait);
    uint8_t base = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, zpy_dummy_wait);
    (void)cpu_read_cycle(cpu_dev, base); // Dummy read
    cpu_dev->address = (base + cpu_dev->y) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, zpy_data_wait);
    return cpu_read_cycle(cpu_dev, cpu_dev->address);
}

// Absolute addressing - sets address and returns fetched value
static inline uint8_t addr_abs(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, abs_lo_wait);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, abs_hi_wait);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, abs_data_wait);
    return cpu_read_cycle(cpu_dev, cpu_dev->address);
}

// Absolute,X addressing - sets address and returns fetched value
static inline uint8_t addr_absx(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, absx_lo_wait);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, absx_hi_wait);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu_dev->address = base + cpu_dev->x;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu_dev->address & 0xFF00)) {
        CPU_READY_OR_STALL(cpu_dev, absx_dummy_wait);
        (void)cpu_read_cycle(cpu_dev, base); // Dummy read
    }
    CPU_READY_OR_STALL(cpu_dev, absx_data_wait);
    return cpu_read_cycle(cpu_dev, cpu_dev->address);
}

// Absolute,Y addressing - sets address and returns fetched value
static inline uint8_t addr_absy(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, absy_lo_wait);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, absy_hi_wait);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu_dev->address = base + cpu_dev->y;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu_dev->address & 0xFF00)) {
        CPU_READY_OR_STALL(cpu_dev, absy_dummy_wait);
        (void)cpu_read_cycle(cpu_dev, base); // Dummy read
    }
    CPU_READY_OR_STALL(cpu_dev, absy_data_wait);
    return cpu_read_cycle(cpu_dev, cpu_dev->address);
}

// (Zero page,X) - Indexed Indirect addressing
static inline uint8_t addr_zpx_ind(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_base_wait);
    uint8_t base = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_dummy_wait);
    (void)cpu_read_cycle(cpu_dev, base); // Dummy read
    uint8_t zp_addr = (base + cpu_dev->x) & 0xFF;
    
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_lo_wait);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_hi_wait);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_data_wait);
    return cpu_read_cycle(cpu_dev, cpu_dev->address);
}

// (Zero page),Y - Indirect Indexed addressing
static inline uint8_t addr_zp_ind_y(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_base_wait);
    uint8_t zp_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);

    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_lo_wait);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_hi_wait);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    uint16_t base = (addr_hi << 8) | addr_lo;
    cpu_dev->address = base + cpu_dev->y;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu_dev->address & 0xFF00)) {
        CPU_READY_OR_STALL(cpu_dev, zp_ind_y_dummy_wait);
        (void)cpu_read_cycle(cpu_dev, base); // Dummy read
    }
    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_data_wait);
    return cpu_read_cycle(cpu_dev, cpu_dev->address);
}

// ============================================================================
// CPU OPERATION HELPER FUNCTIONS (inline for performance)
// ============================================================================

// ADC - Add with Carry
static inline uint8_t op_adc(cpu6510_state_t* cpu_dev, uint8_t value) {
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
        return result;
    } else {    
        uint16_t temp = cpu_dev->a + value + (cpu_get_flag(cpu_dev, FLAG_C) ? 1 : 0);
        cpu_set_flag(cpu_dev, FLAG_C, temp > 255);
        cpu_set_flag(cpu_dev, FLAG_V, (~(cpu_dev->a ^ value) & (cpu_dev->a ^ temp)) & 0x80);
        cpu_dev->a = temp & 0xFF;
        cpu_set_zn(cpu_dev, cpu_dev->a);
        return cpu_dev->a;
    }
}

// AND - Logical AND
static inline void op_and(cpu6510_state_t* cpu_dev, uint8_t value) {
    cpu_dev->a &= value;
    cpu_set_zn(cpu_dev, cpu_dev->a);
}

// ASL - Arithmetic Shift Left
static inline uint8_t op_asl(cpu6510_state_t* cpu_dev, uint8_t value) {
    cpu_set_flag(cpu_dev, FLAG_C, value & 0x80);
    value <<= 1;
    cpu_set_zn(cpu_dev, value);
    return value;
}

// BIT - Bit Test
static inline void op_bit(cpu6510_state_t* cpu_dev, uint8_t value) {
    cpu_set_flag(cpu_dev, FLAG_Z, (cpu_dev->a & value) == 0);
    cpu_set_flag(cpu_dev, FLAG_V, value & FLAG_V);
    cpu_set_flag(cpu_dev, FLAG_N, value & FLAG_N);
}

// CMP - Compare
static inline void op_cmp(cpu6510_state_t* cpu_dev, uint8_t value) {
    uint16_t temp = cpu_dev->a - value;
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->a >= value);
    cpu_set_zn(cpu_dev, temp & 0xFF);
}

// CPX - Compare X Register
static inline void op_cpx(cpu6510_state_t* cpu_dev, uint8_t value) {
    uint16_t temp = cpu_dev->x - value;
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->x >= value);
    cpu_set_zn(cpu_dev, temp & 0xFF);
}

// CPY - Compare Y Register
static inline void op_cpy(cpu6510_state_t* cpu_dev, uint8_t value) {
    uint16_t temp = cpu_dev->y - value;
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->y >= value);
    cpu_set_zn(cpu_dev, temp & 0xFF);
}

// DEC - Decrement
static inline uint8_t op_dec(cpu6510_state_t* cpu_dev, uint8_t value) {
    value--;
    cpu_set_zn(cpu_dev, value);
    return value;
}

// EOR - Exclusive OR
static inline void op_eor(cpu6510_state_t* cpu_dev, uint8_t value) {
    cpu_dev->a ^= value;
    cpu_set_zn(cpu_dev, cpu_dev->a);
}

// INC - Increment
static inline uint8_t op_inc(cpu6510_state_t* cpu_dev, uint8_t value) {
    value++;
    cpu_set_zn(cpu_dev, value);
    return value;
}

// LDA - Load Accumulator
static inline void op_lda(cpu6510_state_t* cpu_dev, uint8_t value) {
    cpu_dev->a = value;
    cpu_set_zn(cpu_dev, cpu_dev->a);
}

// LDX - Load X Register
static inline void op_ldx(cpu6510_state_t* cpu_dev, uint8_t value) {
    cpu_dev->x = value;
    cpu_set_zn(cpu_dev, cpu_dev->x);
}

// LDY - Load Y Register
static inline void op_ldy(cpu6510_state_t* cpu_dev, uint8_t value) {
    cpu_dev->y = value;
    cpu_set_zn(cpu_dev, cpu_dev->y);
}

// LSR - Logical Shift Right
static inline uint8_t op_lsr(cpu6510_state_t* cpu_dev, uint8_t value) {
    cpu_set_flag(cpu_dev, FLAG_C, value & 0x01);
    value >>= 1;
    cpu_set_zn(cpu_dev, value);
    return value;
}

// ORA - Logical Inclusive OR
static inline void op_ora(cpu6510_state_t* cpu_dev, uint8_t value) {
    cpu_dev->a |= value;
    cpu_set_zn(cpu_dev, cpu_dev->a);
}

// ROL - Rotate Left
static inline uint8_t op_rol(cpu6510_state_t* cpu_dev, uint8_t value) {
    uint8_t temp = (value << 1) | (cpu_get_flag(cpu_dev, FLAG_C) ? 1 : 0);
    cpu_set_flag(cpu_dev, FLAG_C, value & 0x80);
    cpu_set_zn(cpu_dev, temp);
    return temp;
}

// ROR - Rotate Right
static inline uint8_t op_ror(cpu6510_state_t* cpu_dev, uint8_t value) {
    uint8_t temp = (value >> 1) | (cpu_get_flag(cpu_dev, FLAG_C) ? 0x80 : 0);
    cpu_set_flag(cpu_dev, FLAG_C, value & 0x01);
    cpu_set_zn(cpu_dev, temp);
    return temp;
}

// SBC - Subtract with Carry
static inline uint8_t op_sbc(cpu6510_state_t* cpu_dev, uint8_t value) {
    uint16_t temp = cpu_dev->a - value - (cpu_get_flag(cpu_dev, FLAG_C) ? 0 : 1);
    cpu_set_flag(cpu_dev, FLAG_C, temp < 0x100);
    cpu_set_flag(cpu_dev, FLAG_V, ((cpu_dev->a ^ value) & (cpu_dev->a ^ temp)) & 0x80);
    cpu_dev->a = temp & 0xFF;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    return cpu_dev->a;
}

// Forward declarations for functions defined in main file
void handle_interrupt_func(cpu6510_state_t* cpu_dev);
void cpu6510_irq(cpu6510_state_t* cpu_dev, uint8_t status);

// CPU core functions
void cpu6510_init(cpu6510_state_t* cpu_dev);
void cpu6510_reset(cpu6510_state_t* cpu_dev);
bool cpu6510_step(cpu6510_state_t* cpu_dev);
void cpu6510_execute(cpu6510_state_t* cpu_dev);
void cpu6510_nmi(cpu6510_state_t* cpu_dev);

// Device attachments
void cpu_attach_bus(cpu6510_state_t* cpu_dev, bus_state_t* bus_state);

// Instruction setup
void cpu6510_setup_opcode_table(void);

#endif // CPU6510_H