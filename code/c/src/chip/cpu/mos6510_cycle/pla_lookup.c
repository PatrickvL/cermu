#include "pla_lookup.h"
#include "instruction_table.h"
#include <stdio.h>
#include <string.h>

/**
 * MOS6510 PLA Lookup System Implementation
 * Direct O(1) array access for all 256 opcodes
 */

// ===== INSTRUCTION TYPE CLASSIFICATION =====

pla_instruction_type_t pla_classify_instruction(uint8_t opcode) {
    const instruction_definition_t* instr = pla_lookup(opcode);
    
    // Check if instruction is implemented (has cycles)
    if (INSTR_GET_CYCLE_COUNT(instr) == 0) {
        return PLA_TYPE_INVALID;
    }
    
    // Check illegal flag
    if (INSTR_GET_IS_ILLEGAL(instr)) {
        return PLA_TYPE_ILLEGAL;
    }
    
    // Legal opcodes
    return PLA_TYPE_LEGAL;
}

// ===== ADVANCED PLA DECODE FUNCTIONS =====

addressing_mode_t pla_get_effective_addressing_mode(uint8_t opcode) {
    const uint8_t bbb = get_bbb_bits(opcode);
    const uint8_t cc = get_instruction_group(opcode);
    
    // Use opcode bit patterns for addressing mode determination
    switch (cc) {
        case 0: // Group 0 (control instructions)
            switch (bbb) {
                case 0: return ADDR_IMPLIED;
                case 1: return ADDR_ZP;
                case 2: return ADDR_IMPLIED;
                case 3: return ADDR_ABSOLUTE;
                case 4: return pla_is_branch(opcode) ? ADDR_RELATIVE : ADDR_IMPLIED;
                case 5: return ADDR_ZP; // ZPX
                case 6: return ADDR_IMPLIED;
                case 7: return ADDR_ABSOLUTE; // ABSX
            }
            break;
            
        case 1: // Group 1 (ALU instructions)
            switch (bbb) {
                case 0: return ADDR_INDIRECT; // ($nn,X)
                case 1: return ADDR_ZP;
                case 2: return ADDR_IMMEDIATE;
                case 3: return ADDR_ABSOLUTE;
                case 4: return ADDR_INDIRECT; // ($nn),Y
                case 5: return ADDR_ZP; // ZPX
                case 6: return ADDR_ABSOLUTE; // ABSY
                case 7: return ADDR_ABSOLUTE; // ABSX
            }
            break;
            
        case 2: // Group 2 (RMW and Load/Store)
            switch (bbb) {
                case 0: return ADDR_IMMEDIATE;
                case 1: return ADDR_ZP;
                case 2: return ADDR_IMPLIED; // Accumulator mode
                case 3: return ADDR_ABSOLUTE;
                case 4: return ADDR_IMPLIED; // Invalid for most
                case 5: return ADDR_ZP; // ZPX/ZPY
                case 6: return ADDR_IMPLIED; // Invalid for most
                case 7: return ADDR_ABSOLUTE; // ABSX/ABSY
            }
            break;
            
        case 3: // Group 3 (illegal opcodes)
            // Illegal opcodes follow similar patterns to legal ones
            return pla_get_effective_addressing_mode(opcode & 0xFC); // Mask off CC bits
    }
    
    return ADDR_IMPLIED; // Default fallback
}

alu_operation_t pla_get_alu_operation(uint8_t opcode) {
    const instruction_definition_t* instr = pla_lookup(opcode);
    return (alu_operation_t)INSTR_GET_ALU_OPERATION(instr);
}

pla_data_flow_t pla_get_data_flow(uint8_t opcode) {
    pla_data_flow_t flow = {0};
    
    const uint8_t aaa = get_aaa_bits(opcode);
    const uint8_t cc = get_instruction_group(opcode);
    
    // Determine source and destination based on instruction group
    if (is_group1_instruction(opcode)) {
        // ALU instructions typically: Memory/Immediate → ALU → Accumulator
        flow.source_register = 0; // Memory or immediate (inferred)
        flow.destination_register = 0; // Accumulator
        flow.uses_memory = !pla_is_immediate_mode(opcode);
        flow.affects_accumulator = true;
    } else if (is_group2_instruction(opcode)) {
        // Load/Store/RMW instructions
        flow.uses_memory = true;
        if ((aaa & 0x04) == 0) { // STx instructions
            flow.source_register = aaa >> 1; // A, X, or Y
            flow.destination_register = 0; // Memory
            flow.affects_accumulator = false;
        } else { // LDx instructions
            flow.source_register = 0; // Memory
            flow.destination_register = aaa >> 1; // A, X, or Y
            flow.affects_accumulator = (aaa >> 1) == 0;
        }
    } else {
        // Control instructions
        flow.uses_memory = pla_is_branch(opcode);
        flow.affects_accumulator = false;
    }
    
    return flow;
}

uint8_t pla_get_total_cycle_count(uint8_t opcode, bool page_crossed, bool branch_taken) {
    uint8_t base_cycles = get_base_cycle_count(opcode);
    
    // Add conditional cycle adjustments
    if (page_crossed && has_variable_timing(opcode)) {
        base_cycles += calculate_page_cross_penalty(opcode, 0, 0);
    }
    
    if (branch_taken && pla_is_branch(opcode)) {
        base_cycles += calculate_branch_penalty(opcode, branch_taken, page_crossed);
    }
    
    return base_cycles;
}

// ===== TIMING ANALYSIS =====

pla_timing_adjustment_t pla_calculate_timing_adjustment(
    uint8_t opcode, uint16_t base_addr, uint16_t target_addr, bool branch_condition) {
    
    pla_timing_adjustment_t adj = {0};
    
    // Check for page crossing
    bool page_crossed = (base_addr & 0xFF00) != (target_addr & 0xFF00);
    
    if (pla_is_branch(opcode)) {
        if (branch_condition) {
            adj.branch_taken_penalty = 1;
            if (page_crossed) {
                adj.page_cross_penalty = 1;
            }
        }
    } else if (has_variable_timing(opcode) && page_crossed) {
        adj.page_cross_penalty = 1;
    }
    
    adj.total_adjustment = adj.page_cross_penalty + adj.branch_taken_penalty;
    return adj;
}

// ===== VALIDATION AND DEBUGGING =====

bool pla_validate_lookup_table(void) {
    bool valid = true;
    
    // Check all 256 opcodes for consistency
    for (int opcode = 0; opcode < 256; opcode++) {
        const instruction_definition_t* instr = pla_lookup((uint8_t)opcode);
        
        // Validate cycle count is reasonable (1-7 cycles for most instructions)
        uint8_t cycles = INSTR_GET_CYCLE_COUNT(instr);
        if (cycles > 7) {
            printf("Warning: Opcode 0x%02X has unusual cycle count: %d\n", opcode, cycles);
            valid = false;
        }
        
        // Validate illegal opcodes are marked correctly
        if (INSTR_GET_IS_ILLEGAL(instr) && !is_illegal_opcode((uint8_t)opcode)) {
            printf("Error: Opcode 0x%02X marked illegal but follows legal pattern\n", opcode);
            valid = false;
        }
    }
    
    return valid;
}

pla_decode_stats_t pla_get_decode_statistics(void) {
    pla_decode_stats_t stats = {0};
    stats.total_opcodes = 256;
    
    for (int opcode = 0; opcode < 256; opcode++) {
        pla_instruction_type_t type = pla_classify_instruction((uint8_t)opcode);
        
        switch (type) {
            case PLA_TYPE_LEGAL:    stats.legal_opcodes++; break;
            case PLA_TYPE_ILLEGAL:  stats.illegal_opcodes++; break;
            case PLA_TYPE_INVALID:  stats.invalid_opcodes++; break;
        }
    }
    
    return stats;
}

pla_coverage_stats_t pla_get_coverage_statistics(void) {
    pla_coverage_stats_t stats = {0};
    stats.total_opcodes = 256;
    
    for (int opcode = 0; opcode < 256; opcode++) {
        const instruction_definition_t* instr = pla_lookup((uint8_t)opcode);
        
        if (INSTR_GET_CYCLE_COUNT(instr) > 0) {
            stats.implemented_opcodes++;
            
            if (INSTR_GET_IS_ILLEGAL(instr)) {
                stats.illegal_opcodes++;
            } else {
                stats.legal_opcodes++;
            }
        }
    }
    
    stats.coverage_percentage = (float)stats.implemented_opcodes / 256.0f * 100.0f;
    return stats;
}

void pla_debug_print_opcode(uint8_t opcode) {
    const instruction_definition_t* instr = pla_lookup(opcode);
    pla_instruction_type_t type = pla_classify_instruction(opcode);
    
    printf("Opcode 0x%02X:\n", opcode);
    printf("  Type: ");
    switch (type) {
        case PLA_TYPE_LEGAL:    printf("Legal\n"); break;
        case PLA_TYPE_ILLEGAL:  printf("Illegal\n"); break;
        case PLA_TYPE_INVALID:  printf("Invalid\n"); break;
    }
    
    printf("  Cycles: %d\n", INSTR_GET_CYCLE_COUNT(instr));
    printf("  Group: %d (CC bits: %02X)\n", get_instruction_group(opcode), opcode & 0x03);
    printf("  AAA: %d, BBB: %d\n", get_aaa_bits(opcode), get_bbb_bits(opcode));
    printf("  Branch: %s\n", INSTR_GET_IS_BRANCH(instr) ? "Yes" : "No");
    printf("  RMW: %s\n", INSTR_GET_IS_RMW(instr) ? "Yes" : "No");
    printf("  X Index: %s\n", INSTR_GET_USES_X_INDEX(instr) ? "Yes" : "No");
    printf("  Y Index: %s\n", INSTR_GET_USES_Y_INDEX(instr) ? "Yes" : "No");
    printf("  Affects Flags: 0x%X\n", INSTR_GET_AFFECTS_FLAGS(instr));
    printf("  ALU Op: %d\n", INSTR_GET_ALU_OPERATION(instr));
}

pla_performance_stats_t pla_benchmark_lookup_performance(uint32_t iterations) {
    pla_performance_stats_t stats = {0};
    uint64_t start_cycles, end_cycles;
    
    // Simple cycle counter (platform-specific)
#ifdef __x86_64__
    __asm__ volatile ("rdtsc" : "=A" (start_cycles));
#else
    start_cycles = 0; // Fallback for non-x86 platforms
#endif
    
    // Perform lookups
    volatile const instruction_definition_t* instr;
    for (uint32_t i = 0; i < iterations; i++) {
        uint8_t opcode = (uint8_t)(i & 0xFF); // Cycle through all opcodes
        instr = pla_lookup(opcode);
        (void)instr; // Prevent optimization
    }
    
#ifdef __x86_64__
    __asm__ volatile ("rdtsc" : "=A" (end_cycles));
#else
    end_cycles = 1000; // Fallback estimate
#endif
    
    stats.total_lookups = iterations;
    stats.total_cycles = end_cycles - start_cycles;
    stats.average_cycles_per_lookup = (double)stats.total_cycles / iterations;
    
    return stats;
}
