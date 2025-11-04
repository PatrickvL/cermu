/*
 * control.inc.hpp - Control Flow Operations for MOS 65xx Family
 *
 * This file contains template member function implementations that are
 * included within the fam65xx_t template class.
 */

#include "inc_lint_prevention.hpp"

#ifndef FAM65XX_SKIP_IMPLEMENTATION

// ============================================================================
// JUMP OPERATIONS
// ============================================================================

/* JMP - Jump */
bus_state_t op_jmp(bus_state_t pins) {
    /* Set PC to target address (addressing mode has already set up AB register) */
    this->set(REG_PC, this->get(REG_AB));
    transition_to_fetch();
    return pins;
}

/* JSR - Jump to Subroutine */
bus_state_t op_jsr(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* PHI2: Read low byte of target address from PC directly to ABL */
            pins = phi2_read(pins, REG_PC, REG_ABL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Dummy read from stack pointer (internal operation) */
            pins = phi2_dummy_read(pins, REG_SP);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            /* PHI2: Push PCH (high byte of return address) to stack */
            if (this->should_complete_write_cycle(pins)) {
                pins = this->phi2_write(pins, (REG_SP), this->get(REG_PCH));
                this->dec(REG_S);
                this->cycle_index++;
            }
            return pins;
            
        case 3:
            /* PHI2: Push PCL (low byte of return address) to stack */
            if (this->should_complete_write_cycle(pins)) {
                pins = this->phi2_write(pins, (REG_SP), this->get(REG_PCL));
                this->dec(REG_S);
                this->cycle_index++;
            }
            return pins;
            
        case 4:
            /* PHI2: Read high byte of target address from PC directly to ABH */
            pins = phi2_read(pins, REG_PC, REG_ABH);
            if (FAM65XX_GET_RDY(pins)) {
                /* PHI1: Set PC to target address */
                this->set(REG_PC, this->get(REG_AB));
                transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

/* RTS - Return from Subroutine */
bus_state_t op_rts(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* PHI2: Dummy read from PC */
            pins = phi2_dummy_read(pins, REG_PC);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Dummy read from current stack pointer, then increment SP */
            pins = phi2_dummy_read(pins, REG_SP);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_S);
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            /* PHI2: Pull PCL from stack */
            pins = phi2_read(pins, REG_SP, REG_PCL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_S);
                this->cycle_index++;
            }
            return pins;
            
        case 3:
            /* PHI2: Pull PCH from stack */
            pins = phi2_read(pins, REG_SP, REG_PCH);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 4:
            /* PHI2: Dummy read from PC, then increment PC */
            pins = phi2_dummy_read(pins, REG_PC);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

// ============================================================================
// INTERRUPT OPERATIONS
// ============================================================================

/* Helper function to get interrupt vector address based on active_interrupt and CPU type
 *
 * Uses direct enum indexing for clean, efficient vector determination.
 * Higher priority interrupts use higher enum values in active_interrupt.
 *
 * INTERRUPT TYPES AND VECTOR TABLE LAYOUT:
 * ========================================
 * Enum | Interrupt Type      | Priority | 6502/65C02 | 65C816 Emul | 65C816 Native
 * -----|---------------------|----------|------------|-------------|---------------
 *  0   | FAM65XX_INT_NONE    |  N/A     |   0xFFFE   |   0xFFFE    |    0xFFE6
 *  1   | FAM65XX_INT_BRK     |  Lowest  |   0xFFFE   |   0xFFFE    |    0xFFE6
 *  2   | FAM65XX_INT_IRQ     |    ↑     |   0xFFFE   |   0xFFFE    |    0xFFEE
 *  3   | FAM65XX_INT_COP#    |    |     |   N/A*     |   0xFFF4    |    0xFFE4
 *  4   | FAM65XX_INT_NMI     |    |     |   0xFFFA   |   0xFFFA    |    0xFFEA
 *  5   | FAM65XX_INT_ABORT#  |    ↓     |   N/A*     |   0xFFF8    |    0xFFE8
 *  6   | FAM65XX_INT_RESET   | Highest  |   0xFFFC   |   0xFFFC    |    0xFFFC
 *
 * # = Only available on 65C816 CPU's
 * * = Not supported on this CPU, conditional compilation prevents usage
 *
 * LOOKUP METHOD:
 * ==============
 * The active_interrupt enum serves directly as an index into vector tables.
 * This eliminates the need for bit manipulation and provides clean lookup.
 */
uint16_t get_vector_addr() const {
    // Vector lookup tables indexed by interrupt enum values
    static constexpr uint16_t standard_vectors[7] = {
        0xFFFE, // FAM65XX_INT_NONE: Default to BRK/IRQ vector
        0xFFFE, // FAM65XX_INT_BRK: Software interrupt and default/fallback
        0xFFFE, // FAM65XX_INT_IRQ: Hardware interrupt
        0xFFF4, // FAM65XX_INT_COP: CoProcessor (65C816 emulation mode)
        0xFFFA, // FAM65XX_INT_NMI: Non-maskable interrupt
        0xFFF8, // FAM65XX_INT_ABORT: Memory abort (65C816 emulation mode)
        0xFFFC  // FAM65XX_INT_RESET: Reset vector
    };
    
    static constexpr uint16_t native_65C816_vectors[7] = {
        0xFFE6, // FAM65XX_INT_NONE: Default to BRK vector (native mode)
        0xFFE6, // FAM65XX_INT_BRK: Software interrupt and default (native mode)
        0xFFEE, // FAM65XX_INT_IRQ: Hardware interrupt (native mode)
        0xFFE4, // FAM65XX_INT_COP: CoProcessor (native mode)  
        0xFFEA, // FAM65XX_INT_NMI: Non-maskable interrupt (native mode)
        0xFFE8, // FAM65XX_INT_ABORT: Memory abort (native mode)
        0xFFFC  // FAM65XX_INT_RESET: Reset vector (same in both modes)
    };
    
    // Use active_interrupt directly as table index - much simpler!
    const int interrupt_index = static_cast<int>(this->active_interrupt);
    
    // Select appropriate vector table and return vector address
    if constexpr (has_wide_registers()) {
        if (!this->get_emulation_mode()) {
            return native_65C816_vectors[interrupt_index];
        }
    }
    return standard_vectors[interrupt_index];
}

/* BRK - Break (Software Interrupt) */
bus_state_t op_brk(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* PHI2: Dummy read from PC+1 (BRK has optional signature byte) */
            pins = phi2_dummy_read(pins, REG_PC);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_PC);
                /* Set interrupt type if not already set by hardware interrupt detection */
                if (this->active_interrupt == FAM65XX_INT_NONE) {
                    this->active_interrupt = FAM65XX_INT_BRK;
                }
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Push PCH to stack */
            if (this->should_complete_write_cycle(pins)) {
                pins = this->phi2_write(pins, (REG_SP), this->get(REG_PCH));
                this->dec(REG_S);
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            /* PHI2: Push PCL to stack */
            if (this->should_complete_write_cycle(pins)) {
                pins = this->phi2_write(pins, (REG_SP), this->get(REG_PCL));
                this->dec(REG_S);
                this->set(REG_DL, this->get(REG_P) | FLAG_B | FLAG_U);
                this->cycle_index++;
            }
            return pins;
            
        case 3:
            /* PHI2: Push P|B|U to stack (B flag set for BRK) */
            if (this->should_complete_write_cycle(pins)) {
                pins = this->phi2_write(pins, (REG_SP), this->get(REG_DL));
                this->dec(REG_S);
                /* Set interrupt disable flag - processor specific behavior */
                if constexpr (has_nmos_bugs()) {
                    /* NMOS 6502 always sets I flag on BRK */
                    set_flag(FLAG_I);
                } else {
                    /* CMOS 65C02 sets I flag and clears D flag on BRK/IRQ/NMI */
                    set_flag(FLAG_I);
                    clear_flag(FLAG_D);
                }
                this->set(REG_AB, this->get_vector_addr());
                this->cycle_index++;
            }
            return pins;
            
        case 4:
            /* PHI2: Read interrupt vector low byte */
            pins = phi2_read(pins, REG_AB, REG_PCL);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
                this->inc(REG_AB);
            }
            return pins;
            
        case 5:
            /* PHI2: Read interrupt vector high byte */
            pins = phi2_read(pins, REG_AB, REG_PCH);
            if (FAM65XX_GET_RDY(pins)) {
                /* Clear active interrupt - interrupt processing complete */
                this->active_interrupt = FAM65XX_INT_NONE;
                transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

/* RTI - Return from Interrupt */
bus_state_t op_rti(bus_state_t pins) {
    switch (this->cycle_index) {
        case 0:
            /* PHI2: Dummy read from PC */
            pins = phi2_dummy_read(pins, REG_PC);
            if (FAM65XX_GET_RDY(pins)) {
                this->cycle_index++;
            }
            return pins;
            
        case 1:
            /* PHI2: Dummy read from current stack pointer, then increment SP */
            pins = phi2_dummy_read(pins, REG_SP);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_S);
                this->cycle_index++;
            }
            return pins;
            
        case 2:
            /* PHI2: Pull P from stack (clear B, set U) */
            pins = phi2_read(pins, REG_SP, REG_DL);
            if (FAM65XX_GET_RDY(pins)) {
                this->set(REG_P, (this->get(REG_DL) & ~FLAG_B) | FLAG_U);
                this->inc(REG_S);
                this->cycle_index++;
            }
            return pins;
            
        case 3:
            /* PHI2: Pull PCL from stack */
            pins = phi2_read(pins, REG_SP, REG_PCL);
            if (FAM65XX_GET_RDY(pins)) {
                this->inc(REG_S);
                this->cycle_index++;
            }
            return pins;
            
        case 4:
            /* PHI2: Pull PCH from stack */
            pins = phi2_read(pins, REG_SP, REG_PCH);
            if (FAM65XX_GET_RDY(pins)) {
                transition_to_fetch();
            }
            return pins;
    }
    return pins;
}

// ============================================================================
// 65C02 ENHANCED CONTROL OPERATIONS
// ============================================================================
// NOTE: 65C816 long operations (op_jsl, op_rtl) are implemented in wide.inc.hpp
// NOTE: 65C02 branch always (op_bra) is implemented in cmos.inc.hpp
// ============================================================================

#endif // FAM65XX_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"