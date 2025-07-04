// fam65xx_illegal.inc - Refactored MOS6510 illegal opcodes for all 65xx family CPUs

#include "fam65xx_core.h"

// ============================================================================
// ILLEGAL INSTRUCTION SPECIFIC OPERATIONS (inline for performance)
// ============================================================================

// ============================================================================
// ILLEGAL OPCODE HANDLERS
// ============================================================================

// --- AHX ---
void fam65xx_op_ahx_indirect_y(fam65xx_t* cpu) {
    uint16_t addr_ptr = fam65xx_read_cycle(cpu, cpu->pc++);
    uint8_t addr_lo = fam65xx_read_cycle(cpu, addr_ptr);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, (addr_ptr + 1) & 0xFF);
    uint16_t addr = ((addr_hi << 8) | addr_lo) + cpu->y;
    uint8_t value = cpu->a & cpu->x & ((addr >> 8) + 1);
    fam65xx_write_cycle(cpu, addr, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

void fam65xx_op_ahx_absolute_y(fam65xx_t* cpu) {
    uint8_t addr_lo = fam65xx_read_cycle(cpu, cpu->pc++);
    uint8_t addr_hi = fam65xx_read_cycle(cpu, cpu->pc++);
    uint16_t addr = ((addr_hi << 8) | addr_lo) + cpu->y;
    uint8_t value = cpu->a & cpu->x & ((addr >> 8) + 1);
    fam65xx_write_cycle(cpu, addr, value);
    FAM65XX_OPCODE_FOOTER(cpu);
}

// --- ALR/ANC/ARR/AXS (immediate) ---
// ALR operation: AND then LSR
static inline void fam65xx_op_alr(fam65xx_t* cpu, uint8_t value) {
    cpu->a &= value;
    fam65xx_set_flag(cpu, FLAG_C, cpu->a & 0x01);
    cpu->a >>= 1;
    fam65xx_set_nz_flags(cpu, cpu->a);
}
// ANC operation: AND then copy N to C
static inline void fam65xx_op_anc(fam65xx_t* cpu, uint8_t value) {
    cpu->a &= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
    fam65xx_set_flag(cpu, FLAG_C, (cpu->p & FLAG_N) != 0);
}

// ARR operation: AND then ROR with special V flag behavior
static inline void fam65xx_op_arr(fam65xx_t* cpu, uint8_t value) {
    cpu->a &= value;
    uint8_t old_carry = fam65xx_get_flag(cpu, FLAG_C) ? 1 : 0;
    fam65xx_set_flag(cpu, FLAG_C, (cpu->a >> 7) & 0x01);
    cpu->a = (cpu->a >> 1) | (old_carry << 7);
    fam65xx_set_nz_flags(cpu, cpu->a);
    fam65xx_set_flag(cpu, FLAG_V, ((cpu->a >> 6) ^ (cpu->a >> 5)) & 1);
}

// AXS operation: (A & X) - immediate, store in X
static inline void fam65xx_op_axs(fam65xx_t* cpu, uint8_t value) {
    uint8_t temp = cpu->a & cpu->x;
    uint16_t result = temp - value;
    fam65xx_set_flag(cpu, FLAG_C, result < 0x100);
    cpu->x = result & 0xFF;
    fam65xx_set_nz_flags(cpu, cpu->x);
}

void fam65xx_op_alr_immediate(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_alr); }
void fam65xx_op_anc_immediate(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_anc); }
void fam65xx_op_arr_immediate(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_arr); }
void fam65xx_op_axs_immediate(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_axs); }

// --- DCP ---
// DCP: DEC memory, then CMP with A (illegal opcode)
static inline void fam65xx_op_dcp(fam65xx_t* cpu, uint8_t value) {
    value--;
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_set_flag(cpu, FLAG_C, cpu->a >= value);
    fam65xx_set_nz_flags(cpu, (uint8_t)(cpu->a - value));
}

void fam65xx_op_dcp_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_dcp); }
void fam65xx_op_dcp_zero_page_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_dcp); }
void fam65xx_op_dcp_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_dcp); }
void fam65xx_op_dcp_absolute_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_dcp); }
void fam65xx_op_dcp_absolute_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_dcp); }
void fam65xx_op_dcp_indirect_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_dcp); }
void fam65xx_op_dcp_indirect_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_dcp); }

// --- ISC ---
// ISC: INC memory, then SBC with A (illegal opcode)
static inline void fam65xx_op_isc(fam65xx_t* cpu, uint8_t value) {
    value++;
    fam65xx_write_cycle(cpu, cpu->address, value);
    // Use the existing binary SBC logic
    fam65xx_op_sbc(cpu, value);
}

void fam65xx_op_isc_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_isc); }
void fam65xx_op_isc_zero_page_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_isc); }
void fam65xx_op_isc_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_isc); }
void fam65xx_op_isc_absolute_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_isc); }
void fam65xx_op_isc_absolute_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_isc); }
void fam65xx_op_isc_indirect_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_isc); }
void fam65xx_op_isc_indirect_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_isc); }

// --- JAM (KIL) ---
void fam65xx_op_jam(fam65xx_t* cpu) {
    (void)fam65xx_read_cycle(cpu, cpu->pc);
    cpu->pc--; // JAM instruction halts the CPU by looping on itself
    while (1) {
        if (unlikely(FAM65XX_TEST_NMI(cpu))) {
            fam65xx_nmi(cpu);
            break;
        }
        // For now, perform a dummy read cycle to simulate the JAM behavior
        // This is a no-op in terms of CPU state, but allows the CPU to remain
        // in a valid state without crashing.
        // In a real implementation, this would halt the CPU until an interrupt occurs.
        // This is a placeholder to simulate the JAM behavior without crashing.
        // In a real CPU, this would halt execution until an interrupt occurs.
        (void)fam65xx_read_cycle(cpu, cpu->pc);
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

void fam65xx_op_lax_immediate(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_lax); }
void fam65xx_op_lax_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_lax); }
void fam65xx_op_lax_zero_page_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpy, fam65xx_op_lax); }
void fam65xx_op_lax_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_lax); }
void fam65xx_op_lax_absolute_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_lax); }
void fam65xx_op_lax_indirect_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_lax); }
void fam65xx_op_lax_indirect_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_lax); }

// --- NOP (illegal variants) ---

// Illegal NOP operation: Fetches a value but does nothing with it
static inline void fam65xx_op_nop_illegal(fam65xx_t* cpu, uint8_t value) {
    (void)cpu;    // Unused
    (void)value;  // Unused
}

void fam65xx_op_nop_immediate(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_nop_illegal); }
void fam65xx_op_nop_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_nop_illegal); }
void fam65xx_op_nop_zero_page_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_nop_illegal); }
void fam65xx_op_nop_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_nop_illegal); }
void fam65xx_op_nop_absolute_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_nop_illegal); }

// --- RLA ---
// RLA operation: ROL memory, then AND with A
static inline void fam65xx_op_rla(fam65xx_t* cpu, uint8_t value) {
    uint8_t old_carry = fam65xx_get_flag(cpu, FLAG_C);
    fam65xx_set_flag(cpu, FLAG_C, (value & 0x80) != 0);
    value = (value << 1) | (old_carry ? 1 : 0);
    fam65xx_write_cycle(cpu, cpu->address, value);
    cpu->a &= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

void fam65xx_op_rla_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_rla); }
void fam65xx_op_rla_zero_page_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_rla); }
void fam65xx_op_rla_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_rla); }
void fam65xx_op_rla_absolute_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_rla); }
void fam65xx_op_rla_absolute_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_rla); }
void fam65xx_op_rla_indirect_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_rla); }
void fam65xx_op_rla_indirect_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_rla); }

// --- RRA ---
// RRA operation: ROR memory, then ADC with A
static inline void fam65xx_op_rra(fam65xx_t* cpu, uint8_t value) {
    uint8_t old_carry = fam65xx_get_flag(cpu, FLAG_C);
    fam65xx_set_flag(cpu, FLAG_C, value & 0x01);
    value = (value >> 1) | (old_carry ? 0x80 : 0);
    fam65xx_write_cycle(cpu, cpu->address, value);
    fam65xx_op_adc(cpu, value);
}

void fam65xx_op_rra_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_rra); }
void fam65xx_op_rra_zero_page_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_rra); }
void fam65xx_op_rra_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_rra); }
void fam65xx_op_rra_absolute_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_rra); }
void fam65xx_op_rra_absolute_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_rra); }
void fam65xx_op_rra_indirect_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_rra); }
void fam65xx_op_rra_indirect_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_rra); }

// --- SAX ---
// SAX operation: Store A & X
static inline void fam65xx_op_sax(fam65xx_t* cpu, uint8_t value) {
    (void)value; // value is not used, but helper requires it
    fam65xx_write_cycle(cpu, cpu->address, cpu->a & cpu->x);
}

void fam65xx_op_sax_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_sax); }
void fam65xx_op_sax_zero_page_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpy, fam65xx_op_sax); }
void fam65xx_op_sax_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_sax); }
void fam65xx_op_sax_indirect_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_sax); }

// --- SLO ---
// SLO operation: ASL memory, then ORA with A
static inline void fam65xx_op_slo(fam65xx_t* cpu, uint8_t value) {
    fam65xx_set_flag(cpu, FLAG_C, (value & 0x80) != 0);
    value <<= 1;
    fam65xx_write_cycle(cpu, cpu->address, value);
    cpu->a |= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

void fam65xx_op_slo_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_slo); }
void fam65xx_op_slo_zero_page_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_slo); }
void fam65xx_op_slo_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_slo); }
void fam65xx_op_slo_absolute_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_slo); }
void fam65xx_op_slo_absolute_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_slo); }
void fam65xx_op_slo_indirect_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_slo); }
void fam65xx_op_slo_indirect_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_slo); }

// --- SRE ---
// SRE operation: LSR memory, then EOR with A
static inline void fam65xx_op_sre(fam65xx_t* cpu, uint8_t value) {
    fam65xx_set_flag(cpu, FLAG_C, value & 0x01);
    value >>= 1;
    fam65xx_write_cycle(cpu, cpu->address, value);
    cpu->a ^= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

void fam65xx_op_sre_zero_page(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zp, fam65xx_op_sre); }
void fam65xx_op_sre_zero_page_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_zpx, fam65xx_op_sre); }
void fam65xx_op_sre_absolute(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_abs, fam65xx_op_sre); }
void fam65xx_op_sre_absolute_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absx, fam65xx_op_sre); }
void fam65xx_op_sre_absolute_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_absy, fam65xx_op_sre); }
void fam65xx_op_sre_indirect_x(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indx, fam65xx_op_sre); }
void fam65xx_op_sre_indirect_y(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_indy, fam65xx_op_sre); }

// --- XAA (immediate) ---
// XAA operation: Transfer X to A, then AND with immediate
static inline void fam65xx_op_xaa(fam65xx_t* cpu, uint8_t value) {
    cpu->a = cpu->x;
    cpu->a &= value;
    fam65xx_set_nz_flags(cpu, cpu->a);
}

void fam65xx_op_xaa_immediate(fam65xx_t* cpu) { fam65xx_addr_op_helper(cpu, fam65xx_addr_imm, fam65xx_op_xaa); }