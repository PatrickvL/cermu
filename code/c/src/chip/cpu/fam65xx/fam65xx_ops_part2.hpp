#pragma once
/*
 * fam65xx_operations_part2.hpp - MOS 65xx Family CPU Operations (Part 2) (C++ Version)
 * 
 * This file contains the remaining official CPU operations:
 * - Control Flow Operations (BRK, JMP, JSR, RTI, RTS)
 * - Flag Operations (CLC, CLD, CLI, CLV, SEC, SED, SEI)
 * - Load Operations (LDA, LDX, LDY)
 * - Logic Operations (AND, EOR, ORA)
 * - Register Operations (DEX, DEY, INX, INY)
 * - Stack Operations (PHA, PHP, PLA, PLP)
 * - Store Operations (STA, STX, STY)
 * - Transfer Operations (TAX, TAY, TSX, TXA, TXS, TYA)
 */

#include "fam65xx_core.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

/* Helper function to get interrupt vector address based on BRK flags */
static inline uint16_t fam65xx_get_vector_addr(fam65xx_t* cpu) {
    if (cpu->brk_flags & FAM65XX_BRK_RESET) {
        return 0xFFFC;
    } else if (cpu->brk_flags & FAM65XX_BRK_NMI) {
        return 0xFFFA;
    } else {
        return 0xFFFE;  // BRK/IRQ vector
    }
}

/* ============================================================================
 * HELPER FUNCTIONS FOR CODE DEDUPLICATION
 * ============================================================================
 */

/* Common pattern: Dummy cycle for internal flag operations */
static inline bus_state_t part2_flag_operation_with_dummy_cycle(fam65xx_t* cpu, bus_state_t pins, uint8_t flag, bool set_flag) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Set or clear the flag */
    if (set_flag) {
        CPU_P(cpu) |= flag;
    } else {
        CPU_P(cpu) &= ~flag;
    }
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ============================================================================
 * CONTROL FLOW OPERATIONS
 * ============================================================================
 */

/* BRK - Break / Software Interrupt */
static bus_state_t op_brk(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* BRK does a dummy read from PC+1 */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;

            if (!(cpu->brk_flags & FAM65XX_BRK_RESET)) {
                /* Increment PC for BRK/IRQ/NMI (but not RESET) */
                CPU_PC(cpu)++;
            }
            break;
            
        case 1:
            /* PHI2: Push PCH to stack */
            pins = fam65xx_phi2_write(cpu, pins, REG_SP, REG_PCH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            break;
            
        case 2:
            /* PHI2: Push PCL to stack */
            pins = fam65xx_phi2_write(cpu, pins, REG_SP, REG_PCL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            break;
            
        case 3: {
            /* PHI2: Push P with correct B flag handling to stack */
            uint8_t p_flags = CPU_P(cpu) | FLAG_U;  /* Always set U flag */
            
            /* B flag handling: only set for software BRK, not hardware interrupts */
            if (0 == (cpu->brk_flags & (FAM65XX_BRK_IRQ | FAM65XX_BRK_NMI | FAM65XX_BRK_RESET))) {
                /* Software BRK instruction - set B flag */
                p_flags |= FLAG_B;
            }
            /* Hardware interrupts (IRQ/NMI/RESET) - B flag remains clear */
            
            CPU_DL(cpu) = p_flags;
            pins = fam65xx_phi2_write(cpu, pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Decrement stack pointer and set I flag */
            CPU_S(cpu)--;
            CPU_P(cpu) |= FLAG_I;
            
            /* Check for interrupt hijacking - NMI can hijack BRK after P is pushed */
            if ((cpu->brk_flags & FAM65XX_BRK_NMI) && 
                !(cpu->brk_flags & FAM65XX_BRK_RESET)) {
                /* NMI hijacks BRK - use NMI vector instead */
                CPU_AB(cpu) = 0xFFFA;
            } else {
                /* Set up vector address using helper function */
                CPU_AB(cpu) = fam65xx_get_vector_addr(cpu);
            }
            break;
        }
            
        case 4:
            /* PHI2: Read vector low byte */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_AB(cpu)++;
            
            /* PHI1: Store vector low byte and increment address */
            CPU_PCL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 5:
            /* PHI2: Read vector high byte */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Set PC to vector address and transition to fetch */
            CPU_PCH(cpu) = BUS_GET_DATA(pins);
            
            /* Clear BRK flags after interrupt handling is complete */
            cpu->brk_flags = 0;
            
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* JMP - Jump */
static bus_state_t op_jmp(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI1: Set PC to target address (no additional bus cycle needed) */
    CPU_PC(cpu) = CPU_AB(cpu);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* JSR - Jump to Subroutine */
static bus_state_t op_jsr(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte of target address */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store low byte */
            CPU_ABL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Dummy read from stack */
            pins = fam65xx_phi2_read(cpu, pins, REG_SP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            break;
            
        case 2:
            /* PHI2: Push PCH to stack */
            pins = fam65xx_phi2_write(cpu, pins, REG_SP, REG_PCH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            break;
            
        case 3:
            /* PHI2: Push PCL to stack */
            pins = fam65xx_phi2_write(cpu, pins, REG_SP, REG_PCL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(cpu)--;
            break;
            
        case 4:
            /* PHI2: Read high byte of target address */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Set PC to target address */
            CPU_ABH(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu) = CPU_AB(cpu);
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* RTI - Return from Interrupt */
static bus_state_t op_rti(fam65xx_t* cpu, bus_state_t pins) {
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
            /* PHI2: Read P from stack */
            pins = fam65xx_phi2_read(cpu, pins, REG_SP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store P and increment SP */
            CPU_P(cpu) = (BUS_GET_DATA(pins) & ~FLAG_B) | FLAG_U;
            CPU_S(cpu)++;
            break;
            
        case 3:
            /* PHI2: Read PCL from stack */
            pins = fam65xx_phi2_read(cpu, pins, REG_SP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store PCL and increment SP */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_S(cpu)++;
            break;
            
        case 4:
            /* PHI2: Read PCH from stack */
            pins = fam65xx_phi2_read(cpu, pins, REG_SP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Assemble PC and transition to fetch */
            CPU_PC(cpu) = (BUS_GET_DATA(pins) << 8) | CPU_DL(cpu);
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* RTS - Return from Subroutine */
static bus_state_t op_rts(fam65xx_t* cpu, bus_state_t pins) {
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
            /* PHI2: Read PCL from stack */
            pins = fam65xx_phi2_read(cpu, pins, REG_SP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store PCL and increment SP */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_S(cpu)++;
            break;
            
        case 3:
            /* PHI2: Read PCH from stack */
            pins = fam65xx_phi2_read(cpu, pins, REG_SP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Assemble PC */
            CPU_PC(cpu) = (BUS_GET_DATA(pins) << 8) | CPU_DL(cpu);
            break;
            
        case 4:
            /* PHI2: Dummy read from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Increment PC and transition to fetch */
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* ============================================================================
 * FLAG OPERATIONS
 * ============================================================================
 */

/* CLC - Clear Carry Flag */
static bus_state_t op_clc(fam65xx_t* cpu, bus_state_t pins) {
    return part2_flag_operation_with_dummy_cycle(cpu, pins, FLAG_C, false);
}

/* CLD - Clear Decimal Mode Flag */
static bus_state_t op_cld(fam65xx_t* cpu, bus_state_t pins) {
    return part2_flag_operation_with_dummy_cycle(cpu, pins, FLAG_D, false);
}

/* CLI - Clear Interrupt Disable Flag */
static bus_state_t op_cli(fam65xx_t* cpu, bus_state_t pins) {
    return part2_flag_operation_with_dummy_cycle(cpu, pins, FLAG_I, false);
}

/* CLV - Clear Overflow Flag */
static bus_state_t op_clv(fam65xx_t* cpu, bus_state_t pins) {
    return part2_flag_operation_with_dummy_cycle(cpu, pins, FLAG_V, false);
}

/* SEC - Set Carry Flag */
static bus_state_t op_sec(fam65xx_t* cpu, bus_state_t pins) {
    return part2_flag_operation_with_dummy_cycle(cpu, pins, FLAG_C, true);
}

/* SED - Set Decimal Mode Flag */
static bus_state_t op_sed(fam65xx_t* cpu, bus_state_t pins) {
    return part2_flag_operation_with_dummy_cycle(cpu, pins, FLAG_D, true);
}

/* SEI - Set Interrupt Disable Flag */
static bus_state_t op_sei(fam65xx_t* cpu, bus_state_t pins) {
    return part2_flag_operation_with_dummy_cycle(cpu, pins, FLAG_I, true);
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif