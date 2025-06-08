#include "mos6510.h"

// ============================================================================
// MOS 6510 ILLEGAL/UNOFFICIAL INSTRUCTIONS
// ============================================================================
// Undocumented opcodes that combine operations or have unusual behavior

// AHX - Store A & X & high byte of address
void ahx_indirect_y_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    cpu->address = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->address);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, (cpu->address + 1) & 0xFF);
    cpu->address = ((addr_hi << 8) | addr_lo) + cpu->y;
    uint8_t value = cpu->a & cpu->x;
    mos6510_complex_store(cpu, value);
}

void ahx_absolute_y_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = ((addr_hi << 8) | addr_lo) + cpu->y;
    uint8_t value = cpu->a & cpu->x;
    mos6510_complex_store(cpu, value);
}

// ALR - AND then LSR (immediate mode only)
void alr_immediate_func(mos6510_t* cpu) {
    mos6510_immediate_accumulator_op(cpu, op_alr);
}

// ANC - AND then copy N to C (immediate mode only)
void anc_immediate_func(mos6510_t* cpu) {
    mos6510_immediate_accumulator_op(cpu, op_anc);
}

// ARR - AND then ROR (immediate mode only)
void arr_immediate_func(mos6510_t* cpu) {
    mos6510_immediate_accumulator_op(cpu, op_arr);
}

// AXS - (A & X) - immediate, store in X
void axs_immediate_func(mos6510_t* cpu) {
    mos6510_immediate_accumulator_op(cpu, op_axs);
}

// DCP - DEC then CMP
void dcp_zero_page_func(mos6510_t* cpu) {
    uint8_t value = addr_zp(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, -1, op_cmp);
}

void dcp_zero_page_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, -1, op_cmp);
}

void dcp_absolute_func(mos6510_t* cpu) {
    uint8_t value = addr_abs(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, -1, op_cmp);
}

void dcp_absolute_x_func(mos6510_t* cpu) {
    uint8_t value = addr_absx(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, -1, op_cmp);
}

void dcp_absolute_y_func(mos6510_t* cpu) {
    uint8_t value = addr_absy(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, -1, op_cmp);
}

void dcp_indirect_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx_ind(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, -1, op_cmp);
}

void dcp_indirect_y_func(mos6510_t* cpu) {
    uint8_t value = addr_zp_ind_y(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, -1, op_cmp);
}

// ISC - INC then SBC
void isc_zero_page_func(mos6510_t* cpu) {
    uint8_t value = addr_zp(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, 1, op_sbc);
}

void isc_zero_page_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, 1, op_sbc);
}

void isc_absolute_func(mos6510_t* cpu) {
    uint8_t value = addr_abs(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, 1, op_sbc);
}

void isc_absolute_x_func(mos6510_t* cpu) {
    uint8_t value = addr_absx(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, 1, op_sbc);
}

void isc_absolute_y_func(mos6510_t* cpu) {
    uint8_t value = addr_absy(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, 1, op_sbc);
}

void isc_indirect_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx_ind(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, 1, op_sbc);
}

void isc_indirect_y_func(mos6510_t* cpu) {
    uint8_t value = addr_zp_ind_y(cpu);
    mos6510_illegal_inc_dec_combo(cpu, value, 1, op_sbc);
}

// JAM - Halt the processor (multiple opcodes)
void jam_func(mos6510_t* cpu) {
    // JAM instruction - CPU halts until reset or NMI
    // Loop until we get an NMI or reset signal
    while (1) {
        // Use proper SYS_LINES_TEST macros - no runtime interface checks needed
        if (SYS_LINES_TEST(cpu->system_lines, SYS_MASK_NMI)) {
            mos6510_nmi(cpu);
            break;
        }
        // Allow non-CPU cycles during halt
        CPU_BUS_CYCLE(cpu);
    }
    CPU_OPCODE_FOOTER(cpu);
}

// LAS - Load A, X, and S from memory AND S
void las_absolute_y_func(mos6510_t* cpu) {
    uint8_t value = addr_absy(cpu);
    value &= cpu->sp;
    cpu->a = value;
    cpu->x = value;
    cpu->sp = value;
    mos6510_set_zn(cpu, value);
    CPU_OPCODE_FOOTER(cpu);
}

// LAX - Load A and X
void lax_immediate_func(mos6510_t* cpu) {
    uint8_t value = addr_imm(cpu);
    cpu->a = value;
    cpu->x = value;
    mos6510_set_zn(cpu, value);
    CPU_OPCODE_FOOTER(cpu);
}

void lax_zero_page_func(mos6510_t* cpu) {
    uint8_t value = addr_zp(cpu);
    mos6510_load_a_and_x(cpu, value);
}

void lax_zero_page_y_func(mos6510_t* cpu) {
    uint8_t value = addr_zpy(cpu);
    mos6510_load_a_and_x(cpu, value);
}

void lax_absolute_func(mos6510_t* cpu) {
    uint8_t value = addr_abs(cpu);
    mos6510_load_a_and_x(cpu, value);
}

void lax_absolute_y_func(mos6510_t* cpu) {
    uint8_t value = addr_absy(cpu);
    mos6510_load_a_and_x(cpu, value);
}

void lax_indirect_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx_ind(cpu);
    mos6510_load_a_and_x(cpu, value);
}

void lax_indirect_y_func(mos6510_t* cpu) {
    uint8_t value = addr_zp_ind_y(cpu);
    mos6510_load_a_and_x(cpu, value);
}

// RLA - ROL then AND
void rla_zero_page_func(mos6510_t* cpu) {
    uint8_t value = addr_zp(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_rol, op_rla_reg);
}

void rla_zero_page_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_rol, op_rla_reg);
}

void rla_absolute_func(mos6510_t* cpu) {
    uint8_t value = addr_abs(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_rol, op_rla_reg);
}

void rla_absolute_x_func(mos6510_t* cpu) {
    uint8_t value = addr_absx(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_rol, op_rla_reg);
}

void rla_absolute_y_func(mos6510_t* cpu) {
    uint8_t value = addr_absy(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_rol, op_rla_reg);
}

void rla_indirect_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx_ind(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_rol, op_rla_reg);
}

void rla_indirect_y_func(mos6510_t* cpu) {
    uint8_t value = addr_zp_ind_y(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_rol, op_rla_reg);
}

// RRA - ROR then ADC
void rra_zero_page_func(mos6510_t* cpu) {
    uint8_t value = addr_zp(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_ror, op_adc);
}

void rra_zero_page_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_ror, op_adc);
}

void rra_absolute_func(mos6510_t* cpu) {
    uint8_t value = addr_abs(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_ror, op_adc);
}

void rra_absolute_x_func(mos6510_t* cpu) {
    uint8_t value = addr_absx(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_ror, op_adc);
}

void rra_absolute_y_func(mos6510_t* cpu) {
    uint8_t value = addr_absy(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_ror, op_adc);
}

void rra_indirect_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx_ind(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_ror, op_adc);
}

void rra_indirect_y_func(mos6510_t* cpu) {
    uint8_t value = addr_zp_ind_y(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_ror, op_adc);
}

// SAX - Store A & X
void sax_zero_page_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    cpu->address = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, cpu->a & cpu->x);
    CPU_OPCODE_FOOTER(cpu);
}

void sax_zero_page_y_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    cpu->address = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address);  // Dummy read
    cpu->address = (cpu->address + cpu->y) & 0xFF;
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, cpu->a & cpu->x);
    CPU_OPCODE_FOOTER(cpu);
}

void sax_absolute_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = addr_lo;
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, cpu->a & cpu->x);
    CPU_OPCODE_FOOTER(cpu);
}

void sax_indirect_x_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    cpu->address = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    (void)mos6510_read_cycle(cpu, cpu->address);  // Dummy read
    cpu->address = (cpu->address + cpu->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->address);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, (cpu->address + 1) & 0xFF);
    cpu->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu);
    mos6510_write_cycle(cpu, cpu->address, cpu->a & cpu->x);
    CPU_OPCODE_FOOTER(cpu);
}

// SHX - Store X & high byte of address + 1
void shx_absolute_y_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = ((addr_hi << 8) | addr_lo) + cpu->y;
    mos6510_complex_store(cpu, cpu->x);
}

// SHY - Store Y & high byte of address + 1
void shy_absolute_x_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = ((addr_hi << 8) | addr_lo) + cpu->x;
    mos6510_complex_store(cpu, cpu->y);
}

// SLO - ASL then ORA
void slo_zero_page_func(mos6510_t* cpu) {
    uint8_t value = addr_zp(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_asl, op_slo_reg);
}

void slo_zero_page_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_asl, op_slo_reg);
}

void slo_absolute_func(mos6510_t* cpu) {
    uint8_t value = addr_abs(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_asl, op_slo_reg);
}

void slo_absolute_x_func(mos6510_t* cpu) {
    uint8_t value = addr_absx(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_asl, op_slo_reg);
}

void slo_absolute_y_func(mos6510_t* cpu) {
    uint8_t value = addr_absy(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_asl, op_slo_reg);
}

void slo_indirect_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx_ind(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_asl, op_slo_reg);
}

void slo_indirect_y_func(mos6510_t* cpu) {
    uint8_t value = addr_zp_ind_y(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_asl, op_slo_reg);
}

// SRE - LSR then EOR
void sre_zero_page_func(mos6510_t* cpu) {
    uint8_t value = addr_zp(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_lsr, op_sre_reg);
}

void sre_zero_page_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_lsr, op_sre_reg);
}

void sre_absolute_func(mos6510_t* cpu) {
    uint8_t value = addr_abs(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_lsr, op_sre_reg);
}

void sre_absolute_x_func(mos6510_t* cpu) {
    uint8_t value = addr_absx(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_lsr, op_sre_reg);
}

void sre_absolute_y_func(mos6510_t* cpu) {
    uint8_t value = addr_absy(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_lsr, op_sre_reg);
}

void sre_indirect_x_func(mos6510_t* cpu) {
    uint8_t value = addr_zpx_ind(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_lsr, op_sre_reg);
}

void sre_indirect_y_func(mos6510_t* cpu) {
    uint8_t value = addr_zp_ind_y(cpu);
    mos6510_illegal_rmw_combo(cpu, value, op_lsr, op_sre_reg);
}

// TAS - Transfer A & X to S, then store A & X & high byte + 1
void tas_absolute_y_func(mos6510_t* cpu) {
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6510_read_cycle(cpu, cpu->pc++);
    CPU_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6510_read_cycle(cpu, cpu->pc++);
    cpu->address = ((addr_hi << 8) | addr_lo) + cpu->y;
    cpu->sp = cpu->a & cpu->x;
    mos6510_complex_store(cpu, cpu->sp);
}

// XAA - Transfer X to A, then AND with immediate
void xaa_immediate_func(mos6510_t* cpu) {
    mos6510_immediate_accumulator_op(cpu, op_xaa);
}