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
    /* PHI2: Read operand from target address or PC for immediate */
    if (cpu->opcode_entry.am_index == AM_IMM) {
        /* Immediate mode - read from PC */
        pins = fam65xx_phi2_read(cpu, pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        CPU_PC(cpu)++;
    } else {
        /* Memory mode - read from target address */
        pins = fam65xx_phi2_read(cpu, pins, REG_AB);
        if (!FAM65XX_GET_RDY(pins)) return pins;
    }
    
    /* PHI1: Perform ADC operation with BCD support */
    uint8_t operand = BUS_GET_DATA(pins);
    uint8_t carry_in = (CPU_P(cpu) & FLAG_C) ? 1 : 0;
    uint8_t a_old = CPU_A(cpu);
    
    if (CPU_P(cpu) & FLAG_D) {
        /* BCD (Decimal) mode - 6502 hardware behavior */
        uint8_t al = (a_old & 0x0F) + (operand & 0x0F) + carry_in;
        uint8_t ah = (a_old >> 4) + (operand >> 4);
        
        if (al > 9) {
            al = (al + 6) & 0x0F;
            ah++;
        }
        
        /* Set flags before final BCD correction */
        uint16_t binary_result = a_old + operand + carry_in;
        CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                     (binary_result & 0x80 ? FLAG_N : 0) |                    /* N based on binary result */
                     (binary_result == 0 ? FLAG_Z : 0) |                      /* Z based on binary result */
                     (((a_old ^ binary_result) & (operand ^ binary_result) & 0x80) ? FLAG_V : 0); /* V based on binary */
        
        if (ah > 9) {
            ah = (ah + 6) & 0x0F;
            CPU_P(cpu) |= FLAG_C;
        }
        
        CPU_A(cpu) = (ah << 4) | al;
    } else {
        /* Binary mode */
        uint16_t result = a_old + operand + carry_in;
        CPU_A(cpu) = result & 0xFF;
        
        /* ADC modifies only N, V, Z, C flags - preserve all others exactly */
        CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                     (CPU_A(cpu) & FLAG_N) |                                    /* N = bit 7 of result */
                     (CPU_A(cpu) == 0 ? FLAG_Z : 0) |                          /* Z = result is zero */
                     (result > 0xFF ? FLAG_C : 0) |                            /* C = carry out */
                     (((a_old ^ result) & (operand ^ result) & 0x80) ? FLAG_V : 0); /* V = overflow */
    }
    
    fam65xx_transition_to_fetch(cpu);
    return pins;
}


/* SBC - Subtract with Carry */
static bus_state_t op_sbc(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address or PC for immediate */
    if (cpu->opcode_entry.am_index == AM_IMM) {
        /* Immediate mode - read from PC */
        pins = fam65xx_phi2_read(cpu, pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        CPU_PC(cpu)++;
    } else {
        /* Memory mode - read from target address */
        pins = fam65xx_phi2_read(cpu, pins, REG_AB);
        if (!FAM65XX_GET_RDY(pins)) return pins;
    }
    
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
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read from PC+1 (next byte after opcode) */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            /* Don't increment PC - we'll read from same location again */
            break;
            
        case 1:
            /* PHI2: Read from same PC+1 location again (JAM behavior) */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* Restore PC to original opcode location (undo the increment from opcode fetch) */
            CPU_PC(cpu)--;
            
            /* For testing: transition to fetch (in real hardware this would loop forever) */
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* NOP - No Operation */
static bus_state_t op_nop(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read from PC */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;

    if (cpu->opcode_entry.am_index == AM_IMM) {
        /* Immediate mode - increment PC to skip operand */
        CPU_PC(cpu)++;
    }
    
    /* PHI1: No operation performed */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ============================================================================
 * BRANCH OPERATIONS
 * ============================================================================
 */

/* Helper function for branch operations - hardware-accurate 6502 timing */
static bus_state_t fam65xx_branch_helper(fam65xx_t* cpu, bus_state_t pins, uint8_t flag_mask, bool flag_value) {
    switch (cpu->cycle_index++) {
        case 0: {
            /* PHI2: Read branch offset from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Check branch condition */
            bool branch_taken = ((CPU_P(cpu) & flag_mask) != 0) == flag_value;
            
            if (!branch_taken) {
                /* Branch not taken: instruction completes after 2 cycles */
                fam65xx_transition_to_fetch(cpu);
                return pins;
            }
            
            /* Branch taken: store offset and calculate target */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_AB(cpu) = CPU_PC(cpu) + (int8_t)CPU_DL(cpu);
            break;
        }
        
        case 1: {
            /* PHI2: Dummy read from incremented PC (hardware behavior) */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Check for page cross */
            bool page_crossed = fam65xx_page_crossed(CPU_PC(cpu), CPU_AB(cpu));
            
            if (!page_crossed) {
                /* No page cross: set final PC and complete after 3 cycles */
                CPU_PC(cpu) = CPU_AB(cpu);
                fam65xx_transition_to_fetch(cpu);
                return pins;
            }
            
            /* Page cross detected: Set up wrong intermediate address for penalty cycle */
            /* Hardware adds offset to low byte only, keeping original high byte */
            int8_t signed_offset = (int8_t)CPU_DL(cpu);
            CPU_PCL(cpu) += signed_offset;
            break;
        }
        
        case 2:
            /* PHI2: Page cross penalty - dummy read from wrong intermediate address */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Set final correct target PC and complete instruction */
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
    /* PHI2: Read operand from target address or PC for immediate */
    if (cpu->opcode_entry.am_index == AM_IMM) {
        /* Immediate mode - read from PC */
        pins = fam65xx_phi2_read(cpu, pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        CPU_PC(cpu)++;
    } else {
        /* Memory mode - read from target address */
        pins = fam65xx_phi2_read(cpu, pins, REG_AB);
        if (!FAM65XX_GET_RDY(pins)) return pins;
    }
    
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


/* CPX - Compare X Register */
static bus_state_t op_cpx(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address or PC for immediate */
    if (cpu->opcode_entry.am_index == AM_IMM) {
        /* Immediate mode - read from PC */
        pins = fam65xx_phi2_read(cpu, pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        CPU_PC(cpu)++;
    } else {
        /* Memory mode - read from target address */
        pins = fam65xx_phi2_read(cpu, pins, REG_AB);
        if (!FAM65XX_GET_RDY(pins)) return pins;
    }
    
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


/* CPY - Compare Y Register */
static bus_state_t op_cpy(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address or PC for immediate */
    if (cpu->opcode_entry.am_index == AM_IMM) {
        /* Immediate mode - read from PC */
        pins = fam65xx_phi2_read(cpu, pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        CPU_PC(cpu)++;
    } else {
        /* Memory mode - read from target address */
        pins = fam65xx_phi2_read(cpu, pins, REG_AB);
        if (!FAM65XX_GET_RDY(pins)) return pins;
    }
    
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


#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif