#include "cpu6510.h"
#include "bus.h"
#include "c64.h"
#include <string.h>

// ============================================================================
// MOS 6510 CPU STATE
// ============================================================================
cpu6510_state_t cpu;

// Universal instruction table using function pointers
static instruction_func_t instruction_table[256];

// Forward declarations
void* handle_interrupt = NULL;
void* fetch_opcode = NULL;

// Initialize CPU
void cpu6510_init(void) {
    cpu.a = 0;
    cpu.x = 0;
    cpu.y = 0;
    cpu.sp = 0xFD;
    cpu.p = FLAG_U | FLAG_I;  // Unused flag always set, interrupt disable
    cpu.pc = 0;
    
    // 6510-specific I/O port (addresses $0000/$0001) initialization
    ram[0x0000] = 0x2F; // Default Data Direction Register (DDR at $0000)
    ram[0x0001] = 0x37;  // Default I/O Port Data (at $0001)
    switch_cpu_mode(0x07); // All RAM/ROM enabled
 
    cpu.total_cycles = 0;
    
    // Setup instruction table
    cpu6510_setup_opcode_table();
}

// Reset CPU
void cpu6510_reset(void) {
    // Read reset vector from $FFFC/$FFFD
    cpu_read_cycle(0xFFFC);
    uint8_t pcl = bus_state.data;
    cpu_read_cycle(0xFFFD);
    uint8_t pch = bus_state.data;
    
    cpu.pc = (pch << 8) | pcl;
    cpu.sp = 0xFF; // or 0FD?
    cpu.p |= FLAG_I;  // Set interrupt disable
    cpu.total_cycles = 0;
}

bool cpu6510_step(void) {
    cpu.total_cycles++;
    bus_cycle();
    // Return true when instruction completes (for testing)
    return true;
}

void cpu6510_irq(void) {
    // Push PC and status to stack, set interrupt disable, jump to IRQ vector
    cpu_push((cpu.pc >> 8) & 0xFF);
    cpu_push(cpu.pc & 0xFF);
    cpu_push(cpu.p & ~FLAG_B);  // Clear B flag for IRQ
    cpu.p |= FLAG_I;
    cpu_read_cycle(0xFFFE);
    cpu.pc = bus_state.data;
    cpu_read_cycle(0xFFFF);
    cpu.pc |= (bus_state.data << 8);
}

void cpu6510_nmi(void) {
    // Push PC and status to stack, set interrupt disable, jump to NMI vector
    cpu_push((cpu.pc >> 8) & 0xFF);
    cpu_push(cpu.pc & 0xFF);
    cpu_push(cpu.p & ~FLAG_B);  // Clear B flag for NMI
    cpu.p |= FLAG_I;
    cpu_read_cycle(0xFFFA);
    cpu.pc = bus_state.data;
    cpu_read_cycle(0xFFFB);
    cpu.pc |= (bus_state.data << 8);
}

// ============================================================================
// INSTRUCTION OPERATIONS (ALPHABETICAL ORDER)
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
static inline void op_cmp(uint8_t reg, uint8_t value) {
    uint16_t temp = reg - value;
    cpu_set_flag(FLAG_C, reg >= value);
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

// ============================================================================
// INSTRUCTION FUNCTIONS (Universal approach using function pointers)
// ============================================================================

// Forward declarations
void lda_immediate_func(void);
void lda_zero_page_func(void);
void lda_absolute_func(void);
void sta_absolute_func(void);
void sta_zero_page_func(void);
void jmp_absolute_func(void);
void nop_instruction_func(void);
void brk_instruction_func(void);
void adc_immediate_func(void);
void and_immediate_func(void);
void asl_accumulator_func(void);
void illegal_instruction_func(void);
void handle_interrupt_func(void);

// Universal instruction implementations
void lda_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_imm_wait);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_imm_fetch_wait);
}

void lda_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.addr_abs, lda_zp_wait2);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_zp_fetch_wait);
}

void lda_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, lda_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, lda_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_READ(cpu.addr_abs, lda_abs_wait3);
    op_lda(bus_state.data);
    NEXT_INSTRUCTION(lda_abs_fetch_wait);
}

void sta_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sta_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, sta_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_abs_wait3);
    NEXT_INSTRUCTION(sta_abs_fetch_wait);
}

void sta_zero_page_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, sta_zp_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_zp_wait2);
    NEXT_INSTRUCTION(sta_zp_fetch_wait);
}

void jmp_absolute_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, jmp_abs_wait1);
    cpu.addr_abs = bus_state.data;
    WAIT_READY_THEN_READ(cpu.pc++, jmp_abs_wait2);
    cpu.addr_abs |= (bus_state.data << 8);
    cpu.pc = cpu.addr_abs;
    NEXT_INSTRUCTION(jmp_abs_fetch_wait);
}

void adc_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, adc_imm_wait);
    op_adc(bus_state.data);
    NEXT_INSTRUCTION(adc_imm_fetch_wait);
}

void and_immediate_func(void) {
    WAIT_READY_THEN_READ(cpu.pc++, and_imm_wait);
    op_and(bus_state.data);
    NEXT_INSTRUCTION(and_imm_fetch_wait);
}

void asl_accumulator_func(void) {
    // ASL A is a 2-cycle instruction
    WAIT_READY_THEN_READ(cpu.pc, asl_a_wait);  // Dummy read
    cpu.a = op_asl(cpu.a);
    NEXT_INSTRUCTION(asl_a_fetch_wait);
}

void nop_instruction_func(void) {
    WAIT_READY_THEN_READ(cpu.pc, nop_wait);  // Dummy read
    NEXT_INSTRUCTION(nop_fetch_wait);
}

void brk_instruction_func(void) {
    cpu.pc++;  // Skip BRK signature byte
    // Push PC high byte
    cpu_push((cpu.pc >> 8) & 0xFF);
    // Push PC low byte
    cpu_push(cpu.pc & 0xFF);
    // Push status register with B flag set
    cpu_push(cpu.p | FLAG_B);
    // Set interrupt disable
    cpu.p |= FLAG_I;
    // Read IRQ vector
    cpu_read_cycle(0xFFFE);
    cpu.pc = bus_state.data;
    cpu_read_cycle(0xFFFF);
    cpu.pc |= (bus_state.data << 8);
    NEXT_INSTRUCTION(brk_fetch_wait);
}

void illegal_instruction_func(void) {
    // Handle illegal opcodes - just NOP for now
    NEXT_INSTRUCTION(illegal_fetch_wait);
}

void handle_interrupt_func(void) {
    // Interrupt handling logic
    if (bus_state.control_lines & NMI_LINE) {
        // Handle NMI - non-maskable
        cpu6510_nmi();
    } else if ((bus_state.control_lines & IRQ_LINE) && !cpu_get_flag(FLAG_I)) {
        // Handle IRQ - only if interrupt disable is clear
        cpu6510_irq();
    }
    NEXT_INSTRUCTION(interrupt_fetch_wait);
}


// ============================================================================
// CPU EXECUTION LOOP WITH FUNCTION POINTERS (Universal)
// ============================================================================
void cpu6510_execute(void) {  
    // Start execution - fetch first instruction
    if (unlikely(bus_state.control_lines & (IRQ_LINE | NMI_LINE))) {
        handle_interrupt_func();
        return;
    }
    WAIT_READY_THEN_READ(cpu.pc++, main_fetch_start);
    cpu.opcode = bus_state.data;
    instruction_table[cpu.opcode]();
}

// ============================================================================
// INSTRUCTION TABLE SETUP
// ============================================================================
void cpu6510_setup_opcode_table(void) {
    // Initialize function pointer table
    for (int i = 0; i < 256; i++) {
        instruction_table[i] = illegal_instruction_func;
    }
    
    // Map key instructions
    instruction_table[0xA9] = lda_immediate_func;
    instruction_table[0xA5] = lda_zero_page_func;
    instruction_table[0xAD] = lda_absolute_func;
    instruction_table[0x8D] = sta_absolute_func;
    instruction_table[0x85] = sta_zero_page_func;
    instruction_table[0x4C] = jmp_absolute_func;
    instruction_table[0xEA] = nop_instruction_func;
    instruction_table[0x00] = brk_instruction_func;
    instruction_table[0x69] = adc_immediate_func;
    instruction_table[0x29] = and_immediate_func;
    instruction_table[0x0A] = asl_accumulator_func;
}
