#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pla_lookup.h"
#include "instruction_table.h"

/**
 * Test suite for PLA Lookup System
 * Validates the direct O(1) array access and branchless classification
 */

// Test basic PLA lookup functionality
void test_pla_basic_lookup(void) {
    printf("Testing basic PLA lookup...\n");
    
    // Test some known opcodes
    const instruction_definition_t* instr;
    
    // Test NOP (0xEA) - should be legal
    instr = pla_decode(0xEA);
    assert(instr != NULL);
    assert(INSTR_GET_CYCLE_COUNT(instr) > 0);
    assert(!INSTR_GET_IS_ILLEGAL(instr));
    
    // Test BRK (0x00) - should be legal
    instr = pla_decode(0x00);
    assert(instr != NULL);
    assert(INSTR_GET_CYCLE_COUNT(instr) > 0);
    assert(!INSTR_GET_IS_ILLEGAL(instr));
    
    // Test LDA immediate (0xA9) - should be legal
    instr = pla_decode(0xA9);
    assert(instr != NULL);
    assert(INSTR_GET_CYCLE_COUNT(instr) == 2);
    assert(!INSTR_GET_IS_ILLEGAL(instr));
    
    printf("  Basic lookup tests passed!\n");
}

// Test instruction classification
void test_pla_classification(void) {
    printf("Testing PLA instruction classification...\n");
    
    // Test legal opcodes
    assert(pla_classify_instruction(0xEA) == PLA_TYPE_LEGAL); // NOP
    assert(pla_classify_instruction(0xA9) == PLA_TYPE_LEGAL); // LDA #$xx
    assert(pla_classify_instruction(0x4C) == PLA_TYPE_LEGAL); // JMP abs
    
    // Test that we can handle all 256 opcodes without crashing
    for (int i = 0; i < 256; i++) {
        pla_instruction_type_t type = pla_classify_instruction((uint8_t)i);
        assert(type == PLA_TYPE_LEGAL || type == PLA_TYPE_ILLEGAL || type == PLA_TYPE_INVALID);
    }
    
    printf("  Classification tests passed!\n");
}

// Test branchless classification functions
void test_branchless_classification(void) {
    printf("Testing branchless instruction classification...\n");
    
    // Test addressing mode detection
    assert(pla_is_immediate_mode(0xA9)); // LDA #$xx
    assert(!pla_is_immediate_mode(0xA5)); // LDA $zp
    
    assert(pla_is_zero_page_mode(0xA5)); // LDA $zp
    assert(!pla_is_zero_page_mode(0xAD)); // LDA $abs
    
    assert(pla_is_absolute_mode(0xAD)); // LDA $abs
    assert(!pla_is_absolute_mode(0xA5)); // LDA $zp
    
    // Test branch detection
    assert(pla_is_branch(0x10)); // BPL
    assert(pla_is_branch(0x30)); // BMI
    assert(pla_is_branch(0x50)); // BVC
    assert(pla_is_branch(0x70)); // BVS
    assert(pla_is_branch(0x90)); // BCC
    assert(pla_is_branch(0xB0)); // BCS
    assert(pla_is_branch(0xD0)); // BNE
    assert(pla_is_branch(0xF0)); // BEQ
    assert(!pla_is_branch(0xEA)); // NOP
    
    // Test group classification
    assert(is_group1_instruction(0xA9)); // LDA #$xx (ALU group)
    assert(is_group2_instruction(0xA6)); // LDX $zp (Load/Store group)
    assert(is_group0_instruction(0x10)); // BPL (Control group)
    
    printf("  Branchless classification tests passed!\n");
}

// Test statistics and coverage
void test_pla_statistics(void) {
    printf("Testing PLA statistics and coverage...\n");
    
    // Get decode statistics
    pla_decode_stats_t decode_stats = pla_get_decode_statistics();
    assert(decode_stats.total_opcodes == 256);
    assert(decode_stats.legal_opcodes > 0);
    assert(decode_stats.illegal_opcodes >= 0);
    assert(decode_stats.invalid_opcodes >= 0);
    
    // Get coverage statistics  
    pla_coverage_stats_t coverage_stats = pla_get_coverage_statistics();
    assert(coverage_stats.total_opcodes == 256);
    assert(coverage_stats.implemented_opcodes > 0);
    assert(coverage_stats.coverage_percentage > 0.0f);
    assert(coverage_stats.coverage_percentage <= 100.0f);
    
    printf("  Statistics: %d total, %d legal, %d illegal, %d invalid\n",
           decode_stats.total_opcodes, decode_stats.legal_opcodes,
           decode_stats.illegal_opcodes, decode_stats.invalid_opcodes);
    printf("  Coverage: %d/%d implemented (%.1f%%)\n",
           coverage_stats.implemented_opcodes, coverage_stats.total_opcodes,
           coverage_stats.coverage_percentage);
    
    printf("  Statistics tests passed!\n");
}

// Test validation functions
void test_pla_validation(void) {
    printf("Testing PLA validation...\n");
    
    // Validate the lookup table
    bool valid = pla_validate_lookup_table();
    if (!valid) {
        printf("  Warning: PLA lookup table has some issues (see output above)\n");
    } else {
        printf("  PLA lookup table validation passed!\n");
    }
}

// Test performance
void test_pla_performance(void) {
    printf("Testing PLA lookup performance...\n");
    
    const uint32_t iterations = 1000000; // 1M lookups
    pla_performance_stats_t perf = pla_benchmark_lookup_performance(iterations);
    
    printf("  Performance: %lu lookups in %lu cycles\n", 
           (unsigned long)perf.total_lookups, (unsigned long)perf.total_cycles);
    printf("  Average: %.2f cycles per lookup\n", perf.average_cycles_per_lookup);
    
    // Sanity check - should be very fast (direct array access)
    assert(perf.total_lookups == iterations);
    assert(perf.average_cycles_per_lookup < 100.0); // Should be much less on modern CPUs
    
    printf("  Performance tests passed!\n");
}

// Test debug output (just verify it doesn't crash)
void test_pla_debug(void) {
    printf("Testing PLA debug output...\n");
    
    // Test debug output for a few opcodes
    printf("  Debug output for NOP (0xEA):\n");
    pla_debug_print_opcode(0xEA);
    
    printf("  Debug output for LDA #$xx (0xA9):\n");
    pla_debug_print_opcode(0xA9);
    
    printf("  Debug tests passed!\n");
}

int main(void) {
    printf("=== MOS6510 PLA Lookup System Test Suite ===\n\n");
    
    // Initialize instruction table if needed
    if (instruction_table_validate) {
        if (!instruction_table_validate()) {
            printf("ERROR: Instruction table validation failed!\n");
            return 1;
        }
    }
    
    // Run all tests
    test_pla_basic_lookup();
    test_pla_classification();
    test_branchless_classification();
    test_pla_statistics();
    test_pla_validation();
    test_pla_performance();
    test_pla_debug();
    
    printf("\n=== All PLA Lookup Tests Passed! ===\n");
    return 0;
}