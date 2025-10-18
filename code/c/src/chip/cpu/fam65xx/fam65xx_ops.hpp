#pragma once
/*
 * fam65xx_operations.hpp - MOS 65xx Family CPU Official Operation Handlers (C++ Version)
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

#include "fam65xx_types.hpp"
#include "fam65xx_utils.hpp"
#include "fam65xx_helpers.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef AIEMUC_IMPL



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
        
        /* Calculate binary result first for flag computation */
        uint16_t binary_result = a_old + operand + carry_in;
        
        /* 6502 BCD addition algorithm - hardware accurate */
        uint8_t al = (a_old & 0x0F) + (operand & 0x0F) + carry_in;
        uint8_t ah = (a_old >> 4) + (operand >> 4);
        
        /* Adjust low nibble if > 9 */
        if (al > 9) {
            al += 6;
            ah++;
        }
        
        /* Set flags based on binary result BEFORE BCD adjustment */
        CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                     (binary_result & 0x80 ? FLAG_N : 0) |                       /* N = bit 7 of binary result */
                     ((binary_result & 0xFF) == 0 ? FLAG_Z : 0) |                /* Z = binary result is zero */
                     (binary_result > 0xFF ? FLAG_C : 0) |                       /* C = carry out from binary result */
                     (((a_old ^ binary_result) & (operand ^ binary_result) & 0x80) ? FLAG_V : 0); /* V = signed overflow on binary result */
        
        /* Adjust high nibble if > 9 and handle carry out */
        if (ah > 9) {
            ah += 6;
            /* BCD carry is handled differently - set carry if ah >= 16 after adjustment */
            if (ah >= 16) {
                CPU_P(cpu) |= FLAG_C;
            }
        }
        
        /* Assemble BCD result - properly handle high nibble overflow */
        CPU_A(cpu) = ((ah & 0x0F) << 4) | (al & 0x0F);
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
    
    /* PHI1: Perform SBC operation with BCD support */
    uint8_t operand = BUS_GET_DATA(pins);
    uint8_t borrow_in = (CPU_P(cpu) & FLAG_C) ? 0 : 1;
    uint8_t a_old = CPU_A(cpu);
    
    if (CPU_P(cpu) & FLAG_D) {
        /* BCD (Decimal) mode - 6502 hardware behavior */
        uint8_t al = (a_old & 0x0F) - (operand & 0x0F) - borrow_in;
        uint8_t ah = (a_old >> 4) - (operand >> 4);
        
        if (al & 0x10) {
            al = (al - 6) & 0x0F;
            ah--;
        }
        
        /* Set flags before final BCD correction (based on binary operation) */
        uint16_t binary_result = a_old - operand - borrow_in;
        CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                     (binary_result & 0x80 ? FLAG_N : 0) |                    /* N based on binary result */
                     ((binary_result & 0xFF) == 0 ? FLAG_Z : 0) |             /* Z based on binary result */
                     (((a_old ^ operand) & (a_old ^ binary_result) & 0x80) ? FLAG_V : 0); /* V based on binary */
        
        if (ah & 0x10) {
            ah = (ah - 6) & 0x0F;
        }
        
        /* Set carry flag based on borrow */
        if (binary_result < 0x100) {
            CPU_P(cpu) |= FLAG_C;
        }
        
        CPU_A(cpu) = (ah << 4) | al;
    } else {
        /* Binary mode */
        uint16_t result = a_old - operand - borrow_in;
        CPU_A(cpu) = result & 0xFF;
        
        /* SBC modifies only N, V, Z, C flags - preserve all others exactly */
        CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C)) |
                     (CPU_A(cpu) & FLAG_N) |                                    /* N = bit 7 of result */
                     (CPU_A(cpu) == 0 ? FLAG_Z : 0) |                          /* Z = result is zero */
                     (result < 0x100 ? FLAG_C : 0) |                           /* C = no borrow */
                     (((a_old ^ operand) & (a_old ^ result) & 0x80) ? FLAG_V : 0); /* V = overflow */
    }
    
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
    /* NOP behavior depends on addressing mode:
     * - AM_NON (implicit): Legal NOP 0xea - needs dummy internal cycle (2 cycles total)
     * - AM_IMM: Illegal NOPs use this for 2-cycle dummy read without PC increment
     * - Memory modes: Read from target address and discard (hardware accurate) */
    
    if (cpu->opcode_entry.am_index == AM_IMM) {
        /* Check opcode to distinguish between legal immediate NOPs vs illegal variants */
        uint8_t opcode = CPU_IR(cpu);
        
        /* Illegal NOP opcodes that need dummy read without PC increment */
        if (opcode == 0x1a || opcode == 0x3a || opcode == 0x5a ||
            opcode == 0x7a || opcode == 0xda || opcode == 0xfa) {
            /* Illegal NOP: dummy read from PC WITHOUT increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
        } else {
            /* Legal immediate NOP: read operand and increment PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
        }
    } else if (cpu->opcode_entry.am_index > AM_IMM) {
        /* Memory addressing modes - read from target address and discard */
        pins = fam65xx_phi2_read(cpu, pins, REG_AB);
        if (!FAM65XX_GET_RDY(pins)) return pins;
    } else {
        /* Legal NOP (0xea) - AM_NON: Dummy read from PC for internal operation cycle */
        pins = fam65xx_phi2_read(cpu, pins, REG_PC);
        if (!FAM65XX_GET_RDY(pins)) return pins;
        /* Note: PC is NOT incremented for legal NOP - it's an internal operation */
    }
    
    /* PHI1: No operation performed - instruction completes */
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
            CPU_PCL(cpu) += (int8_t)CPU_DL(cpu);
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

/* BCC - Branch if Carry Clear */
static bus_state_t op_bcc(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_C, false);
}

/* BCS - Branch if Carry Set */
static bus_state_t op_bcs(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_C, true);
}

/* BEQ - Branch if Equal (Zero Set) */
static bus_state_t op_beq(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_Z, true);
}

/* BNE - Branch if Not Equal (Zero Clear) */
static bus_state_t op_bne(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_Z, false);
}

/* BMI - Branch if Minus (Negative Set) */
static bus_state_t op_bmi(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_N, true);
}

/* BPL - Branch if Plus (Negative Clear) */
static bus_state_t op_bpl(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_N, false);
}

/* BVC - Branch if Overflow Clear */
static bus_state_t op_bvc(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_V, false);
}

/* BVS - Branch if Overflow Set */
static bus_state_t op_bvs(fam65xx_t* cpu, bus_state_t pins) {
    return fam65xx_branch_helper(cpu, pins, FLAG_V, true);
}

/* ============================================================================
 * COMPARE OPERATIONS  
 * ============================================================================
 */

/* CMP - Compare Accumulator */
static bus_state_t op_cmp(fam65xx_t* cpu, bus_state_t pins) {
    return compare_helper(cpu, pins, CPU_A(cpu));
}

/* CPX - Compare X Register */
static bus_state_t op_cpx(fam65xx_t* cpu, bus_state_t pins) {
    return compare_helper(cpu, pins, CPU_X(cpu));
}

/* CPY - Compare Y Register */
static bus_state_t op_cpy(fam65xx_t* cpu, bus_state_t pins) {
    return compare_helper(cpu, pins, CPU_Y(cpu));
}


#endif /* AIEMUC_IMPL */

#ifdef __cplusplus
}
#endif