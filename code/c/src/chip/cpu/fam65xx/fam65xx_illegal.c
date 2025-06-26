// fam65xx_illegal.c - Refactored MOS6510 illegal opcodes for all 65xx family CPUs

#include "fam65xx_core.h"
#include "fam65xx_constants.h"

// ============================================================================
// ILLEGAL INSTRUCTION SPECIFIC OPERATIONS (inline for performance)
// ============================================================================

// ALR operation: AND then LSR
static inline void fam65xx_op_alr(fam65xx_t* cpu, uint8_t value) {
    cpu->a &= value;
    mos6510_set_flag(cpu, FLAG_C, cpu->a & 0x01);
    cpu->a >>= 1;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

// ANC operation: AND then copy N to C
static inline void fam65xx_op_anc(fam65xx_t* cpu, uint8_t value) {
    cpu->a &= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
    fam65xx_set_nz_flags(cpu, cpu->a & 0x80);
}

// ARR operation: AND then ROR with special V flag behavior
static inline void fam65xx_op_arr(fam65xx_t* cpu, uint8_t value) {
    cpu->a &= value;
    uint8_t old_carry = fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0;
    mos6510_set_flag(cpu, FLAG_C, cpu->a & 0x01);
    cpu->a = (cpu->a >> 1) | (old_carry << 7);
    fam65xx_set_nz_flags(cpu, cpu->a);
    // V flag behavior is complex for ARR
    mos6510_set_flag(cpu, FLAG_V, ((cpu->a >> 6) ^ (cpu->a >> 5)) & 1);
}

// AXS operation: (A & X) - immediate, store in X
static inline void fam65xx_op_axs(fam65xx_t* cpu, uint8_t value) {
    uint8_t temp = cpu->a & cpu->x;
    uint16_t result = temp - value;
    mos6510_set_flag(cpu, FLAG_C, result < 0x100);
    cpu->x = result & 0xFF;
    fam65xx_set_nz_flags(cpu, cpu->x);
}

// XAA operation: Transfer X to A, then AND with immediate
static inline void fam65xx_op_xaa(fam65xx_t* cpu, uint8_t value) {
    cpu->a = cpu->x;
    cpu->a &= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

// SLO register operation: ORA with result
static inline void fam65xx_op_slo_reg(fam65xx_t* cpu, uint8_t value) {
    cpu->a |= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

// RLA register operation: AND with result
static inline void fam65xx_op_rla_reg(fam65xx_t* cpu, uint8_t value) {
    cpu->a &= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

// SRE register operation: EOR with result
static inline void fam65xx_op_sre_reg(fam65xx_t* cpu, uint8_t value) {
    cpu->a ^= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}


// --- AHX ---
void fam65xx_op_ahx_indirect_y(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    cpu->address = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->address);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, (cpu->address + 1) & 0xFF);
    cpu->address = ((addr_hi << 8) | addr_lo) + cpu->y;
    uint8_t value = cpu->a & cpu->x;
    value &= ((cpu->address >> 8) + 1);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_ahx_absolute_y(fam65xx_t* cpu) {
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    FAM65XX_INTRA_CYCLE(cpu);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    cpu->address = ((addr_hi << 8) | addr_lo) + cpu->y;
    uint8_t value = cpu->a & cpu->x;
    value &= ((cpu->address >> 8) + 1);
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// --- ALR/ANC/ARR/AXS (immediate) unified with addr_op_helper ---
void fam65xx_op_alr_immediate(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_alr);
}
void fam65xx_op_anc_immediate(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_anc);
}
void fam65xx_op_arr_immediate(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_arr);
}
void fam65xx_op_axs_immediate(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_axs);
}

// --- DCP ---
// DCP: DEC memory, then CMP with A (illegal opcode)
static inline void fam65xx_op_dcp(fam65xx_t* cpu, uint8_t value) {
    value--;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_set_flag(cpu, FLAG_C, cpu->a >= value);
    fam65xx_set_nz_flags(cpu, (uint8_t)(cpu->a - value));
}

void fam65xx_op_dcp_zero_page(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_dcp);
}
void fam65xx_op_dcp_zero_page_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_dcp);
}
void fam65xx_op_dcp_absolute(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_dcp);
}
void fam65xx_op_dcp_absolute_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_dcp);
}
void fam65xx_op_dcp_absolute_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_dcp);
}
void fam65xx_op_dcp_indirect_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_dcp);
}
void fam65xx_op_dcp_indirect_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_dcp);
}

// --- ISC ---
// ISC: INC memory, then SBC with A (illegal opcode)
static inline void fam65xx_op_isc(fam65xx_t* cpu, uint8_t value) {
    value++;
    FAM65XX_INTRA_CYCLE(cpu);
    fam65xx_write_cycle(cpu, cpu->address, value);
    uint16_t temp = cpu->a - value - (fam65xx_get_flag(cpu, FLAG_C) ? 0 : 1);
    fam65xx_set_flag(cpu, FLAG_C, temp < 0x100);
    fam65xx_set_flag(cpu, FLAG_V, ((cpu->a ^ value) & (cpu->a ^ temp)) & 0x80);
    cpu->a = temp & 0xFF;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

void fam65xx_op_isc_zero_page(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_isc);
}
void fam65xx_op_isc_zero_page_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_isc);
}
void fam65xx_op_isc_absolute(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_isc);
}
void fam65xx_op_isc_absolute_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_isc);
}
void fam65xx_op_isc_absolute_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_isc);
}
void fam65xx_op_isc_indirect_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_isc);
}
void fam65xx_op_isc_indirect_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_isc);
}

// --- JAM ---
// ============================================================================
// JAM OPERATION (illegal)
// ============================================================================

// JAM - Halt and Catch Fire (illegal opcode)
void fam65xx_op_jam(fam65xx_t* cpu) {
    // TODO : Verify if this is the correct behavior for JAM
    (void)fam65xx_read_cycle(cpu, cpu->pc); // Read current PC
    // Decrement PC to stay on the same instruction
    cpu->pc--;
    
    // JAM instruction halts the CPU by entering an infinite loop
    // The program counter is not incremented
    // This effectively freezes the CPU until reset
    while (1) {
        if (fam65xx_system_lines_test(cpu, FAM65XX_MASK_NMI)) {
            fam65xx_op_nmi(cpu);
            break;
        }
        FAM65XX_INTRA_CYCLE(cpu);
    }
    FAM65XX_OPCODE_FOOTER(cpu);
}

// --- LAS ---
void fam65xx_op_las_absolute_y(fam65xx_t* cpu) {
    uint8_t value = fam65xx_addr_absy(cpu);
    value &= cpu->sp;
    cpu->a = value;
    cpu->x = value;
    cpu->sp = value;
    fam65xx_set_nz_flags(cpu, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// --- LAX ---
static inline void fam65xx_op_lax(fam65xx_t* cpu, uint8_t value) {
    cpu->a = value;
    cpu->x = value;
    fam65xx_set_nz_flags(cpu, value);
}

void fam65xx_op_lax_immediate(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_lax);
}
void fam65xx_op_lax_zero_page(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_lax);
}
void fam65xx_op_lax_zero_page_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zpy, fam65xx_op_lax);
}
void fam65xx_op_lax_absolute(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_lax);
}
void fam65xx_op_lax_absolute_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_lax);
}
void fam65xx_op_lax_indirect_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_lax);
}
void fam65xx_op_lax_indirect_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_lax);
}

// --- ILLEGAL NOPs (undocumented NOPs with various addressing modes) ---

// 1-byte NOP (implied)
void fam65xx_op_nop(fam65xx_t* cpu) {
    FAM65XX_OPCODE_FOOTER(cpu);
}

// 2-byte NOPs (zero page, immediate, etc)
void fam65xx_op_nop_imm(fam65xx_t* cpu) {
    (void)fam65xx_addr_imm(cpu);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_nop_zp(fam65xx_t* cpu) {
    (void)fam65xx_addr_zp(cpu);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_nop_zpx(fam65xx_t* cpu) {
    (void)fam65xx_addr_zpx(cpu);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_nop_zpy(fam65xx_t* cpu) {
    (void)fam65xx_addr_zpy(cpu);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// 3-byte NOPs (absolute, absx, absy)
void fam65xx_op_nop_abs(fam65xx_t* cpu) {
    (void)fam65xx_addr_abs(cpu);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_nop_absx(fam65xx_t* cpu) {
    (void)fam65xx_addr_absx(cpu);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_nop_absy(fam65xx_t* cpu) {
    (void)fam65xx_addr_absy(cpu);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// 2-byte NOPs (immediate, but some are "read and ignore" like $89)
void fam65xx_op_nop_imm_special(fam65xx_t* cpu) {
    (void)fam65xx_addr_imm(cpu);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// --- XAA (immediate) ---
void fam65xx_op_xaa_immediate(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_xaa);
}

// --- SLO (all memory addressing modes) ---
void fam65xx_op_slo_zero_page(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_slo_reg);
}
void fam65xx_op_slo_zero_page_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_slo_reg);
}
void fam65xx_op_slo_absolute(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_slo_reg);
}
void fam65xx_op_slo_absolute_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_slo_reg);
}
void fam65xx_op_slo_absolute_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_slo_reg);
}
void fam65xx_op_slo_indirect_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_slo_reg);
}
void fam65xx_op_slo_indirect_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_slo_reg);
}

// --- RLA (all memory addressing modes) ---
void fam65xx_op_rla_zero_page(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_rla_reg);
}
void fam65xx_op_rla_zero_page_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_rla_reg);
}
void fam65xx_op_rla_absolute(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_rla_reg);
}
void fam65xx_op_rla_absolute_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_rla_reg);
}
void fam65xx_op_rla_absolute_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_rla_reg);
}
void fam65xx_op_rla_indirect_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_rla_reg);
}
void fam65xx_op_rla_indirect_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_rla_reg);
}

// --- SRE (all memory addressing modes) ---
void fam65xx_op_sre_zero_page(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_sre_reg);
}
void fam65xx_op_sre_zero_page_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_sre_reg);
}
void fam65xx_op_sre_absolute(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_sre_reg);
}
void fam65xx_op_sre_absolute_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_sre_reg);
}
void fam65xx_op_sre_absolute_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_sre_reg);
}
void fam65xx_op_sre_indirect_x(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_sre_reg);
}
void fam65xx_op_sre_indirect_y(fam65xx_t* cpu) {
    fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_sre_reg);
}