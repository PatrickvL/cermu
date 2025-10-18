#pragma once
/*
 * fam65xx_illegal_operations.hpp - MOS 65xx Family Illegal/Undocumented Operations (C++ Version)
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

#include "fam65xx_types.hpp"
#include "fam65xx_utils.hpp"
#include "fam65xx_helpers.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef AIEMUC_IMPL

/* ============================================================================
 * LOAD/STORE COMBINATION OPERATIONS
 * ============================================================================
 */

/* LAX - Load A and X with unstable immediate mode behavior */
static bus_state_t op_lax(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand using common pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Load both A and X */
    uint8_t data = BUS_GET_DATA(pins);
    
    if (cpu->opcode_entry.am_index == AM_IMM) {
        /* LAX immediate mode has unstable behavior - testing 0xEE constant like XAA
         * XAA pattern: (A | 0xEE) & X & operand works perfectly
         * Testing similar pattern for LAX: (A | 0xEE) & operand */
        data = (CPU_A(cpu) | 0xEE) & data;
    }
    
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
    /* PHI2: Read operand using common pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Perform AND and set carry to bit 7 */
    CPU_A(cpu) &= BUS_GET_DATA(pins);
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    CPU_P(cpu) = (CPU_P(cpu) & ~FLAG_C) | ((CPU_A(cpu) & 0x80) ? FLAG_C : 0);
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ARR - AND + ROR with BCD correction in decimal mode
 *
 * ARR is a complex illegal opcode with different behavior in decimal vs binary mode:
 * - Binary mode: A & operand, then ROR with proper flag calculations
 * - Decimal mode: A & operand, then ROR, then BCD adjustment with special flag behavior
 */
static bus_state_t op_arr(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand (ARR is immediate mode only) */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    CPU_PC(cpu)++;
    
    /* PHI1: Perform AND then ROR, then mode-dependent processing */
    uint8_t operand = BUS_GET_DATA(pins);
    uint8_t carry_in = (CPU_P(cpu) & FLAG_C) ? 0x80 : 0;
    
    /* Step 1: AND A with operand */
    CPU_A(cpu) &= operand;
    
    /* Step 2: ROR the result (both modes do this) */
    CPU_A(cpu) = (CPU_A(cpu) >> 1) | carry_in;
    
    /* Update N and Z flags normally */
    fam65xx_update_nz_flags(cpu, CPU_A(cpu));
    
    if (CPU_P(cpu) & FLAG_D) {
        /* Decimal mode - BCD adjustments following hardware behavior */
        uint8_t result = CPU_A(cpu);
        
        /* ARR decimal mode uses a different BCD correction algorithm */
        /* Check if low nibble needs correction (>= 0x0A) */
        if ((result & 0x0F) >= 0x0A) {
            result += 0x06;
        }
        
        /* Check if high nibble needs correction (>= 0xA0) */
        if ((result & 0xF0) >= 0xA0) {
            result += 0x60;
            CPU_P(cpu) |= FLAG_C;
        } else {
            CPU_P(cpu) &= ~FLAG_C;
        }
        
        CPU_A(cpu) = result;
        
        /* Update flags after BCD correction */
        fam65xx_update_nz_flags(cpu, CPU_A(cpu));
        
        /* V flag: bit 6 XOR bit 5 of final result */
        CPU_P(cpu) = (CPU_P(cpu) & ~FLAG_V) |
                     (((CPU_A(cpu) & 0x40) ^ ((CPU_A(cpu) & 0x20) << 1)) ? FLAG_V : 0);
    } else {
        /* Binary mode - special C and V flag behavior */
        /* ARR has special C and V flag behavior:
         * C = bit 6 of result (not the shifted-out bit!)
         * V = bit 6 XOR bit 5 of result */
        CPU_P(cpu) = (CPU_P(cpu) & ~(FLAG_C | FLAG_V)) |
                     ((CPU_A(cpu) & 0x40) ? FLAG_C : 0) |
                     (((CPU_A(cpu) & 0x40) ^ ((CPU_A(cpu) & 0x20) << 1)) ? FLAG_V : 0);
    }
    
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* ASR - AND + LSR */
static bus_state_t op_asr(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand using common pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
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
    /* PHI2: Read operand using common pattern */
    pins = read_operand_immediate_or_memory(cpu, pins);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
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

/* XAA - Unstable illegal opcode with chip-dependent behavior
 * Most common behavior: (A | 0xEE) & X & operand -> A
 * Some chips use 0xFF, 0x00, or other constants instead of 0xEE */
static bus_state_t op_xaa(fam65xx_t* cpu, bus_state_t pins) {
    /* PHI2: Read operand (immediate mode only) */
    pins = fam65xx_phi2_read(cpu, pins, REG_PC);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    CPU_PC(cpu)++;
    
    /* PHI1: Perform XAA with hardware-accurate unstable behavior
     * XAA = (A | CONST) & X & operand -> A
     * Using 0xEE as the most common constant for ProcessorTests compatibility */
    uint8_t operand = BUS_GET_DATA(pins);
    CPU_A(cpu) = (CPU_A(cpu) | 0xEE) & CPU_X(cpu) & operand;
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

/* SHA (AHX, AXA) - Store A & X & (H+1)
 * 
 * HARDWARE QUIRK BEHAVIOR:
 * When a page cross occurs in indexed addressing (ABY or IDY):
 * 
 * 1. The value written is ALWAYS: A & X & (intermediate_high + 1)
 *    where intermediate_high is the high byte BEFORE page cross correction
 * 
 * 2. The address written to gets CORRUPTED:
 *    - Normal address would be: corrected_high:corrected_low
 *    - Actual address becomes: (A & X & (intermediate_high + 1)):corrected_low
 *    - The high byte of the write address is replaced by the AND result!
 * 
 * 3. When NO page cross occurs:
 *    - Value: A & X & (high + 1) (normal)
 *    - Address: Not corrupted (normal)
 * 
 * Example with page cross:
 *   SHA $1234,Y where Y=$FF, A=$80, X=$FF
 *   Base: $1234
 *   Effective: $1234 + $FF = $1333 (page cross from $12 to $13)
 *   Intermediate high: $12 (before carry)
 *   Corrected high: $13 (after carry)
 *   
 *   Value calculation: $80 & $FF & ($12 + 1) = $80 & $FF & $13 = $00
 *   Address corruption: High byte = $00 (the value!)
 *   Final write: $00 to address $0033 (not $1333!)
 */
/* SHA (AHX, AXA) - Store A & X & (H+1)
 * 
 * HARDWARE QUIRK BEHAVIOR:
 * When a page cross occurs in indexed addressing (ABY or IDY):
 * 
 * 1. The value written is ALWAYS: A & X & (intermediate_high + 1)
 *    where intermediate_high is the high byte BEFORE page cross correction
 * 
 * 2. The address written to gets CORRUPTED:
 *    - Normal address would be: corrected_high:corrected_low
 *    - Actual address becomes: (A & X & (intermediate_high + 1)):corrected_low
 *    - The high byte of the write address is replaced by the AND result!
 * 
 * 3. When NO page cross occurs:
 *    - Value: A & X & (high + 1) (normal)
 *    - Address: Not corrupted (normal)
 * 
 * Example with page cross:
 *   SHA $1234,Y where Y=$FF, A=$80, X=$FF
 *   Base: $1234
 *   Effective: $1234 + $FF = $1333 (page cross from $12 to $13)
 *   Intermediate high: $12 (before carry)
 *   Corrected high: $13 (after carry)
 *   
 *   Value calculation: $80 & $FF & ($12 + 1) = $80 & $FF & $13 = $00
 *   Address corruption: High byte = $00 (the value!)
 *   Final write: $00 to address $0033 (not $1333!)
 */
/* SHA (AHX, AXA) - Store A & X & (H+1) with address corruption on page cross */
static bus_state_t op_sha(fam65xx_t* cpu, bus_state_t pins) {
    /* Calculate value: A & X & (intermediate_high + 1) */
    uint8_t data_value = CPU_A(cpu) & CPU_X(cpu) & (CPU_DL(cpu) + 1);
    
    /* Apply address corruption on page cross (DL != ABH means page crossed) */
    if (CPU_DL(cpu) != CPU_ABH(cpu)) {
        CPU_ABH(cpu) = data_value;
    }
    
    /* Set data to write */
    CPU_DL(cpu) = data_value;
    
    /* PHI2: Write to (possibly corrupted) address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SHS (TAS) - Store A & X & (H+1), Set S to A & X */
static bus_state_t op_shs(fam65xx_t* cpu, bus_state_t pins) {
    /* Calculate A & X first (used for both value and S) */
    uint8_t ax = CPU_A(cpu) & CPU_X(cpu);
    
    /* Calculate value: (A & X) & (intermediate_high + 1) */
    uint8_t data_value = ax & (CPU_DL(cpu) + 1);
    
    /* Apply address corruption on page cross */
    if (CPU_DL(cpu) != CPU_ABH(cpu)) {
        CPU_ABH(cpu) = data_value;
    }
    
    /* Set data to write */
    CPU_DL(cpu) = data_value;
    
    /* PHI2: Write to (possibly corrupted) address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Set stack pointer to A & X (unique to SHS) */
    CPU_S(cpu) = ax;
    
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SHX (XAS, SXA) - Store X & (H+1) */
static bus_state_t op_shx(fam65xx_t* cpu, bus_state_t pins) {
    /* Calculate value: X & (intermediate_high + 1) */
    uint8_t data_value = CPU_X(cpu) & (CPU_DL(cpu) + 1);
    
    /* Apply address corruption on page cross (DL != ABH means page crossed) */
    if (CPU_DL(cpu) != CPU_ABH(cpu)) {
        CPU_ABH(cpu) = data_value;
    }
    
    /* Set data to write */
    CPU_DL(cpu) = data_value;
    
    /* PHI2: Write to (possibly corrupted) address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

/* SHY (SYA, SAY) - Store Y & (H+1) */
static bus_state_t op_shy(fam65xx_t* cpu, bus_state_t pins) {
    /* Calculate value: Y & (intermediate_high + 1) */
    uint8_t data_value = CPU_Y(cpu) & (CPU_DL(cpu) + 1);
    
    /* Apply address corruption on page cross (DL != ABH means page crossed) */
    if (CPU_DL(cpu) != CPU_ABH(cpu)) {
        CPU_ABH(cpu) = data_value;
    }
    
    /* Set data to write */
    CPU_DL(cpu) = data_value;
    
    /* PHI2: Write to (possibly corrupted) address */
    pins = fam65xx_phi2_write(cpu, pins, REG_AB, REG_DL);
    if (!FAM65XX_GET_RDY(pins)) return pins;
    
    /* PHI1: Complete instruction */
    fam65xx_transition_to_fetch(cpu);
    return pins;
}

#endif /* AIEMUC_IMPL */

#ifdef __cplusplus
}
#endif