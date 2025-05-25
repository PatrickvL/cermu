#include "cpu6510_core.h"

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
    
    cpu.total_cycles = 0;
    
    // Setup instruction table
    cpu6510_setup_opcode_table();
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
// ILLEGAL OPCODE OPERATIONS (ALPHABETICAL ORDER)
// ============================================================================

// AHX - A AND X AND (H+1) (unstable)
static inline uint8_t op_ahx(void) {
    return cpu.a & cpu.x & ((cpu.hi + 1) & 0xFF);
}

// ALR - AND and LSR
static inline void op_alr(uint8_t value) {
    cpu.a &= value;
    cpu_set_flag(FLAG_C, cpu.a & 0x01);
    cpu.a >>= 1;
    cpu_set_zn(cpu.a);
}

// ANC - AND with Carry
static inline void op_anc(uint8_t value) {
    cpu.a &= value;
    cpu_set_zn(cpu.a);
    cpu_set_flag(FLAG_C, cpu.a & 0x80);
}

// ARR - AND and ROR
static inline void op_arr(uint8_t value) {
    cpu.a &= value;
    cpu.a = (cpu.a >> 1) | (cpu_get_flag(FLAG_C) ? 0x80 : 0);
    cpu_set_zn(cpu.a);
    cpu_set_flag(FLAG_C, cpu.a & 0x40);
    cpu_set_flag(FLAG_V, ((cpu.a ^ (cpu.a >> 1)) & 0x40) != 0);
}

// AXS - A AND X minus value
static inline void op_axs(uint8_t value) {
    uint16_t temp = (cpu.a & cpu.x) - value;
    cpu_set_flag(FLAG_C, temp < 0x100);
    cpu.x = temp & 0xFF;
    cpu_set_zn(cpu.x);
}

// DCP - Decrement and Compare
static inline uint8_t op_dcp(uint8_t value) {
    value--;
    op_cmp(cpu.a, value);
    return value;
}

// ISC - Increment and Subtract with Carry
static inline uint8_t op_isc(uint8_t value) {
    value++;
    op_sbc(value);
    return value;
}

// LAS - Load A, X, S
static inline void op_las(uint8_t value) {
    cpu.a = value & cpu.sp;
    cpu.x = cpu.a;
    cpu.sp = cpu.a;
    cpu_set_zn(cpu.a);
}

// LAX - Load A and X
static inline void op_lax(uint8_t value) {
    cpu.a = value;
    cpu.x = value;
    cpu_set_zn(cpu.a);
}

// RLA - Rotate Left and AND
static inline uint8_t op_rla(uint8_t value) {
    value = op_rol(value);
    cpu.a &= value;
    cpu_set_zn(cpu.a);
    return value;
}

// RRA - Rotate Right and Add
static inline uint8_t op_rra(uint8_t value) {
    value = op_ror(value);
    op_adc(value);
    return value;
}

// SAX - Store A AND X
static inline uint8_t op_sax(void) {
    return cpu.a & cpu.x;
}

// SHX - Store X AND (H+1) (unstable)
static inline uint8_t op_shx(void) {
    return cpu.x & ((cpu.hi + 1) & 0xFF);
}

// SHY - Store Y AND (H+1) (unstable)
static inline uint8_t op_shy(void) {
    return cpu.y & ((cpu.hi + 1) & 0xFF);
}

// SLO - Shift Left and OR
static inline uint8_t op_slo(uint8_t value) {
    value = op_asl(value);
    cpu.a |= value;
    cpu_set_zn(cpu.a);
    return value;
}

// SRE - Shift Right and EOR
static inline uint8_t op_sre(uint8_t value) {
    value = op_lsr(value);
    cpu.a ^= value;
    cpu_set_zn(cpu.a);
    return value;
}

// TAS - Transfer A AND X to S (unstable)
static inline uint8_t op_tas(void) {
    cpu.sp = cpu.a & cpu.x;
    return cpu.a & cpu.x & ((cpu.hi + 1) & 0xFF);
}

// XAA - X AND A (unstable)
static inline void op_xaa(uint8_t value) {
    cpu.a = cpu.x & value;
    cpu_set_zn(cpu.a);
}