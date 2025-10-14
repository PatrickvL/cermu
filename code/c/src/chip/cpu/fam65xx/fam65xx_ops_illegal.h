#pragma once
/*
 * fam65xx_illegal_operations.h - MOS 65xx Family Illegal/Undocumented Operations
 * 
 * This file contains all illegal/undocumented opcodes that exist on the 6502.
 * These are not officially supported instructions but have predictable behavior
 * that some software relies on.
 *
 * Categories:
 * - Load/Store combinations (LAX, SAX)
 * - Special accumulator operations (ANC, ASR, ARR, SBX, XAA)
 * - Store with AND operations (SHA, SHS, SHX, SHY)
 * - Special load operation (LAS)
 * - 65C02 enhancement (BRA)
 * - Halt operation (JAM)
 *
 * HARDWARE QUIRK - ILLEGAL STORE OPERATIONS:
 * ==========================================
 *
 * The store-with-AND illegal opcodes (SHA, SHS, SHX, SHY) have a hardware quirk
 * when used with indexed addressing modes that cross page boundaries:
 *
 * 1. During the page cross penalty cycle, the CPU reads from an incorrect "intermediate"
 *    address where only the low byte has the index added.
 *
 * 2. On the final write cycle, these illegal opcodes corrupt the high byte of the
 *    target address by ANDing it with the data being written.
 *
 * 3. This results in unpredictable store locations when page boundaries are crossed.
 *
 * The quirk affects:
 * - SHA (Store A & X & (H+1)) with ABY, IDY addressing modes
 * - SHS (Store A & X & (H+1), set SP) with ABY addressing mode
 * - SHX (Store X & (H+1)) with ABY addressing mode
 * - SHY (Store Y & (H+1)) with ABX addressing mode
 *
 * The illegal_store flag in opcode_info_t indicates when this quirk should activate.
 */

#include "fam65xx_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CHIPS_IMPL

/* ============================================================================
 * LOAD/STORE COMBINATION OPERATIONS
 * ============================================================================
 */

/* LAX - Load A and X */
static bus_state_t op_lax(fam65xx_t* cpu, bus_state_t pins) {
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
    
    /* PHI1: Load both A and X */
    uint8_t data = BUS_GET_DATA(pins);
    CPU_A(cpu) = data;
    CPU_X(cpu) = data;
    fam65xx_update_nz_flags(cpu, data);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SAX - Store A & X */
static bus_state_t op_sax(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Write A&X to target address */
    CPU_DL(cpu) = CPU_A(cpu) & CPU_X(cpu);
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ============================================================================
 * SPECIAL ACCUMULATOR OPERATIONS
 * ============================================================================
 */

/* ANC - AND with Carry */
static bus_state_t op_anc(fam65xx_t* cpu, bus_state_t pins) {
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
    
    /* PHI1: Perform AND and set carry to bit 7 */
    CPU_A(cpu) &= BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    CPU_P(cpu) = (CPU_P(cpu) & ~FLAG_C) | ((CPU_A(cpu) & 0x80) ? FLAG_C : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ARR - AND + ROR */
static bus_state_t op_arr(fam65xx_t* cpu, bus_state_t pins) {
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
    
    /* PHI1: Perform AND then ROR with complex flag behavior */
    uint8_t carry = (CPU_P(cpu) & FLAG_C) ? 0x80 : 0;
    CPU_A(cpu) &= BUS_GET_DATA(pins);
    CPU_A(cpu) = (CPU_A(cpu) >> 1) | carry;
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_C | FLAG_V)) |
                 ((CPU_A(cpu) & 0x40) ? FLAG_C : 0) |
                 (((CPU_A(cpu) & 0x40) ^ ((CPU_A(cpu) & 0x20) << 1)) ? FLAG_V : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ASR - AND + LSR */
static bus_state_t op_asr(fam65xx_t* cpu, bus_state_t pins) {
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
    
    /* PHI1: Perform AND then LSR */
    CPU_A(cpu) &= BUS_GET_DATA(pins);
    CPU_P(cpu) = (CPU_P(cpu) & ~FLAG_C) | ((CPU_A(cpu) & 0x01) ? FLAG_C : 0);
    CPU_A(cpu) >>= 1;
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SBX - (A & X) - operand -> X */
static bus_state_t op_sbx(fam65xx_t* cpu, bus_state_t pins) {
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
    
    /* PHI1: Perform (A & X) - operand -> X */
    uint8_t data = BUS_GET_DATA(pins);
    uint16_t result = (CPU_A(cpu) & CPU_X(cpu)) - data;
    CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_N | FLAG_Z | FLAG_C)) |
                 (result & FLAG_N) |
                 ((result & 0xFF) == 0 ? FLAG_Z : 0) |
                 ((result < 0x100) ? FLAG_C : 0);
    CPU_X(cpu) = result & 0xFF;
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* XAA - Transfer X to A, then AND with immediate */
static bus_state_t op_xaa(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand (immediate mode only) */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    CPU_PC(cpu)++;
    
    /* PHI1: X -> A, then A & operand */
    CPU_A(cpu) = CPU_X(cpu) & BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ============================================================================
 * STORE WITH AND OPERATIONS
 * ============================================================================
 */

/* LAS - Load A, X, SP with (operand & SP) */
static bus_state_t op_las(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand from target address */
    pins = fam65xx_phi2_read(cpu, pins, REG_AB);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Load A, X, SP with (operand & SP) */
    uint8_t val = BUS_GET_DATA(pins) & CPU_S(cpu);
    CPU_A(cpu) = val;
    CPU_X(cpu) = val;
    CPU_S(cpu) = val;
    fam65xx_update_nz_flags(cpu, val);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SHA - Store A & X & H */
static bus_state_t op_sha(fam65xx_t* cpu, bus_state_t pins) {
    /* NON-STANDARD DL REGISTER REUSE: Check page cross penalty flag first */
    uint8_t page_cross_penalty = CPU_DL(cpu);
    
    /* CRITICAL: Calculate data using ORIGINAL high byte before any address corruption */
    uint8_t original_high = CPU_ABH(cpu);
    CPU_DL(cpu) = CPU_A(cpu) & CPU_X(cpu) & original_high;
    
    /* SHA hardware quirk: Only occurs during page crossings
     * Corruption formula varies by addressing mode */
    if (page_cross_penalty) {
        if (cpu->opcode_entry.am_index == AM_ABY || cpu->opcode_entry.am_index == AM_INY) {
            /* SHA with ABY/IDY: High byte ANDed with A & X & Y */
            CPU_ABH(cpu) &= (CPU_A(cpu) & CPU_X(cpu) & CPU_Y(cpu));
        }
    }
    
    /* PHI2: Write to (possibly corrupted) address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SHS - Store A & X & H, set SP to A & X */
static bus_state_t op_shs(fam65xx_t* cpu, bus_state_t pins) {
    /* NON-STANDARD DL REGISTER REUSE: Check page cross penalty flag first */
    uint8_t page_cross_penalty = CPU_DL(cpu);
    
    /* CRITICAL: Calculate data using ORIGINAL high byte before any address corruption */
    uint8_t original_high = CPU_ABH(cpu);
    CPU_DL(cpu) = CPU_A(cpu) & CPU_X(cpu) & original_high;
    
    /* SHS hardware quirk: Only occurs during page crossings
     * With ABY addressing: High byte ANDed with A & X */
    if (page_cross_penalty && cpu->opcode_entry.am_index == AM_ABY) {
        CPU_ABH(cpu) &= (CPU_A(cpu) & CPU_X(cpu));
    }
    
    /* PHI2: Write A&X&H to (possibly corrupted) address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Set SP to A & X */
    CPU_S(cpu) = CPU_A(cpu) & CPU_X(cpu);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SHX - Store X & H */
static bus_state_t op_shx(fam65xx_t* cpu, bus_state_t pins) {
    /* NON-STANDARD DL REGISTER REUSE: Check page cross penalty flag first */
    uint8_t page_cross_penalty = CPU_DL(cpu);
    
    /* CRITICAL: Calculate data using ORIGINAL high byte before any address corruption */
    uint8_t original_high = CPU_ABH(cpu);
    CPU_DL(cpu) = CPU_X(cpu) & original_high;
    
    /* SHX hardware quirk: Only occurs during page crossings
     * With ABY addressing: High byte ANDed with X & Y */
    if (page_cross_penalty && cpu->opcode_entry.am_index == AM_ABY) {
        CPU_ABH(cpu) &= (CPU_X(cpu) & CPU_Y(cpu));
    }
    
    /* PHI2: Write X&H to (possibly corrupted) address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SHY - Store Y & (H+1) */
static bus_state_t op_shy(fam65xx_t* cpu, bus_state_t pins) {
    /* CRITICAL: Calculate data using ORIGINAL high byte before any address corruption
     * Testing: Use H instead of H+1 based on ProcessorTests evidence */
    uint8_t original_high = CPU_ABH(cpu);
    CPU_DL(cpu) = CPU_Y(cpu) & original_high;
    
    /* SHY hardware quirk: Always occurs for illegal store opcodes */
    CPU_ABH(cpu) &= (original_high + 1);
    
    /* PHI2: Write Y&(H+1) to (possibly corrupted) address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

#endif /* CHIPS_IMPL */

#ifdef __cplusplus
}
#endif