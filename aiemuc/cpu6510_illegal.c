#include "cpu6510.h"

// ============================================================================
// MOS 6510 ILLEGAL/UNOFFICIAL INSTRUCTIONS
// ============================================================================
// Undocumented opcodes that combine operations or have unusual behavior

// AHX - Store A & X & high byte of address
void ahx_indirect_y_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    uint8_t value = cpu_dev->a & cpu_dev->x & ((cpu_dev->address >> 8) + 1);
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void ahx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    uint8_t value = cpu_dev->a & cpu_dev->x & ((cpu_dev->address >> 8) + 1);
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// ALR - AND then LSR (immediate mode only)
void alr_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_immediate_accumulator_op(cpu_dev, op_alr);
}

// ANC - AND then copy N to C (immediate mode only)
void anc_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_immediate_accumulator_op(cpu_dev, op_anc);
}

// ARR - AND then ROR (immediate mode only)
void arr_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_immediate_accumulator_op(cpu_dev, op_arr);
}

// AXS - (A & X) - immediate, store in X
void axs_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_immediate_accumulator_op(cpu_dev, op_axs);
}

// DCP - DEC then CMP
void dcp_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_zp, -1, op_cmp);
}

void dcp_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_zpx, -1, op_cmp);
}

void dcp_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_abs, -1, op_cmp);
}

void dcp_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_absx, -1, op_cmp);
}

void dcp_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_absy, -1, op_cmp);
}

void dcp_indirect_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_zpx_ind, -1, op_cmp);
}

void dcp_indirect_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_zp_ind_y, -1, op_cmp);
}

// ISC - INC then SBC
void isc_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_zp, 1, op_sbc_void);
}

void isc_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_zpx, 1, op_sbc_void);
}

void isc_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_abs, 1, op_sbc_void);
}

void isc_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_absx, 1, op_sbc_void);
}

void isc_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_absy, 1, op_sbc_void);
}

void isc_indirect_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_zpx_ind, 1, op_sbc_void);
}

void isc_indirect_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_inc_dec_combo(cpu_dev, addr_zp_ind_y, 1, op_sbc_void);
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
    CPU_OPCODE_FOOTER(cpu_dev);
}

// LAX - Load A and X
void lax_immediate_func(cpu6510_state_t* cpu_dev) {
    uint8_t value = addr_imm(cpu_dev);
    cpu_dev->a = value;
    cpu_dev->x = value;
    cpu_set_zn(cpu_dev, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void lax_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_load_a_and_x(cpu_dev, addr_zp);
}

void lax_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    cpu_load_a_and_x(cpu_dev, addr_zpy);
}

void lax_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_load_a_and_x(cpu_dev, addr_abs);
}

void lax_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_load_a_and_x(cpu_dev, addr_absy);
}

void lax_indirect_x_func(cpu6510_state_t* cpu_dev) {
    cpu_load_a_and_x(cpu_dev, addr_zpx_ind);
}

void lax_indirect_y_func(cpu6510_state_t* cpu_dev) {
    cpu_load_a_and_x(cpu_dev, addr_zp_ind_y);
}

// RLA - ROL then AND
void rla_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zp, op_rol, op_rla_reg);
}

void rla_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zpx, op_rol, op_rla_reg);
}

void rla_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_abs, op_rol, op_rla_reg);
}

void rla_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_absx, op_rol, op_rla_reg);
}

void rla_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_absy, op_rol, op_rla_reg);
}

void rla_indirect_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zpx_ind, op_rol, op_rla_reg);
}

void rla_indirect_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zp_ind_y, op_rol, op_rla_reg);
}

// RRA - ROR then ADC
void rra_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zp, op_ror, op_adc_void);
}

void rra_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zpx, op_ror, op_adc_void);
}

void rra_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_abs, op_ror, op_adc_void);
}

void rra_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_absx, op_ror, op_adc_void);
}

void rra_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_absy, op_ror, op_adc_void);
}

void rra_indirect_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zpx_ind, op_ror, op_adc_void);
}

void rra_indirect_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zp_ind_y, op_ror, op_adc_void);
}

// SAX - Store A & X
void sax_zero_page_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sax_zero_page_y_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->y) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sax_absolute_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address |= (addr_hi << 8);
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

void sax_indirect_x_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_dev->address = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    (void)cpu_read_cycle(cpu_dev, cpu_dev->address);  // Dummy read
    cpu_dev->address = (cpu_dev->address + cpu_dev->x) & 0xFF;
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->address);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, (cpu_dev->address + 1) & 0xFF);
    cpu_dev->address = (addr_hi << 8) | addr_lo;
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_write_cycle(cpu_dev, cpu_dev->address, cpu_dev->a & cpu_dev->x);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// SHX - Store X & high byte of address + 1
void shx_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    uint8_t value = cpu_dev->x & ((cpu_dev->address >> 8) + 1);
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// SHY - Store Y & high byte of address + 1
void shy_absolute_x_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->x;
    uint8_t value = cpu_dev->y & ((cpu_dev->address >> 8) + 1);
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// SLO - ASL then ORA
void slo_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zp, op_asl, op_slo_reg);
}

void slo_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zpx, op_asl, op_slo_reg);
}

void slo_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_abs, op_asl, op_slo_reg);
}

void slo_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_absx, op_asl, op_slo_reg);
}

void slo_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_absy, op_asl, op_slo_reg);
}

void slo_indirect_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zpx_ind, op_asl, op_slo_reg);
}

void slo_indirect_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zp_ind_y, op_asl, op_slo_reg);
}

// SRE - LSR then EOR
void sre_zero_page_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zp, op_lsr, op_sre_reg);
}

void sre_zero_page_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zpx, op_lsr, op_sre_reg);
}

void sre_absolute_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_abs, op_lsr, op_sre_reg);
}

void sre_absolute_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_absx, op_lsr, op_sre_reg);
}

void sre_absolute_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_absy, op_lsr, op_sre_reg);
}

void sre_indirect_x_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zpx_ind, op_lsr, op_sre_reg);
}

void sre_indirect_y_func(cpu6510_state_t* cpu_dev) {
    cpu_illegal_rmw_combo(cpu_dev, addr_zp_ind_y, op_lsr, op_sre_reg);
}

// TAS - Transfer A & X to S, then store A & X & high byte + 1
void tas_absolute_y_func(cpu6510_state_t* cpu_dev) {
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_INTRA_CYCLE(cpu_dev);
    uint8_t addr_hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->address = ((addr_hi << 8) | addr_lo) + cpu_dev->y;
    cpu_dev->sp = cpu_dev->a & cpu_dev->x;
    uint8_t value = cpu_dev->sp & ((cpu_dev->address >> 8) + 1);
    CPU_INTRA_CYCLE(cpu_dev);
    cpu_write_cycle(cpu_dev, cpu_dev->address, value);
    CPU_OPCODE_FOOTER(cpu_dev);
}

// XAA - Transfer X to A, then AND with immediate
void xaa_immediate_func(cpu6510_state_t* cpu_dev) {
    cpu_immediate_accumulator_op(cpu_dev, op_xaa);
}