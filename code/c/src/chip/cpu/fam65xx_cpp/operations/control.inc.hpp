/*
 * control.inc - Control Flow Operations for MOS 65xx Family
 *
 * This file contains control flow operation implementations (JMP, JSR, RTS, etc.)
 * that are included within the fam65xx_t template class.
 */

// ============================================================================
// JUMP OPERATIONS
// ============================================================================

/* JMP - Jump */
bus_state_t op_jmp(bus_state_t pins) {
    /* Set PC to target address (addressing mode has already set up AB register) */
    CPU_PC(this) = CPU_AB(this);
    transition_to_fetch();
    return pins;
}

/* JSR - Jump to Subroutine */
bus_state_t op_jsr(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            /* PHI2: Read low byte of target address from PC directly to ABL */
            pins = phi2_read(pins, REG_PC, REG_ABL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            break;
            
        case 1:
            /* PHI2: Dummy read from stack pointer (internal operation) */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            break;
            
        case 2:
            /* PHI2: Push PCH (high byte of return address) to stack */
            CPU_DL(this) = CPU_PCH(this);
            pins = phi2_write(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(this)--;
            break;
            
        case 3:
            /* PHI2: Push PCL (low byte of return address) to stack */
            CPU_DL(this) = CPU_PCL(this);
            pins = phi2_write(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Decrement stack pointer */
            CPU_S(this)--;
            break;
            
        case 4:
            /* PHI2: Read high byte of target address from PC directly to ABH */
            pins = phi2_read(pins, REG_PC, REG_ABH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Set PC to target address */
            CPU_PC(this) = CPU_AB(this);
            transition_to_fetch();
            break;
    }
    return pins;
}

/* RTS - Return from Subroutine */
bus_state_t op_rts(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            /* PHI2: Dummy read from PC */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            break;
            
        case 1:
            /* PHI2: Dummy read from current stack pointer, then increment SP */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_S(this)++;
            break;
            
        case 2:
            /* PHI2: Pull PCL from stack */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PCL(this) = CPU_DL(this);
            CPU_S(this)++;
            break;
            
        case 3:
            /* PHI2: Pull PCH from stack */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PCH(this) = CPU_DL(this);
            break;
            
        case 4:
            /* PHI2: Dummy read from PC, then increment PC */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            transition_to_fetch();
            break;
    }
    return pins;
}

// ============================================================================
// INTERRUPT OPERATIONS
// ============================================================================

/* BRK - Break (Software Interrupt) */
bus_state_t op_brk(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            /* PHI2: Dummy read from PC+1 (BRK has optional signature byte) */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(this)++;
            break;
            
        case 1:
            /* PHI2: Push PCH to stack */
            CPU_DL(this) = CPU_PCH(this);
            pins = phi2_write_internal(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_S(this)--;
            break;
            
        case 2:
            /* PHI2: Push PCL to stack */
            CPU_DL(this) = CPU_PCL(this);
            pins = phi2_write_internal(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_S(this)--;
            break;
            
        case 3:
            /* PHI2: Push P|B|U to stack (B flag set for BRK) */
            CPU_DL(this) = CPU_P(this) | FLAG_B | FLAG_U;
            pins = phi2_write_internal(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_S(this)--;
            
            /* PHI1: Set interrupt disable flag */
            CPU_P(this) |= FLAG_I;
            break;
            
        case 4:
            /* PHI2: Read IRQ vector low byte from $FFFE */
            CPU_AB(this) = 0xFFFE;
            pins = phi2_read(pins, REG_AB, REG_PCL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            break;
            
        case 5:
            /* PHI2: Read IRQ vector high byte from $FFFF */
            CPU_AB(this) = 0xFFFF;
            pins = phi2_read(pins, REG_AB, REG_PCH);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            transition_to_fetch();
            break;
    }
    return pins;
}

/* RTI - Return from Interrupt */
bus_state_t op_rti(bus_state_t pins) {
    switch (this->cycle_index++) {
        case 0:
            /* PHI2: Dummy read from PC */
            pins = phi2_read(pins, REG_PC, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            break;
            
        case 1:
            /* PHI2: Dummy read from current stack pointer, then increment SP */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_S(this)++;
            break;
            
        case 2:
            /* PHI2: Pull P from stack (clear B, set U) */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_P(this) = (CPU_DL(this) & ~FLAG_B) | FLAG_U;
            CPU_S(this)++;
            break;
            
        case 3:
            /* PHI2: Pull PCL from stack */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PCL(this) = CPU_DL(this);
            CPU_S(this)++;
            break;
            
        case 4:
            /* PHI2: Pull PCH from stack */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PCH(this) = CPU_DL(this);
            transition_to_fetch();
            break;
    }
    return pins;
}

// ============================================================================
// 65C02 ENHANCED CONTROL OPERATIONS
// ============================================================================
// NOTE: 65C816 long operations (op_jsl, op_rtl) are implemented in wide.inc.hpp
// NOTE: 65C02 branch always (op_bra) is implemented in cmos.inc.hpp
// ============================================================================