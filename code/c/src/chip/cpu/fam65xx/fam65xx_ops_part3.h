#pragma once
/*
 * fam65xx_operations_part3.h - MOS 65xx Family CPU Operations (Part 3)
 * 
 * This file contains the final set of official CPU operations:
 * - Load Operations (LDA, LDX, LDY)
 * - Logic Operations (AND, EOR, ORA)
 * - Register Operations (DEX, DEY, INX, INY)
 * - Stack Operations (PHA, PHP, PLA, PLP)
 * - Store Operations (STA, STX, STY)
 * - Transfer Operations (TAX, TAY, TSX, TXA, TXS, TYA)
 */

#include "fam65xx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

/* ============================================================================
 * LOAD OPERATIONS
 * ============================================================================
 */

/* LDA - Load Accumulator */
static bus_state_t op_lda(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read from target address or PC for immediate */
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
    
    /* PHI1: Load accumulator and set flags */
    CPU_A(cpu) = BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}


/* LDX - Load X Register */
static bus_state_t op_ldx(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read from target address or PC for immediate */
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
    
    /* PHI1: Load X register and set flags */
    CPU_X(cpu) = BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_X(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}


/* LDY - Load Y Register */
static bus_state_t op_ldy(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read from target address or PC for immediate */
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
    
    /* PHI1: Load Y register and set flags */
    CPU_Y(cpu) = BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_Y(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}


/* ============================================================================
 * LOGIC OPERATIONS
 * ============================================================================
 */

/* AND - Logical AND */
static bus_state_t op_and(fam65xx_t* cpu, bus_state_t pins) {
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
    
    /* PHI1: Perform AND operation */
    CPU_A(cpu) &= BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}


/* EOR - Exclusive OR */
static bus_state_t op_eor(fam65xx_t* cpu, bus_state_t pins) {
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
    
    /* PHI1: Perform EOR operation */
    CPU_A(cpu) ^= BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}


/* ORA - Logical OR */
static bus_state_t op_ora(fam65xx_t* cpu, bus_state_t pins) {
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
    
    /* PHI1: Perform ORA operation */
    CPU_A(cpu) |= BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}


/* ============================================================================
 * REGISTER OPERATIONS
 * ============================================================================
 */

/* DEX - Decrement X Register */
static bus_state_t op_dex(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Decrement X register */
    CPU_X(cpu)--;
    fam65xx_update_nz_flags(cpu, CPU_X(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* DEY - Decrement Y Register */
static bus_state_t op_dey(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Decrement Y register */
    CPU_Y(cpu)--;
    fam65xx_update_nz_flags(cpu, CPU_Y(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* INX - Increment X Register */
static bus_state_t op_inx(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Increment X register */
    CPU_X(cpu)++;
    fam65xx_update_nz_flags(cpu, CPU_X(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* INY - Increment Y Register */
static bus_state_t op_iny(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Increment Y register */
    CPU_Y(cpu)++;
    fam65xx_update_nz_flags(cpu, CPU_Y(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ============================================================================
 * STACK OPERATIONS
 * ============================================================================
 */

/* PHA - Push Accumulator */
static bus_state_t op_pha(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            break;
            
        case 1:
            /* PHI2: Write A to stack and decrement SP */
            pins = fam65xx_phi2_write(cpu, pins, REG_SP, REG_A);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* PHP - Push Processor Status */
static bus_state_t op_php(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* Dummy cycle for internal operation */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            break;
            
        case 1:
            /* PHI2: Write P|B|U to stack and decrement SP */
            CPU_DL(cpu) = CPU_P(cpu) | FLAG_B | FLAG_U;
            pins = fam65xx_phi2_write(cpu, pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* PLA - Pull Accumulator */
static bus_state_t op_pla(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Dummy read from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            break;
            
        case 1:
            /* PHI2: Dummy read from current stack pointer, then increment SP */
            pins = fam65xx_phi2_read(cpu, pins, REG_SP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Increment stack pointer */
            CPU_S(cpu)++;
            break;
            
        case 2:
            /* PHI2: Read from incremented stack pointer */
            pins = fam65xx_phi2_read(cpu, pins, REG_SP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store in A and set flags */
            CPU_A(cpu) = BUS_GET_DATA(pins);
            fam65xx_update_nz_flags(cpu, CPU_A(cpu));
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* PLP - Pull Processor Status */
static bus_state_t op_plp(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Dummy read from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            break;
            
        case 1:
            /* PHI2: Dummy read from current stack pointer, then increment SP */
            pins = fam65xx_phi2_read(cpu, pins, REG_SP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Increment stack pointer */
            CPU_S(cpu)++;
            break;
            
        case 2:
            /* PHI2: Read from incremented stack pointer */
            pins = fam65xx_phi2_read(cpu, pins, REG_SP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store in P (clear B, set U) */
            CPU_P(cpu) = (BUS_GET_DATA(pins) & ~FLAG_B) | FLAG_U;
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* ============================================================================
 * STORE OPERATIONS
 * ============================================================================
 */

/* STA - Store Accumulator */
static bus_state_t op_sta(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Write A to target address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_A);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* STX - Store X Register */
static bus_state_t op_stx(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Write X to target address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_X);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* STY - Store Y Register */
static bus_state_t op_sty(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Write Y to target address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_Y);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ============================================================================
 * TRANSFER OPERATIONS
 * ============================================================================
 */

/* TAX - Transfer A to X */
static bus_state_t op_tax(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Transfer A to X */
    CPU_X(cpu) = CPU_A(cpu);
    fam65xx_update_nz_flags(cpu, CPU_X(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* TAY - Transfer A to Y */
static bus_state_t op_tay(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Transfer A to Y */
    CPU_Y(cpu) = CPU_A(cpu);
    fam65xx_update_nz_flags(cpu, CPU_Y(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* TSX - Transfer S to X */
static bus_state_t op_tsx(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Transfer S to X */
    CPU_X(cpu) = CPU_S(cpu);
    fam65xx_update_nz_flags(cpu, CPU_X(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* TXA - Transfer X to A */
static bus_state_t op_txa(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Transfer X to A */
    CPU_A(cpu) = CPU_X(cpu);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* TXS - Transfer X to S */
static bus_state_t op_txs(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Transfer X to S */
    CPU_S(cpu) = CPU_X(cpu);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* TYA - Transfer Y to A */
static bus_state_t op_tya(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Transfer Y to A */
    CPU_A(cpu) = CPU_Y(cpu);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif