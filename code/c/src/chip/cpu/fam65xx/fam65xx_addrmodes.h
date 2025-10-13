#pragma once
/*
 * fam65xx_addressing.h - MOS 65xx Family CPU Addressing Mode Handlers
 *
 * This file contains all addressing mode handlers that prepare addresses
 * for CPU operations. These handlers are called before operation handlers
 * and set up the target address in the Address Bus (AB) register.
 *
 * Addressing modes supported:
 * - Zero Page (ZP): $00nn
 * - Zero Page,X (ZPX): ($00nn + X) & 0xFF
 * - Zero Page,Y (ZPY): ($00nn + Y) & 0xFF
 * - Absolute (ABS): $nnnn
 * - Absolute,X (ABX): $nnnn + X (with page cross optimization)
 * - Absolute,Y (ABY): $nnnn + Y (with page cross optimization)
 * - Indirect (IND): ($nnnn) - JMP only, includes 6502 page boundary bug
 * - Indexed Indirect (INX): (($nn + X) & 0xFF)
 * - Indirect Indexed (INY): ($nn) + Y (with page cross optimization)
 *
 * Note: Immediate, Implicit, Accumulator, and Relative modes don't use
 * addressing mode handlers - they're handled directly in operations.
 */

#include "fam65xx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

// Forward declarations for internal functions
static bus_state_t fam65xx_phi2_read(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg);
static void fam65xx_transition_to_operation(fam65xx_t* cpu);

/* ============================================================================
 * ADDRESSING MODE HANDLERS
 * ============================================================================
 * These handlers prepare the address/data for operations.
 * They transition to the operation handler when complete.
 * Functions are alphabetically ordered for maintainability.
 */

/* Absolute - operand at $nnnn */
static bus_state_t am_abs(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store low byte and increment PC */
            CPU_ABL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Read high byte from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store high byte and increment PC */
            CPU_ABH(cpu) = BUS_GET_DATA(pins);
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Absolute,X - operand at $nnnn + X (may skip cycle if no page cross) */
static bus_state_t am_abx(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store low byte and increment PC */
            CPU_ABL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1: {
            /* PHI2: Read high byte from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store high byte and calculate addresses */
            CPU_ABH(cpu) = BUS_GET_DATA(pins);

            bool is_page_cross = ((uint16_t)CPU_ABL(cpu) + CPU_X(cpu)) > 0xFF;
            /* Add X to low byte only (hardware behavior) */
            CPU_ABL(cpu) += CPU_X(cpu);
            
            /* Check for page cross optimization */
            if (cpu->opcode_entry.page_cross && !is_page_cross) {
                /* No page cross - address is correct, skip penalty cycle */
                fam65xx_transition_to_operation(cpu);
            }
            /* Otherwise continue with "wrong" address for penalty cycle */
            break;
        }
            
        case 2:
            /* PHI2: Page cross penalty cycle - dummy read from wrong address */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;

            /* PHI1: Fix page cross by adding 0x100 to address */
            CPU_AB(cpu) += 0x0100;
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Absolute,Y - operand at $nnnn + Y (may skip cycle if no page cross) */
static bus_state_t am_aby(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store low byte and increment PC */
            CPU_ABL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1: {
            /* PHI2: Read high byte from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store high byte and calculate addresses */
            CPU_ABH(cpu) = BUS_GET_DATA(pins);

            bool is_page_cross = ((uint16_t)CPU_ABL(cpu) + CPU_Y(cpu)) > 0xFF;
            /* Add Y to low byte only (hardware behavior) */
            CPU_ABL(cpu) += CPU_Y(cpu);
            
            /* Check for page cross optimization */
            if (cpu->opcode_entry.page_cross && !is_page_cross) {
                /* No page cross - address is correct, skip penalty cycle */
                fam65xx_transition_to_operation(cpu);
            }
            /* Otherwise continue with "wrong" address for penalty cycle */
            break;
        }
            
        case 2:
            /* PHI2: Page cross penalty cycle - dummy read from wrong address */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Fix page cross by adding 0x100 to address */
            CPU_AB(cpu) += 0x100;
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indexed Indirect - operand at (($nn + X) & 0xFF) */
static bus_state_t am_idx(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read pointer from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store pointer in ZP register and increment PC */
            CPU_ZPL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Dummy read from ZP (before adding X) */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Calculate ZP+X during dummy cycle (hardware accurate) */
            CPU_ZPL(cpu) += CPU_X(cpu);
            break;
            
        case 2:
            /* PHI2: Read low byte of target from ZP+X (read directly from ZP register) */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store target low byte and increment ZP pointer for high byte read */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_ZPL(cpu) += 1;
            break;
            
        case 3:
            /* PHI2: Read high byte of target from ZP+X+1 (read directly from ZP register) */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Assemble final address */
            CPU_ABL(cpu) = CPU_DL(cpu);
            CPU_ABH(cpu) = BUS_GET_DATA(pins);
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indirect Indexed - operand at ($nn) + Y (may skip cycle if no page cross) */
static bus_state_t am_idy(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read pointer from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store pointer in ZP and increment PC */
            CPU_ZPL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Read low byte from ZP pointer */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store low byte and increment ZP pointer */
            CPU_ABL(cpu) = BUS_GET_DATA(pins);
            CPU_ZPL(cpu)++;
            break;
            
        case 2: {
            /* PHI2: Read high byte from ZP pointer+1 */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Calculate effective address and set up page crossing */
            CPU_ABH(cpu) = BUS_GET_DATA(pins);
            
            bool is_page_cross = ((uint16_t)CPU_ABL(cpu) + CPU_Y(cpu)) > 0xFF;
            /* Add Y to low byte only (hardware behavior) */
            CPU_ABL(cpu) += CPU_Y(cpu);
            
            /* Check for page cross optimization */
            if (cpu->opcode_entry.page_cross && !is_page_cross) {
                /* No page cross - address is correct, skip penalty cycle */
                fam65xx_transition_to_operation(cpu);
            }
            /* Otherwise continue with "wrong" address for penalty cycle */
            break;
        }
            
        case 3:
            /* PHI2: Page cross penalty cycle - dummy read from wrong address */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Fix page cross by adding 0x100 to address (elegant correction) */
            CPU_AB(cpu) += 0x0100;
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indirect - used only by JMP ($nnnn) */
static bus_state_t am_ind(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte of pointer address from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store pointer low byte and increment PC */
            CPU_ABL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Read high byte of pointer address from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store pointer high byte and increment PC */
            CPU_ABH(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 2:
            /* PHI2: Read low byte of target address from (pointer) */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store target low byte and prepare for 6502 bug */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            
            /* IMPORTANT: 6502 bug - if pointer is at page boundary (e.g., $xxFF),
             * high byte is read from $xx00 instead of $(xx+1)00
             * To emulate this bug: increment only low byte for next read */
            CPU_ABL(cpu) += 1;
            break;
            
        case 3:
            /* PHI2: Read high byte of target address */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Assemble final target address */
            CPU_ABL(cpu) = CPU_DL(cpu);
            CPU_ABH(cpu) = BUS_GET_DATA(pins);
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Zero Page - operand at $00nn */
static bus_state_t am_zp(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read from PC */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    CPU_PC(cpu)++;

    /* PHI1: Store operand address in ZP, then copy to AB and increment PC */
    CPU_ZPL(cpu) = BUS_GET_DATA(pins);
    /* Copy ZP to AB for final address (ZPH is always 0x00) */
    CPU_AB(cpu) = CPU_ZP(cpu);
    fam65xx_transition_to_operation(cpu);
    return pins;
}

/* Zero Page,X - operand at ($00nn + X) & 0xFF */
static bus_state_t am_zpx(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read base address from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store base address in ZP and increment PC */
            CPU_ZPL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Dummy read from ZP while adding X */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Add X to ZP address, then copy to AB */
            CPU_ZPL(cpu) += CPU_X(cpu);
            CPU_AB(cpu) = CPU_ZP(cpu); /* Copy final ZP address to AB */
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Zero Page,Y - operand at ($00nn + Y) & 0xFF */
static bus_state_t am_zpy(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read base address from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_PC(cpu)++;
            
            /* PHI1: Store base address in ZP and increment PC */
            CPU_ZPL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Dummy read from ZP while adding Y */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Add Y to ZP address, then copy to AB */
            CPU_ZPL(cpu) += CPU_Y(cpu);
            CPU_AB(cpu) = CPU_ZP(cpu); /* Copy final ZP address to AB */
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif