#pragma once
/*
 * fam65xx_addressing.hpp - MOS 65xx Family CPU Addressing Mode Handlers (C++ Version)
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

#include "fam65xx_core.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

// Forward declarations for internal functions
static bus_state_t fam65xx_phi2_read(fam65xx_t* cpu, bus_state_t pins, reg16_t addr_reg);

/* Transition from addressing mode to operation - defined in fam65xx_tables.hpp */
static void fam65xx_transition_to_operation(fam65xx_t* cpu);

/* ============================================================================
 * HELPER FUNCTIONS FOR CODE DEDUPLICATION
 * ============================================================================
 */

/* Helper for zero page indexed addressing (ZPX, ZPY) */
static inline bus_state_t addrmodes_zpx_helper(fam65xx_t* cpu, bus_state_t pins, uint8_t index_reg) {
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
            /* PHI2: Dummy read from ZP while adding index */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Add index to ZP address, then copy to AB */
            CPU_ZPL(cpu) += index_reg;
            CPU_AB(cpu) = CPU_ZP(cpu); /* Copy final ZP address to AB */
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

/* Helper for absolute indexed addressing with page cross optimization (ABX, ABY) */
static inline bus_state_t addrmodes_abx_helper(fam65xx_t* cpu, bus_state_t pins, uint8_t index_reg) {
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
            
            /* PHI1: Calculate addresses */
            CPU_ABH(cpu) = BUS_GET_DATA(pins);
            uint16_t base = CPU_AB(cpu);
            uint16_t effective = base + index_reg;
            
            /* Store original high byte in DL for illegal opcodes */
            CPU_DL(cpu) = CPU_ABH(cpu);
            
            /* Add index to low byte (creates intermediate "wrong" address for page cross) */
            CPU_ABL(cpu) += index_reg;
            
            /* Skip penalty cycle if allowed and no page cross occurred */
            if (cpu->opcode_entry.can_skip_page_cross && !fam65xx_page_crossed(base, effective)) {
                CPU_AB(cpu) = effective;  /* Fix address */
                fam65xx_transition_to_operation(cpu);
            }
            /* Illegal store opcodes skip penalty cycle regardless of page cross */
            else if (cpu->opcode_entry.illegal_store) {
                CPU_AB(cpu) = effective;  /* Fix address for illegal store */
                fam65xx_transition_to_operation(cpu);
            }
            /* Otherwise continue to cycle 2 with intermediate address */
            break;
        }
            
        case 2:
            /* PHI2: Page cross penalty - read from intermediate address */
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Correct final address */
            CPU_ABH(cpu) = CPU_DL(cpu);  /* Restore original high byte */
            CPU_ABL(cpu) -= index_reg;   /* Restore original low byte */
            CPU_AB(cpu) += index_reg;    /* Correctly calculate final address */
            
            fam65xx_transition_to_operation(cpu);
            break;
    }
    return pins;
}

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
    return addrmodes_abx_helper(cpu, pins, CPU_X(cpu));
}

/* Absolute,Y - operand at $nnnn + Y (may skip cycle if no page cross) */
static bus_state_t am_aby(fam65xx_t* cpu, bus_state_t pins) {
    return addrmodes_abx_helper(cpu, pins, CPU_Y(cpu));
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
            CPU_ZPL(cpu)++;
            
            /* PHI1: Store target low byte and increment ZP pointer for high byte read */
            CPU_ABL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 3:
            /* PHI2: Read high byte of target from ZP+X+1 (read directly from ZP register) */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Store target high byte, assembling the final address */
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
            
            /* PHI1: Store pointer in ZP */
            CPU_ZPL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 1:
            /* PHI2: Read low byte from ZP pointer */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            CPU_ZPL(cpu)++;
            
            /* PHI1: Store base address low byte */
            CPU_ABL(cpu) = BUS_GET_DATA(pins);
            break;
            
        case 2: {
            /* PHI2: Read high byte from ZP pointer+1 */
            pins = fam65xx_phi2_read(cpu, pins, REG_ZP);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* PHI1: Calculate addresses */
            CPU_ABH(cpu) = BUS_GET_DATA(pins);
            uint16_t base = CPU_AB(cpu);
            uint16_t effective = base + CPU_Y(cpu);
            
            /* Store original high byte in DL for illegal opcodes */
            CPU_DL(cpu) = CPU_ABH(cpu);
            
            /* Add Y to low byte (creates intermediate address for page cross penalty) */
            CPU_ABL(cpu) += CPU_Y(cpu);
            
            /* Skip penalty cycle if allowed and no page cross */
            if (cpu->opcode_entry.can_skip_page_cross && !fam65xx_page_crossed(base, effective)) {
                CPU_AB(cpu) = effective;  /* Set correct final address */
                fam65xx_transition_to_operation(cpu);
            }
            /* Illegal store opcodes skip penalty cycle regardless of page cross */
            else if (cpu->opcode_entry.illegal_store) {
                CPU_AB(cpu) = effective;  /* Fix address for illegal store */
                fam65xx_transition_to_operation(cpu);
            }
            break;
        }
            
        case 3: {
            pins = fam65xx_phi2_read(cpu, pins, REG_AB);
            if (!FAM65XX_GET_RDY(pins)) return pins;
            
            /* DL already contains intermediate high byte from case 1 */
            
            /* Correct final address: base + Y (recalculate from scratch) */
            /* Current AB has intermediate address: wrong_high:(base_low + Y) */
            /* We need: (base_high:(base_low)) + Y */
            CPU_ABH(cpu) = CPU_DL(cpu);  /* Restore original high byte */
            CPU_ABL(cpu) -= CPU_Y(cpu);  /* Recover original base low */
            CPU_AB(cpu) += CPU_Y(cpu);   /* Calculate correct final */
            
            fam65xx_transition_to_operation(cpu);
            break;
        }
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
            CPU_ABL(cpu)++;
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
    return addrmodes_zpx_helper(cpu, pins, CPU_X(cpu));
}

/* Zero Page,Y - operand at ($00nn + Y) & 0xFF */
static bus_state_t am_zpy(fam65xx_t* cpu, bus_state_t pins) {
    return addrmodes_zpx_helper(cpu, pins, CPU_Y(cpu));
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif