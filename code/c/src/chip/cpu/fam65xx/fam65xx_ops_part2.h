#pragma once
/*
 * fam65xx_operations_part2.h - MOS 65xx Family CPU Operations (Part 2)
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

#include "fam65xx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

/* ============================================================================
 * CONTROL FLOW OPERATIONS
 * ============================================================================
 */

/* BRK - Break / Software Interrupt */
static bus_state_t op_brk(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0: {
            /* BRK does a dummy read from PC+1, then increments PC to point to PC+2 for return address */
            if (0 == (cpu->brk_flags & (FAM65XX_BRK_IRQ | FAM65XX_BRK_NMI))) {
                /* For software BRK, do dummy read from PC+1, then set PC to PC+2 for return address */
                uint16_t dummy_read_addr = CPU_PC(cpu) + 1;
                CPU_PC(cpu) += 2;  /* PC now points to PC+2 for return address */
                pins = fam65xx_phi2_read(cpu, pins, REG_AB);
                if (!FAM65XX_GET_RDY(pins)) return pins;
                /* Set up address for dummy read */
                CPU_AB(cpu) = dummy_read_addr;
            } else {
                /* For hardware interrupts, do dummy read from current PC (don't increment) */
                pins = fam65xx_phi2_read(cpu, pins, REG_PC);
                if (!FAM65XX_GET_RDY(pins)) return pins;
            }
            break;
        }
            
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
            
            /* Set up vector address using helper function */
            CPU_AB(cpu) = fam65xx_get_vector_addr(cpu);
            break;
        }
            
        case 4:
            /* PHI2: Read vector low byte */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store vector low byte and increment address */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_AB(cpu)++;
            break;
            
        case 5:
            /* PHI2: Read vector high byte */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Set PC to vector address and transition to fetch */
            CPU_PC(cpu) = (BUS_GET_DATA(pins) << 8) | CPU_DL(cpu);
            
            /* Clear BRK flags after interrupt handling is complete */
            cpu->brk_flags = 0;
            
            fam65xx_transition_to_fetch(cpu);
            break;
    }
    return pins;
}

/* JMP - Jump */
static bus_state_t op_jmp(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Dummy read (JMP target address set by addressing mode) */
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Set PC to target address */
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
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Clear carry flag */
    CPU_P(cpu) &= ~FLAG_C;
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* CLD - Clear Decimal Mode Flag */
static bus_state_t op_cld(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Clear decimal mode flag */
    CPU_P(cpu) &= ~FLAG_D;
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* CLI - Clear Interrupt Disable Flag */
static bus_state_t op_cli(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Clear interrupt disable flag */
    CPU_P(cpu) &= ~FLAG_I;
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* CLV - Clear Overflow Flag */
static bus_state_t op_clv(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Clear overflow flag */
    CPU_P(cpu) &= ~FLAG_V;
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SEC - Set Carry Flag */
static bus_state_t op_sec(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Set carry flag */
    CPU_P(cpu) |= FLAG_C;
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SED - Set Decimal Mode Flag */
static bus_state_t op_sed(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Set decimal mode flag */
    CPU_P(cpu) |= FLAG_D;
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SEI - Set Interrupt Disable Flag */
static bus_state_t op_sei(fam65xx_t* cpu, bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Set interrupt disable flag */
    CPU_P(cpu) |= FLAG_I;
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif