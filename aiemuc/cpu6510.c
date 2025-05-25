#include "cpu6510.h"

// ============================================================================
// MOS 6510 CPU STATE
// ============================================================================
cpu6510_state_t cpu;

// Direct threading instruction table
static const void* instruction_table[256];

// ============================================================================
// CPU INITIALIZATION
// ============================================================================
void cpu6510_init(void) {
    cpu.a = 0;
    cpu.x = 0;
    cpu.y = 0;
    cpu.sp = 0xFD;
    cpu.p = FLAG_U | FLAG_I;
    cpu.pc = 0;
    
    // 6510 I/O port initialization
    cpu.port_ddr = 0x2F;   // Default DDR
    cpu.port_data = 0x37;  // Default port data
    
    cpu.cycles = 0;
    cpu.page_crossed = false;
}

void cpu6510_reset(void) {
    // Read reset vector
    cpu_read_cycle(0xFFFC);
    uint8_t pcl = bus_state.data;
    cpu_read_cycle(0xFFFD);
    uint8_t pch = bus_state.data;
    cpu.pc = (pch << 8) | pcl;
    
    cpu.sp = 0xFD;
    cpu.p |= FLAG_I;  // Set interrupt disable
    cpu.cycles = 0;
}

// ============================================================================
// INSTRUCTION OPERATIONS
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
// CPU EXECUTION LOOP WITH DIRECT THREADING
// ============================================================================
void cpu6510_execute(void) {
    // Initialize instruction table
    for (int i = 0; i < 256; i++) {
        instruction_table[i] = &&illegal_instruction;
    }
    
    // Map key instructions (simplified set for demonstration)
    instruction_table[0xA9] = &&lda_immediate;
    instruction_table[0xA5] = &&lda_zero_page;
    instruction_table[0xAD] = &&lda_absolute;
    instruction_table[0x8D] = &&sta_absolute;
    instruction_table[0x85] = &&sta_zero_page;
    instruction_table[0x4C] = &&jmp_absolute;
    instruction_table[0xEA] = &&nop_instruction;
    instruction_table[0x00] = &&brk_instruction;
    instruction_table[0x69] = &&adc_immediate;
    instruction_table[0x29] = &&and_immediate;
    instruction_table[0x0A] = &&asl_accumulator;
    
    // Start execution
    NEXT_INSTRUCTION(main_fetch_wait);

    // ========================================================================
    // INSTRUCTION IMPLEMENTATIONS
    // ========================================================================
    
    lda_immediate:
        WAIT_READY_THEN_READ(cpu.pc++, lda_imm_wait);
        op_lda(bus_state.data);
        NEXT_INSTRUCTION(lda_imm_fetch_wait);

    lda_zero_page:
        WAIT_READY_THEN_READ(cpu.pc++, lda_zp_wait1);
        cpu.addr_abs = bus_state.data;
        WAIT_READY_THEN_READ(cpu.addr_abs, lda_zp_wait2);
        op_lda(bus_state.data);
        NEXT_INSTRUCTION(lda_zp_fetch_wait);

    lda_absolute:
        WAIT_READY_THEN_READ(cpu.pc++, lda_abs_wait1);
        cpu.addr_abs = bus_state.data;
        WAIT_READY_THEN_READ(cpu.pc++, lda_abs_wait2);
        cpu.addr_abs |= (bus_state.data << 8);
        WAIT_READY_THEN_READ(cpu.addr_abs, lda_abs_wait3);
        op_lda(bus_state.data);
        NEXT_INSTRUCTION(lda_abs_fetch_wait);

    sta_absolute:
        WAIT_READY_THEN_READ(cpu.pc++, sta_abs_wait1);
        cpu.addr_abs = bus_state.data;
        WAIT_READY_THEN_READ(cpu.pc++, sta_abs_wait2);
        cpu.addr_abs |= (bus_state.data << 8);
        WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_abs_wait3);
        NEXT_INSTRUCTION(sta_abs_fetch_wait);

    sta_zero_page:
        WAIT_READY_THEN_READ(cpu.pc++, sta_zp_wait1);
        cpu.addr_abs = bus_state.data;
        WAIT_READY_THEN_WRITE(cpu.addr_abs, cpu.a, sta_zp_wait2);
        NEXT_INSTRUCTION(sta_zp_fetch_wait);

    jmp_absolute:
        WAIT_READY_THEN_READ(cpu.pc++, jmp_abs_wait1);
        cpu.addr_abs = bus_state.data;
        WAIT_READY_THEN_READ(cpu.pc++, jmp_abs_wait2);
        cpu.addr_abs |= (bus_state.data << 8);
        cpu.pc = cpu.addr_abs;
        NEXT_INSTRUCTION(jmp_abs_fetch_wait);

    adc_immediate:
        WAIT_READY_THEN_READ(cpu.pc++, adc_imm_wait);
        op_adc(bus_state.data);
        NEXT_INSTRUCTION(adc_imm_fetch_wait);

    and_immediate:
        WAIT_READY_THEN_READ(cpu.pc++, and_imm_wait);
        op_and(bus_state.data);
        NEXT_INSTRUCTION(and_imm_fetch_wait);

    asl_accumulator:
        // ASL A is a 2-cycle instruction
        WAIT_READY_THEN_READ(cpu.pc, asl_a_wait);  // Dummy read
        cpu.a = op_asl(cpu.a);
        NEXT_INSTRUCTION(asl_a_fetch_wait);

    nop_instruction:
        WAIT_READY_THEN_READ(cpu.pc, nop_wait);  // Dummy read
        NEXT_INSTRUCTION(nop_fetch_wait);

    brk_instruction:
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

    illegal_instruction:
        // Handle illegal opcodes - just NOP for now
        NEXT_INSTRUCTION(illegal_fetch_wait);

    handle_interrupt:
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