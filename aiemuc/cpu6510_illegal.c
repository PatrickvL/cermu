#include "cpu6510.h"

// ============================================================================
// MOS 6510 ILLEGAL/UNOFFICIAL INSTRUCTIONS
// ============================================================================
// Undocumented opcodes that combine operations or have unusual behavior

// AHX - Store A & X & high byte of address
void ahx_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ahx_izy_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, ahx_izy_wait2);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->address);
    CPU_READY_OR_STALL(cpu_dev, ahx_izy_wait3);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    uint8_t value = cpu_dev->a & cpu_dev->x & ((cpu_dev->address >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, ahx_izy_wait4);
    NEXT_INSTRUCTION(cpu_dev, ahx_izy_fetch_wait);
}

void ahx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ahx_absy_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, ahx_absy_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    uint8_t value = cpu_dev->a & cpu_dev->x & ((cpu_dev->address >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, ahx_absy_wait3);
    NEXT_INSTRUCTION(cpu_dev, ahx_absy_fetch_wait);
}

// ALR - AND then LSR (immediate mode only)
void alr_immediate_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_imm(cpu_dev);
    cpu_dev->a &= value;
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->a & 0x01);
    cpu_dev->a >>= 1;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, alr_imm_fetch_wait);
}

// ANC - AND then copy N to C (immediate mode only)
void anc_immediate_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_imm(cpu_dev);
    cpu_dev->a &= value;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->a & 0x80);
    NEXT_INSTRUCTION(cpu_dev, anc_imm_fetch_wait);
}

// ARR - AND then ROR (immediate mode only)
void arr_immediate_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_imm(cpu_dev);
    cpu_dev->a &= value;
    uint8_t old_carry = cpu_get_flag(cpu_dev, FLAG_C) ? 1 : 0;
    cpu_set_flag(cpu_dev, FLAG_C, cpu_dev->a & 0x01);
    cpu_dev->a = (cpu_dev->a >> 1) | (old_carry << 7);
    cpu_set_zn(cpu_dev, cpu_dev->a);
    // V flag behavior is complex for ARR
    cpu_set_flag(cpu_dev, FLAG_V, ((cpu_dev->a >> 6) ^ (cpu_dev->a >> 5)) & 1);
    NEXT_INSTRUCTION(cpu_dev, arr_imm_fetch_wait);
}

// AXS - (A & X) - immediate, store in X
void axs_immediate_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_imm(cpu_dev);
    uint8_t temp = cpu_dev->a & cpu_dev->x;
    uint16_t result = temp - value;
    cpu_set_flag(cpu_dev, FLAG_C, result < 0x100);
    cpu_dev->x = result & 0xFF;
    cpu_set_zn(cpu_dev, cpu_dev->x);
    NEXT_INSTRUCTION(cpu_dev, axs_imm_fetch_wait);
}

// DCP - DEC then CMP
void dcp_zero_page_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    value--;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dcp_zp_wait);
    op_cmp(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, dcp_zp_fetch_wait);
}

void dcp_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    value--;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dcp_zpx_wait);
    op_cmp(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, dcp_zpx_fetch_wait);
}

void dcp_absolute_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    value--;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dcp_abs_wait);
    op_cmp(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, dcp_abs_fetch_wait);
}

void dcp_absolute_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    value--;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dcp_absx_wait);
    op_cmp(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, dcp_absx_fetch_wait);
}

void dcp_absolute_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    value--;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dcp_absy_wait);
    op_cmp(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, dcp_absy_fetch_wait);
}

void dcp_indirect_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    value--;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dcp_izx_wait);
    op_cmp(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, dcp_izx_fetch_wait);
}

void dcp_indirect_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    value--;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, dcp_izy_wait);
    op_cmp(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, dcp_izy_fetch_wait);
}

// ISC - INC then SBC
void isc_zero_page_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    value++;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, isc_zp_wait);
    op_sbc(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, isc_zp_fetch_wait);
}

void isc_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    value++;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, isc_zpx_wait);
    op_sbc(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, isc_zpx_fetch_wait);
}

void isc_absolute_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    value++;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, isc_abs_wait);
    op_sbc(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, isc_abs_fetch_wait);
}

void isc_absolute_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    value++;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, isc_absx_wait);
    op_sbc(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, isc_absx_fetch_wait);
}

void isc_absolute_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    value++;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, isc_absy_wait);
    op_sbc(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, isc_absy_fetch_wait);
}

void isc_indirect_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    value++;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, isc_izx_wait);
    op_sbc(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, isc_izx_fetch_wait);
}

void isc_indirect_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    value++;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, isc_izy_wait);
    op_sbc(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, isc_izy_fetch_wait);
}

// JAM - Halt the processor (multiple opcodes)
void jam_func(cpu6510_state_t* cpu_dev) {
    // JAM instruction - CPU halts until reset
    // In a real implementation, this would halt the CPU
    // For emulation, we can either halt or treat as NOP
    while (1) {        // Wait for reset or NMI
        if (cpu_dev->bus->control_lines & NMI_LINE) {
            cpu6510_nmi(cpu_dev);
            break;
        }
    }
}

// LAS - Load A, X, and S from memory AND S
void las_absolute_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    value &= cpu_dev->sp;
    cpu_dev->a = value;
    cpu_dev->x = value;
    cpu_dev->sp = value;
    cpu_set_zn(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, las_absy_fetch_wait);
}

// LAX - Load A and X
void lax_immediate_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_imm(cpu_dev);
    cpu_dev->a = value;
    cpu_dev->x = value;
    cpu_set_zn(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, lax_imm_fetch_wait);
}

void lax_zero_page_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    cpu_dev->a = value;
    cpu_dev->x = value;
    cpu_set_zn(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, lax_zp_fetch_wait);
}

void lax_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpy(cpu_dev);
    cpu_dev->a = value;
    cpu_dev->x = value;
    cpu_set_zn(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, lax_zpy_fetch_wait);
}

void lax_absolute_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    cpu_dev->a = value;
    cpu_dev->x = value;
    cpu_set_zn(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, lax_abs_fetch_wait);
}

void lax_absolute_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    cpu_dev->a = value;
    cpu_dev->x = value;
    cpu_set_zn(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, lax_absy_fetch_wait);
}

void lax_indirect_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    cpu_dev->a = value;
    cpu_dev->x = value;
    cpu_set_zn(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, lax_izx_fetch_wait);
}

void lax_indirect_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    cpu_dev->a = value;
    cpu_dev->x = value;
    cpu_set_zn(cpu_dev, value);
    NEXT_INSTRUCTION(cpu_dev, lax_izy_fetch_wait);
}

// RLA - ROL then AND
void rla_zero_page_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    uint8_t result = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rla_zp_wait);
    cpu_dev->a &= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, rla_zp_fetch_wait);
}

void rla_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    uint8_t result = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rla_zpx_wait);
    cpu_dev->a &= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, rla_zpx_fetch_wait);
}

void rla_absolute_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    uint8_t result = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rla_abs_wait);
    cpu_dev->a &= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, rla_abs_fetch_wait);
}

void rla_absolute_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    uint8_t result = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rla_absx_wait);
    cpu_dev->a &= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, rla_absx_fetch_wait);
}

void rla_absolute_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    uint8_t result = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rla_absy_wait);
    cpu_dev->a &= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, rla_absy_fetch_wait);
}

void rla_indirect_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    uint8_t result = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rla_izx_wait);
    cpu_dev->a &= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, rla_izx_fetch_wait);
}

void rla_indirect_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    uint8_t result = op_rol(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rla_izy_wait);
    cpu_dev->a &= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, rla_izy_fetch_wait);
}

// RRA - ROR then ADC
void rra_zero_page_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    uint8_t result = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rra_zp_wait);
    op_adc(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, rra_zp_fetch_wait);
}

void rra_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    uint8_t result = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rra_zpx_wait);
    op_adc(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, rra_zpx_fetch_wait);
}

void rra_absolute_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    uint8_t result = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rra_abs_wait);
    op_adc(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, rra_abs_fetch_wait);
}

void rra_absolute_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    uint8_t result = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rra_absx_wait);
    op_adc(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, rra_absx_fetch_wait);
}

void rra_absolute_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    uint8_t result = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rra_absy_wait);
    op_adc(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, rra_absy_fetch_wait);
}

void rra_indirect_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    uint8_t result = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rra_izx_wait);
    op_adc(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, rra_izx_fetch_wait);
}

void rra_indirect_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    uint8_t result = op_ror(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, rra_izy_wait);
    op_adc(cpu_dev, result);
    NEXT_INSTRUCTION(cpu_dev, rra_izy_fetch_wait);
}

// SAX - Store A & X
void sax_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sax_zp_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x, sax_zp_wait2);
    NEXT_INSTRUCTION(cpu_dev, sax_zp_fetch_wait);
}

void sax_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sax_zpy_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sax_zpy_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->y) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x, sax_zpy_wait3);
    NEXT_INSTRUCTION(cpu_dev, sax_zpy_fetch_wait);
}

void sax_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sax_abs_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_READY_OR_STALL(cpu_dev, sax_abs_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x, sax_abs_wait3);
    NEXT_INSTRUCTION(cpu_dev, sax_abs_fetch_wait);
}

void sax_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, sax_izx_wait1);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, sax_izx_wait2);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, sax_izx_wait3);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->address);
    CPU_READY_OR_STALL(cpu_dev, sax_izx_wait4);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x, sax_izx_wait5);
    NEXT_INSTRUCTION(cpu_dev, sax_izx_fetch_wait);
}

// SHX - Store X & high byte of address + 1
void shx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, shx_absy_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, shx_absy_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    uint8_t value = cpu_dev->x & ((cpu_dev->address >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, shx_absy_wait3);
    NEXT_INSTRUCTION(cpu_dev, shx_absy_fetch_wait);
}

// SHY - Store Y & high byte of address + 1
void shy_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, shy_absx_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, shy_absx_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->x;
    uint8_t value = cpu_dev->y & ((cpu_dev->address >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, shy_absx_wait3);
    NEXT_INSTRUCTION(cpu_dev, shy_absx_fetch_wait);
}

// SLO - ASL then ORA
void slo_zero_page_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    uint8_t result = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, slo_zp_wait);
    cpu_dev->a |= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, slo_zp_fetch_wait);
}

void slo_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    uint8_t result = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, slo_zpx_wait);
    cpu_dev->a |= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, slo_zpx_fetch_wait);
}

void slo_absolute_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    uint8_t result = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, slo_abs_wait);
    cpu_dev->a |= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, slo_abs_fetch_wait);
}

void slo_absolute_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    uint8_t result = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, slo_absx_wait);
    cpu_dev->a |= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, slo_absx_fetch_wait);
}

void slo_absolute_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    uint8_t result = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, slo_absy_wait);
    cpu_dev->a |= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, slo_absy_fetch_wait);
}

void slo_indirect_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    uint8_t result = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, slo_izx_wait);
    cpu_dev->a |= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, slo_izx_fetch_wait);
}

void slo_indirect_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    uint8_t result = op_asl(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, slo_izy_wait);
    cpu_dev->a |= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, slo_izy_fetch_wait);
}

// SRE - LSR then EOR
void sre_zero_page_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    uint8_t result = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, sre_zp_wait);
    cpu_dev->a ^= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, sre_zp_fetch_wait);
}

void sre_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    uint8_t result = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, sre_zpx_wait);
    cpu_dev->a ^= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, sre_zpx_fetch_wait);
}

void sre_absolute_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    uint8_t result = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, sre_abs_wait);
    cpu_dev->a ^= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, sre_abs_fetch_wait);
}

void sre_absolute_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    uint8_t result = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, sre_absx_wait);
    cpu_dev->a ^= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, sre_absx_fetch_wait);
}

void sre_absolute_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    uint8_t result = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, sre_absy_wait);
    cpu_dev->a ^= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, sre_absy_fetch_wait);
}

void sre_indirect_x_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    uint8_t result = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, sre_izx_wait);
    cpu_dev->a ^= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, sre_izx_fetch_wait);
}

void sre_indirect_y_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    uint8_t result = op_lsr(cpu_dev, value);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, result, sre_izy_wait);
    cpu_dev->a ^= result;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, sre_izy_fetch_wait);
}

// TAS - Transfer A & X to S, then store A & X & high byte + 1
void tas_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, tas_absy_wait1);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, tas_absy_wait2);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    cpu_dev->sp = cpu_dev->a & cpu_dev->x;
    uint8_t value = cpu_dev->sp & ((cpu_dev->address >> 8) + 1);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->address, value, tas_absy_wait3);
    NEXT_INSTRUCTION(cpu_dev, tas_absy_fetch_wait);
}

// XAA - Transfer X to A, then AND with immediate
void xaa_immediate_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_imm(cpu_dev);
    cpu_dev->a = cpu_dev->x;
    cpu_dev->a &= value;
    cpu_set_zn(cpu_dev, cpu_dev->a);
    NEXT_INSTRUCTION(cpu_dev, xaa_imm_fetch_wait);
}