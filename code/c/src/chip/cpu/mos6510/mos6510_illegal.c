#include "mos6510.h"

// ============================================================================
// MOS 6510 ILLEGAL/UNOFFICIAL INSTRUCTIONS
// ============================================================================
// Undocumented opcodes that combine operations or have unusual behavior

// AHX - Store A & X & high byte of address
void ahx_indirect_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    uint8_t value = cpu_dev->a & cpu_dev->x;
    mos6510_complex_store(cpu_dev, value);
}

void ahx_absolute_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    uint8_t value = cpu_dev->a & cpu_dev->x;
    mos6510_complex_store(cpu_dev, value);
}

// ALR - AND then LSR (immediate mode only)
void alr_immediate_func(mos6510_t* cpu_dev) {
    mos6510_immediate_accumulator_op(cpu_dev, op_alr);
}

// ANC - AND then copy N to C (immediate mode only)
void anc_immediate_func(mos6510_t* cpu_dev) {
    mos6510_immediate_accumulator_op(cpu_dev, op_anc);
}

// ARR - AND then ROR (immediate mode only)
void arr_immediate_func(mos6510_t* cpu_dev) {
    mos6510_immediate_accumulator_op(cpu_dev, op_arr);
}

// AXS - (A & X) - immediate, store in X
void axs_immediate_func(mos6510_t* cpu_dev) {
    mos6510_immediate_accumulator_op(cpu_dev, op_axs);
}

// DCP - DEC then CMP
void dcp_zero_page_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, -1, op_cmp);
}

void dcp_zero_page_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, -1, op_cmp);
}

void dcp_absolute_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, -1, op_cmp);
}

void dcp_absolute_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, -1, op_cmp);
}

void dcp_absolute_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, -1, op_cmp);
}

void dcp_indirect_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, -1, op_cmp);
}

void dcp_indirect_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, -1, op_cmp);
}

// ISC - INC then SBC
void isc_zero_page_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, 1, op_sbc);
}

void isc_zero_page_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, 1, op_sbc);
}

void isc_absolute_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, 1, op_sbc);
}

void isc_absolute_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, 1, op_sbc);
}

void isc_absolute_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, 1, op_sbc);
}

void isc_indirect_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, 1, op_sbc);
}

void isc_indirect_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    mos6510_illegal_inc_dec_combo(cpu_dev, value, 1, op_sbc);
}

// JAM - Halt the processor (multiple opcodes)
void jam_func(mos6510_t* cpu_dev) {
    // JAM instruction - CPU halts until reset
    // In a real implementation, this would halt the CPU
    // For emulation, we can either halt or treat as NOP
    while (1) {        // Wait for reset or NMI
        if (CPU_CONTROL_LINES(cpu_dev) & NMI_LINE) {
            mos6510_nmi(cpu_dev);
            break;
        }
    }
}

// LAS - Load A, X, and S from memory AND S
void las_absolute_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    value &= cpu_dev->sp;
    cpu_dev->a = value;
    cpu_dev->x = value;
    cpu_dev->sp = value;
    mos6510_set_zn(cpu_dev, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// LAX - Load A and X
void lax_immediate_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_imm(cpu_dev);
    cpu_dev->a = value;
    cpu_dev->x = value;
    mos6510_set_zn(cpu_dev, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lax_zero_page_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    mos6510_load_a_and_x(cpu_dev, value);
}

void lax_zero_page_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpy(cpu_dev);
    mos6510_load_a_and_x(cpu_dev, value);
}

void lax_absolute_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    mos6510_load_a_and_x(cpu_dev, value);
}

void lax_absolute_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    mos6510_load_a_and_x(cpu_dev, value);
}

void lax_indirect_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    mos6510_load_a_and_x(cpu_dev, value);
}

void lax_indirect_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    mos6510_load_a_and_x(cpu_dev, value);
}

// RLA - ROL then AND
void rla_zero_page_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_rol, op_rla_reg);
}

void rla_zero_page_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_rol, op_rla_reg);
}

void rla_absolute_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_rol, op_rla_reg);
}

void rla_absolute_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_rol, op_rla_reg);
}

void rla_absolute_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_rol, op_rla_reg);
}

void rla_indirect_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_rol, op_rla_reg);
}

void rla_indirect_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_rol, op_rla_reg);
}

// RRA - ROR then ADC
void rra_zero_page_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_ror, op_adc);
}

void rra_zero_page_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_ror, op_adc);
}

void rra_absolute_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_ror, op_adc);
}

void rra_absolute_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_ror, op_adc);
}

void rra_absolute_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_ror, op_adc);
}

void rra_indirect_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_ror, op_adc);
}

void rra_indirect_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_ror, op_adc);
}

// SAX - Store A & X
void sax_zero_page_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sax_zero_page_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->y) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sax_absolute_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sax_indirect_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)mos6510_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    mos6510_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// SHX - Store X & high byte of address + 1
void shx_absolute_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    mos6510_complex_store(cpu_dev, cpu_dev->x);
}

// SHY - Store Y & high byte of address + 1
void shy_absolute_x_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->x;
    mos6510_complex_store(cpu_dev, cpu_dev->y);
}

// SLO - ASL then ORA
void slo_zero_page_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_asl, op_slo_reg);
}

void slo_zero_page_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_asl, op_slo_reg);
}

void slo_absolute_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_asl, op_slo_reg);
}

void slo_absolute_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_asl, op_slo_reg);
}

void slo_absolute_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_asl, op_slo_reg);
}

void slo_indirect_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_asl, op_slo_reg);
}

void slo_indirect_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_asl, op_slo_reg);
}

// SRE - LSR then EOR
void sre_zero_page_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_lsr, op_sre_reg);
}

void sre_zero_page_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_lsr, op_sre_reg);
}

void sre_absolute_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_abs(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_lsr, op_sre_reg);
}

void sre_absolute_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absx(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_lsr, op_sre_reg);
}

void sre_absolute_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_absy(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_lsr, op_sre_reg);
}

void sre_indirect_x_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zpx_ind(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_lsr, op_sre_reg);
}

void sre_indirect_y_func(mos6510_t* cpu_dev) {
    uint8_t value = addr_zp_ind_y(cpu_dev);
    mos6510_illegal_rmw_combo(cpu_dev, value, op_lsr, op_sre_reg);
}

// TAS - Transfer A & X to S, then store A & X & high byte + 1
void tas_absolute_y_func(mos6510_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = mos6510_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    cpu_dev->sp = cpu_dev->a & cpu_dev->x;
    mos6510_complex_store(cpu_dev, cpu_dev->sp);
}

// XAA - Transfer X to A, then AND with immediate
void xaa_immediate_func(mos6510_t* cpu_dev) {
    mos6510_immediate_accumulator_op(cpu_dev, op_xaa);
}