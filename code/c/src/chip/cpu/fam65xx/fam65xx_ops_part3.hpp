#pragma once
/*
 * fam65xx_operations_part3.hpp - MOS 65xx Family CPU Operations (Part 3) (C++ Version)
 *
 * This file contains the final set of official CPU operations:
 * - Load Operations (LDA, LDX, LDY)
 * - Logic Operations (AND, EOR, ORA)
 * - Register Operations (DEX, DEY, INX, INY)
 * - Stack Operations (PHA, PHP, PLA, PLP)
 * - Store Operations (STA, STX, STY)
 * - Transfer Operations (TAX, TAY, TSX, TXA, TYA)
 */

#include "fam65xx_types.hpp"
#include "fam65xx_utils.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef AIEMUC_IMPL

/* ============================================================================
 * HELPER FUNCTIONS FOR CODE DEDUPLICATION
 * ============================================================================
 */

/* Common pattern: Read operand from immediate or memory mode */
static inline bus_state_t read_operand_immediate_or_memory(fam65xx_t* cpu, bus_state_t pins) {
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
    return pins;
}

/* Common pattern: Simple register operation with dummy cycle */
static inline bus_state_t simple_register_op_with_dummy_cycle(fam65xx_t* cpu, bus_state_t pins) {
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    return pins;
}

/* ============================================================================
 * LOAD OPERATIONS
 * ============================================================================
 */

/* LDA - Load Accumulator */
static bus_state_t op_lda(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand using common pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Load accumulator and set flags */
    CPU_A(cpu) = BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* LDX - Load X Register */
static bus_state_t op_ldx(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand using common pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Load X register and set flags */
    CPU_X(cpu) = BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_X(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* LDY - Load Y Register */
static bus_state_t op_ldy(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand using common pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
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
    /* PHI2: Read operand using common pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform AND operation */
    CPU_A(cpu) &= BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* EOR - Exclusive OR */
static bus_state_t op_eor(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand using common pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform EOR operation */
    CPU_A(cpu) ^= BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ORA - Logical OR */
static bus_state_t op_ora(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand using common pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
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
    /* Dummy cycle using common pattern */
    pins = simple_register_op_with_dummy_cycle(cpu, pins);
    
    /* PHI1: Decrement X register */
    CPU_X(cpu)--;
    fam65xx_update_nz_flags(cpu, CPU_X(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* DEY - Decrement Y Register */
static bus_state_t op_dey(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle using common pattern */
    pins = simple_register_op_with_dummy_cycle(cpu, pins);
    
    /* PHI1: Decrement Y register */
    CPU_Y(cpu)--;
    fam65xx_update_nz_flags(cpu, CPU_Y(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* INX - Increment X Register */
static bus_state_t op_inx(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle using common pattern */
    pins = simple_register_op_with_dummy_cycle(cpu, pins);
    
    /* PHI1: Increment X register */
    CPU_X(cpu)++;
    fam65xx_update_nz_flags(cpu, CPU_X(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* INY - Increment Y Register */
static bus_state_t op_iny(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle using common pattern */
    pins = simple_register_op_with_dummy_cycle(cpu, pins);
    
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

/* Helper for store operations */
static inline bus_state_t store_register_helper(fam65xx_t* cpu, bus_state_t pins, reg8_t reg_index) {
    /* PHI2: Write register to target address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, reg_index);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* STA - Store Accumulator */
static bus_state_t op_sta(fam65xx_t* cpu, bus_state_t pins) {
    return store_register_helper(cpu, pins, REG_A);
}

/* STX - Store X Register */
static bus_state_t op_stx(fam65xx_t* cpu, bus_state_t pins) {
    return store_register_helper(cpu, pins, REG_X);
}

/* STY - Store Y Register */
static bus_state_t op_sty(fam65xx_t* cpu, bus_state_t pins) {
    return store_register_helper(cpu, pins, REG_Y);
}

/* ============================================================================
 * TRANSFER OPERATIONS
 * ============================================================================
 */

/* Helper for transfer operations with flags */
static inline bus_state_t transfer_with_flags_helper(fam65xx_t* cpu, bus_state_t pins, uint8_t value, uint8_t* target_reg) {
    /* Dummy cycle using common pattern */
    pins = simple_register_op_with_dummy_cycle(cpu, pins);
    
    /* PHI1: Transfer and update flags */
    *target_reg = value;
    fam65xx_update_nz_flags(cpu, *target_reg);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* Helper for transfer operations without flags */
static inline bus_state_t transfer_no_flags_helper(fam65xx_t* cpu, bus_state_t pins, uint8_t value, uint8_t* target_reg) {
    /* Dummy cycle using common pattern */
    pins = simple_register_op_with_dummy_cycle(cpu, pins);
    
    /* PHI1: Transfer without updating flags */
    *target_reg = value;
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* TAX - Transfer A to X */
static bus_state_t op_tax(fam65xx_t* cpu, bus_state_t pins) {
    return transfer_with_flags_helper(cpu, pins, CPU_A(cpu), &CPU_X(cpu));
}

/* TAY - Transfer A to Y */
static bus_state_t op_tay(fam65xx_t* cpu, bus_state_t pins) {
    return transfer_with_flags_helper(cpu, pins, CPU_A(cpu), &CPU_Y(cpu));
}

/* TSX - Transfer S to X */
static bus_state_t op_tsx(fam65xx_t* cpu, bus_state_t pins) {
    return transfer_with_flags_helper(cpu, pins, CPU_S(cpu), &CPU_X(cpu));
}

/* TXA - Transfer X to A */
static bus_state_t op_txa(fam65xx_t* cpu, bus_state_t pins) {
    return transfer_with_flags_helper(cpu, pins, CPU_X(cpu), &CPU_A(cpu));
}

/* TXS - Transfer X to S */
static bus_state_t op_txs(fam65xx_t* cpu, bus_state_t pins) {
    return transfer_no_flags_helper(cpu, pins, CPU_X(cpu), &CPU_S(cpu));
}

/* TYA - Transfer Y to A */
static bus_state_t op_tya(fam65xx_t* cpu, bus_state_t pins) {
    return transfer_with_flags_helper(cpu, pins, CPU_Y(cpu), &CPU_A(cpu));
}

#endif /* AIEMUC_IMPL */

#ifdef __cplusplus
}
#endif