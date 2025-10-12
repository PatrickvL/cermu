#pragma once
/*
 * fam65xx_operations.h - MOS 65xx Family CPU Official Operation Handlers
 * 
 * This file contains all official (documented) CPU operation handlers.
 * These functions perform the actual CPU operations after addressing
 * modes have prepared the target addresses.
 * 
 * Operations included:
 * - Load/Store operations (LDA, LDX, LDY, STA, STX, STY)
 * - Arithmetic operations (ADC, SBC)
 * - Logic operations (AND, ORA, EOR)
 * - Compare operations (CMP, CPX, CPY)
 * - Register operations (INX, INY, DEX, DEY)
 * - Transfer operations (TAX, TAY, TXA, TYA, TSX, TXS)
 * - Stack operations (PHA, PHP, PLA, PLP)
 * - Branch operations (BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS)
 * - Flag operations (CLC, SEC, CLI, SEI, CLD, SED, CLV)
 * - Control flow (JMP, JSR, RTS, RTI, BRK)
 * - Bit test and misc (BIT, NOP, JAM)
 * - 65C02 enhancements (BRA)
 * 
 * Functions are alphabetically ordered within groups for maintainability.
 * RMW operations are in a separate file (fam65xx_rmw.h).
 * Illegal operations are in a separate file (fam65xx_illegal.h).
 */

#include "fam65xx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

// Forward declarations for internal functions
static bus_state_t fam65xx_phi2_read(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg);
static bus_state_t fam65xx_phi2_write(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg, reg8_t reg_write);
static void fam65xx_transition_to_fetch(fam65xx_t* cpu);
static bus_state_t fam65xx_branch_helper(fam65xx_t* cpu, bus_state_t pins, uint8_t flag_mask, bool flag_value);

/* ============================================================================
 * ARITHMETIC OPERATIONS
 * ============================================================================
 */

/* ADC - Add with Carry */
static bus_state_t op_adc(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform ADC operation */
    uint8_t operand = BUS_GET_DATA(pins);
    uint16_t result = CPU_A(cpu) + operand + (CPU_P(cpu) & FLAG_C ? 1 : 0);
    
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    
    if (result > 0xFF) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    
    if (((a_old ^ result) & (operand ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ADC immediate mode uses operand from PC */
static bus_state_t op_adc_imm(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from PC and increment */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform ADC operation */
    uint8_t operand = BUS_GET_DATA(pins);
    CPU_PC(cpu)++;
    uint16_t result = CPU_A(cpu) + operand + (CPU_P(cpu) & FLAG_C ? 1 : 0);
    
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    
    if (result > 0xFF) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    
    if (((a_old ^ result) & (operand ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SBC - Subtract with Carry */
static bus_state_t op_sbc(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform SBC operation */
    uint8_t operand = BUS_GET_DATA(pins);
    uint16_t result = CPU_A(cpu) - operand - (CPU_P(cpu) & FLAG_C ? 0 : 1);
    
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    
    if (result < 0x100) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    
    if (((a_old ^ operand) & (a_old ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SBC immediate mode */
static bus_state_t op_sbc_imm(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from PC and increment */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform SBC operation */
    uint8_t operand = BUS_GET_DATA(pins);
    CPU_PC(cpu)++;
    uint16_t result = CPU_A(cpu) - operand - (CPU_P(cpu) & FLAG_C ? 0 : 1);
    
    uint8_t a_old = CPU_A(cpu);
    CPU_A(cpu) = result & 0xFF;
    
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    
    if (result < 0x100) CPU_P(cpu) |= FLAG_C;
    else CPU_P(cpu) &= ~FLAG_C;
    
    if (((a_old ^ operand) & (a_old ^ result) & 0x80))
        CPU_P(cpu) |= FLAG_V;
    else
        CPU_P(cpu) &= ~FLAG_V;
    
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ============================================================================
 * BIT TEST AND MISC OPERATIONS
 * ============================================================================
 */

/* BIT - Bit Test */
static bus_state_t op_bit(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform BIT test */
    uint8_t data = BUS_GET_DATA(pins);
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_V | FLAG_Z)) |
                 (data & FLAG_N) |
                 (data & FLAG_V) |
                 ((CPU_A(cpu) & data) == 0 ? FLAG_Z : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* JAM - Jam/Halt CPU */
static bus_state_t op_jam(fam65xx_t* cpu, bus_state_t pins) {
    /* JAM instruction - CPU halts indefinitely */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    /* Don't transition to fetch - stay in JAM state */
    return pins;
}

/* NOP - No Operation */
static bus_state_t op_nop(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read next byte from PC (dummy read for NOP) */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: No need to increment PC - already done in opcode fetch */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ============================================================================
 * BRANCH OPERATIONS
 * ============================================================================
 */

/* Helper function for branch operations */
static bus_state_t fam65xx_branch_helper(fam65xx_t* cpu, bus_state_t pins, uint8_t flag_mask, bool flag_value) {
    switch (cpu->cycle_index++) {
        case 0: {
            /* PHI2: Read branch offset from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Check branch condition */
            bool branch_taken = ((CPU_P(cpu) & flag_mask) != 0) == flag_value;
            CPU_PC(cpu)++;
            
            if (!branch_taken) {
                fam65xx_transition_to_fetch(cpu);
                return pins;
            }
            
            /* Calculate target address */
            int8_t offset = (int8_t)BUS_GET_DATA(pins);
            uint16_t target = CPU_PC(cpu) + offset;
            CPU_AB(cpu) = target;
            
            /* Skip cycle 2 if no page cross and page_cross flag set */
            if (cpu->opcode_entry.page_cross && !fam65xx_page_crossed(CPU_PC(cpu), target)) {
                CPU_PC(cpu) = target;
                fam65xx_transition_to_fetch(cpu);
                return pins;
            }
            break;
        }
        
        case 1:
            /* PHI2: Page cross penalty cycle */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Set final PC */
            CPU_PC(cpu) = CPU_AB(cpu);
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* BRA - Branch Always (65C02) */
static bus_state_t op_bra(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, 0xFF, true); /* Always branch */
}

/* Branch operations - alphabetically ordered */
static bus_state_t op_bcc(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_C, false);
}

static bus_state_t op_bcs(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_C, true);
}

static bus_state_t op_beq(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_Z, true);
}

static bus_state_t op_bmi(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_N, true);
}

static bus_state_t op_bne(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_Z, false);
}

static bus_state_t op_bpl(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_N, false);
}

static bus_state_t op_bvc(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_V, false);
}

static bus_state_t op_bvs(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_V, true);
}

/* ============================================================================
 * COMPARE OPERATIONS  
 * ============================================================================
 */

/* CMP - Compare Accumulator */
static bus_state_t op_cmp(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform CMP operation */
    uint8_t data = BUS_GET_DATA(pins);
    uint16_t result = CPU_A(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_A(cpu) >= data ? FLAG_C : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* CMP immediate mode */
static bus_state_t op_cmp_imm(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from PC and increment */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform CMP operation */
    uint8_t data = BUS_GET_DATA(pins);
    CPU_PC(cpu)++;
    uint16_t result = CPU_A(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_A(cpu) >= data ? FLAG_C : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* CPX - Compare X Register */
static bus_state_t op_cpx(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform CPX operation */
    uint8_t data = BUS_GET_DATA(pins);
    uint16_t result = CPU_X(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_X(cpu) >= data ? FLAG_C : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* CPX immediate mode */
static bus_state_t op_cpx_imm(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from PC and increment */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform CPX operation */
    uint8_t data = BUS_GET_DATA(pins);
    CPU_PC(cpu)++;
    uint16_t result = CPU_X(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_X(cpu) >= data ? FLAG_C : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* CPY - Compare Y Register */
static bus_state_t op_cpy(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform CPY operation */
    uint8_t data = BUS_GET_DATA(pins);
    uint16_t result = CPU_Y(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_Y(cpu) >= data ? FLAG_C : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* CPY immediate mode */
static bus_state_t op_cpy_imm(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from PC and increment */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform CPY operation */
    uint8_t data = BUS_GET_DATA(pins);
    CPU_PC(cpu)++;
    uint16_t result = CPU_Y(cpu) - data;
    
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 (CPU_Y(cpu) >= data ? FLAG_C : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif