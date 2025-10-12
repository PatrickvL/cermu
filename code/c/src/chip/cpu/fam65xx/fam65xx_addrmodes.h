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
            /* PHI2: Read low byte from PC and increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store low byte and increment PC */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1:
            /* PHI2: Read high byte from PC and increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store high byte and increment PC */
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Absolute,X - operand at $nnnn + X (may skip cycle if no page cross) */
static bus_state_t am_abx(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte from PC and increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store low byte */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1: {
            /* PHI2: Read high byte from PC and increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store high byte and add X */
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            
            uint16_t base = CPU_AB(cpu);
            uint16_t effective = base + CPU_X(cpu);
            CPU_AB(cpu) = effective;
            
            /* Skip cycle 2 if page_cross flag set and no page crossed */
            if (cpu->opcode_entry.page_cross && !fam65xx_page_crossed(base, effective)) {
                fam65xx_transition_to_operation(cpu);
            }
            break;
        }
            
        case 2:
            /* PHI2: Page cross penalty cycle */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;

            /* PHI1: Complete addressing */
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Absolute,Y - operand at $nnnn + Y (may skip cycle if no page cross) */
static bus_state_t am_aby(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read low byte from PC and increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store low byte */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1: {
            /* PHI2: Read high byte from PC and increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store high byte and add Y */
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            
            uint16_t base = CPU_AB(cpu);
            uint16_t effective = base + CPU_Y(cpu);
            CPU_AB(cpu) = effective;
            
            if (cpu->opcode_entry.page_cross && !fam65xx_page_crossed(base, effective)) {
                fam65xx_transition_to_operation(cpu);
            }
            break;
        }
            
        case 2:
            /* PHI2: Page cross penalty cycle */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Complete addressing */
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indexed Indirect - operand at (($nn + X) & 0xFF) */
static bus_state_t am_idx(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read pointer from PC and increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store pointer */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Dummy read from ZP */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Dummy cycle - no operation */
            break;
            
        case 2:
            /* PHI2: Read low byte of target from ZP+X */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store target low byte and increment pointer */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_ADL(cpu) = (CPU_ADL(cpu) + CPU_X(cpu) + 1) & 0xFF;
            break;
            
        case 3:
            /* PHI2: Read high byte of target from ZP+X+1 */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Assemble final address */
            CPU_ADL(cpu) = CPU_DL(cpu);
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Indirect Indexed - operand at ($nn) + Y (may skip cycle if no page cross) */
static bus_state_t am_idy(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read pointer from PC and increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store pointer */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Read low byte from ZP pointer */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store low byte and increment pointer */
            CPU_DL(cpu) = BUS_GET_DATA(pins);
            CPU_ADL(cpu) = (CPU_ADL(cpu) + 1) & 0xFF;
            break;
            
        case 2: {
            /* PHI2: Read high byte from ZP pointer+1 */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Calculate effective address with Y */
            uint16_t base = (BUS_GET_DATA(pins) << 8) | CPU_DL(cpu);
            uint16_t effective = base + CPU_Y(cpu);
            CPU_AB(cpu) = effective;
            
            if (cpu->opcode_entry.page_cross && !fam65xx_page_crossed(base, effective)) {
                fam65xx_transition_to_operation(cpu);
            }
            break;
        }
            
        case 3:
            /* PHI2: Page cross penalty cycle */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Complete addressing */
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
            
            /* PHI1: Store pointer low byte */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Read high byte of pointer address from PC */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store pointer high byte */
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
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
            CPU_ADL(cpu) = (CPU_ADL(cpu) + 1) & 0xFF;
            break;
            
        case 3:
            /* PHI2: Read high byte of target address */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Assemble final target address */
            CPU_ADL(cpu) = CPU_DL(cpu);
            CPU_ADH(cpu) = BUS_GET_DATA(pins);
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Zero Page - operand at $00nn */
static bus_state_t am_zp(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read from PC and increment */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;

    /* PHI1: Store operand address in ADL and increment PC */
    CPU_ADL(cpu) = BUS_GET_DATA(pins);
    CPU_PC(cpu)++;
    fam65xx_transition_to_operation(cpu);
    return pins;
}

/* Zero Page,X - operand at ($00nn + X) & 0xFF */
static bus_state_t am_zpx(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read base address from PC and increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store base address and increment PC */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1:
            /* PHI2: Dummy read from ZP while adding X */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Add X to address */
            CPU_ADL(cpu) = (CPU_ADL(cpu) + CPU_X(cpu)) & 0xFF;
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Zero Page,Y - operand at ($00nn + Y) & 0xFF */
static bus_state_t am_zpy(fam65xx_t* cpu, bus_state_t pins) {
    switch (cpu->cycle_index++) {
        case 0:
            /* PHI2: Read base address from PC and increment */
            pins = fam65xx_phi2_read(cpu, pins, REG_PC);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store base address and increment PC */
            CPU_ADL(cpu) = BUS_GET_DATA(pins);
            CPU_PC(cpu)++;
            break;
            
        case 1:
            /* PHI2: Dummy read from ZP while adding Y */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Add Y to address */
            CPU_ADL(cpu) = (CPU_ADL(cpu) + CPU_Y(cpu)) & 0xFF;
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif