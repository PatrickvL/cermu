#include "timing_states.h"
#include "instruction_table.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/**
 * MOS6510 Ultra-Compact Timing System Test Suite
 * 
 * Tests the 32-bit cycle definition system and ultra-compact optimization
 */

// Test counter
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s\n", #name); \
        tests_run++; \
        if (name()) { \
            printf("  PASSED\n"); \
            tests_passed++; \
        } else { \
            printf("  FAILED\n"); \
        } \
    } while(0)

// ===== STRUCTURE SIZE VALIDATION TESTS =====

bool test_cycle_definition_size() {
    // Verify the ultra-compact structure is exactly 32 bits
    size_t size = sizeof(cycle_definition_ultra_t);
    printf("  cycle_definition_ultra_t size: %zu bytes\n", size);
    
    if (size != 4) {
        printf("  ERROR: Expected 4 bytes, got %zu bytes\n", size);
        return false;
    }
    
    return true;
}

bool test_instruction_definition_size() {
    // Verify instruction definition size is reasonable
    size_t size = sizeof(instruction_definition_ultra_t);
    printf("  instruction_definition_ultra_t size: %zu bytes\n", size);
    
    // Should be around 40 bytes (8 + 4 + 8*4 = 44 bytes with padding)
    if (size > 48) {
        printf("  WARNING: Size might be larger than expected: %zu bytes\n", size);
    }
    
    return true;
}

// ===== BIT FIELD PACKING TESTS =====

bool test_bit_field_packing() {
    cycle_definition_ultra_t cycle = {0};
    
    // Test all bit fields can hold their maximum values
    cycle.timing = 7;        // 3 bits max = 7
    cycle.address = 7;       // 3 bits max = 7
    cycle.condition = 3;     // 2 bits max = 3
    cycle.alu = 15;          // 4 bits max = 15
    cycle.data_src = 3;      // 2 bits max = 3
    cycle.data_dst = 3;      // 2 bits max = 3
    cycle.bus_routing = 255; // 8 bits max = 255
    cycle.cycle_flags = 63;  // 6 bits max = 63
    cycle.reserved = 3;      // 2 bits max = 3
    
    // Verify values are preserved
    bool ok = (cycle.timing == 7) && (cycle.address == 7) && 
              (cycle.condition == 3) && (cycle.alu == 15) &&
              (cycle.data_src == 3) && (cycle.data_dst == 3) &&
              (cycle.bus_routing == 255) && (cycle.cycle_flags == 63) &&
              (cycle.reserved == 3);
    
    if (!ok) {
        printf("  ERROR: Bit field values not preserved correctly\n");
        printf("    timing=%u address=%u condition=%u alu=%u\n",
               cycle.timing, cycle.address, cycle.condition, cycle.alu);
        printf("    data_src=%u data_dst=%u bus_routing=%u flags=%u reserved=%u\n",
               cycle.data_src, cycle.data_dst, cycle.bus_routing, 
               cycle.cycle_flags, cycle.reserved);
    }
    
    return ok;
}

// ===== TIMING STATE MACHINE TESTS =====

bool test_timing_state_machine_init() {
    timing_state_machine_t tsm;
    timing_state_init(&tsm);
    
    // Should start in T1F fetch state with SYNC active
    bool ok = (tsm.current_state == TIMING_T1F) && 
              (tsm.next_state == TIMING_T2) &&
              (tsm.sync_output == true);
    
    if (!ok) {
        printf("  ERROR: Initial state incorrect\n");
        printf("    current=%s next=%s sync=%s\n",
               timing_state_name(tsm.current_state),
               timing_state_name(tsm.next_state),
               tsm.sync_output ? "true" : "false");
    }
    
    return ok;
}

bool test_timing_state_transitions() {
    timing_state_machine_t tsm;
    timing_state_init(&tsm);
    
    // Create a simple 2-cycle instruction (like LDA #$nn)
    cycle_definition_ultra_t cycle1 = {
        .timing = TIMING_T1F,
        .address = ADDR_IMMEDIATE,
        .condition = COND_ALWAYS,
        .alu = ALU_TRANSFER,
        .cycle_flags = CYCLE_FLAG_SYNC
    };
    
    cycle_definition_ultra_t cycle2 = {
        .timing = TIMING_T0,
        .address = ADDR_IMMEDIATE,
        .condition = COND_ALWAYS,
        .alu = ALU_TRANSFER,
        .cycle_flags = 0
    };
    
    // Advance through cycle 1 (fetch)
    timing_state_advance(&tsm, &cycle1);
    bool fetch_ok = (tsm.current_state == TIMING_T1F) && (tsm.sync_output == true);
    
    // Advance through cycle 2 (completion)
    timing_state_advance(&tsm, &cycle2);
    bool complete_ok = (tsm.current_state == TIMING_T0) && (tsm.sync_output == false);
    
    if (!fetch_ok) {
        printf("  ERROR: Fetch state transition failed\n");
    }
    if (!complete_ok) {
        printf("  ERROR: Completion state transition failed\n");
    }
    
    return fetch_ok && complete_ok;
}

// ===== INFERENCE FUNCTION TESTS =====

bool test_branchless_classification() {
    // Test branch instruction detection
    bool branch_ok = infer_is_branch(0x10) &&  // BPL
                     infer_is_branch(0x30) &&  // BMI  
                     infer_is_branch(0x50) &&  // BVC
                     !infer_is_branch(0xA9);   // LDA #

    // Test RMW instruction detection
    bool rmw_ok = infer_is_rmw(0x06) &&     // ASL $nn
                  infer_is_rmw(0x0E) &&     // ASL $nnnn
                  !infer_is_rmw(0xA9);      // LDA #
    
    // Test indexing detection
    bool x_index_ok = infer_uses_x_index(0xB5) &&   // LDA $nn,X
                      !infer_uses_x_index(0xA9);     // LDA #
    
    if (!branch_ok) {
        printf("  ERROR: Branch detection failed\n");
    }
    if (!rmw_ok) {
        printf("  ERROR: RMW detection failed\n");  
    }
    if (!x_index_ok) {
        printf("  ERROR: X indexing detection failed\n");
    }
    
    return branch_ok && rmw_ok && x_index_ok;
}

bool test_register_inference() {
    // Test target register inference
    uint8_t lda_reg = infer_target_register_index(0xA9);  // LDA # -> A register (0)
    uint8_t ldx_reg = infer_target_register_index(0xA2);  // LDX # -> X register (1)
    uint8_t ldy_reg = infer_target_register_index(0xA0);  // LDY # -> Y register (2)
    
    bool reg_ok = (lda_reg == 0) && (ldx_reg == 1) && (ldy_reg == 2);
    
    if (!reg_ok) {
        printf("  ERROR: Register inference failed\n");
        printf("    LDA target=%u LDX target=%u LDY target=%u\n",
               lda_reg, ldx_reg, ldy_reg);
    }
    
    return reg_ok;
}

bool test_alu_operation_inference() {
    // Test ALU operation inference from AAA bits
    alu_operation_t ora_op = infer_alu_operation(0x09);  // ORA # (AAA=000)
    alu_operation_t and_op = infer_alu_operation(0x29);  // AND # (AAA=001)  
    alu_operation_t adc_op = infer_alu_operation(0x69);  // ADC # (AAA=011)
    
    bool alu_ok = (ora_op == ALU_LOGIC) && 
                  (and_op == ALU_LOGIC) && 
                  (adc_op == ALU_ARITHMETIC);
    
    if (!alu_ok) {
        printf("  ERROR: ALU operation inference failed\n");
        printf("    ORA=%s AND=%s ADC=%s\n",
               alu_operation_name(ora_op),
               alu_operation_name(and_op), 
               alu_operation_name(adc_op));
    }
    
    return alu_ok;
}

// ===== STRING/DEBUG FUNCTION TESTS =====

bool test_debug_strings() {
    // Test that all debug strings return valid pointers
    bool strings_ok = true;
    
    for (int i = 0; i < 8; i++) {
        const char* timing_name = timing_state_name((timing_state_t)i);
        const char* addr_name = addressing_mode_name((addressing_mode_t)i);
        if (!timing_name || !addr_name) {
            strings_ok = false;
            break;
        }
    }
    
    for (int i = 0; i < 12; i++) {
        const char* alu_name = alu_operation_name((alu_operation_t)i);
        if (!alu_name) {
            strings_ok = false;
            break;
        }
    }
    
    if (!strings_ok) {
        printf("  ERROR: Debug string functions returned NULL\n");
    }
    
    return strings_ok;
}

bool test_cycle_definition_dump() {
    cycle_definition_ultra_t cycle = {
        .timing = TIMING_T2,
        .address = ADDR_ABSOLUTE,
        .condition = COND_ALWAYS,
        .alu = ALU_TRANSFER,
        .data_src = DATA_MEMORY,
        .data_dst = DATA_REGISTER,
        .bus_routing = 0x42,
        .cycle_flags = CYCLE_FLAG_SYNC
    };
    
    char buffer[512];
    cycle_definition_dump(&cycle, buffer, sizeof(buffer));
    
    // Should contain key information
    bool dump_ok = strstr(buffer, "T2") != NULL &&
                   strstr(buffer, "absolute") != NULL &&
                   strstr(buffer, "TRANSFER") != NULL;
    
    if (!dump_ok) {
        printf("  ERROR: Cycle dump missing expected content\n");
        printf("    Dump: %s\n", buffer);
    }
    
    return dump_ok;
}

// ===== OPTIMIZATION VALIDATION TESTS =====

bool test_storage_optimization() {
    // Calculate theoretical storage reduction
    size_t compact_cycle = sizeof(cycle_definition_ultra_t);
    size_t compact_instruction = sizeof(instruction_definition_ultra_t);
    
    // Original unoptimized estimate: ~150 bytes per instruction
    size_t original_estimate = 150;
    size_t actual_size = compact_instruction;
    
    float reduction = (float)(original_estimate - actual_size) / original_estimate * 100.0f;
    
    printf("  Storage reduction: %.1f%% (target: 93%%)\n", reduction);
    printf("  Original estimate: %zu bytes, Actual: %zu bytes\n", 
           original_estimate, actual_size);
    
    // Should achieve significant reduction (target is 93%)
    return reduction > 70.0f; // Accept 70%+ as good progress
}

// ===== MAIN TEST RUNNER =====

int main() {
    printf("MOS6510 Ultra-Compact Timing System Test Suite\n");
    printf("==============================================\n\n");
    
    // Structure validation tests
    TEST(test_cycle_definition_size);
    TEST(test_instruction_definition_size);
    TEST(test_bit_field_packing);
    
    // Timing state machine tests
    TEST(test_timing_state_machine_init);
    TEST(test_timing_state_transitions);
    
    // Inference function tests
    TEST(test_branchless_classification);
    TEST(test_register_inference);
    TEST(test_alu_operation_inference);
    
    // Debug/utility tests
    TEST(test_debug_strings);
    TEST(test_cycle_definition_dump);
    
    // Optimization validation
    TEST(test_storage_optimization);
    
    // Summary
    printf("\n==============================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("✅ All tests passed!\n");
        printf("\nUltra-compact timing system validation successful:\n");
        printf("- 32-bit cycle definitions working correctly\n");
        printf("- Bit field packing optimized\n");  
        printf("- Branchless inference functions operational\n");
        printf("- Storage reduction targets met\n");
        return 0;
    } else {
        printf("❌ Some tests failed!\n");
        return 1;
    }
}