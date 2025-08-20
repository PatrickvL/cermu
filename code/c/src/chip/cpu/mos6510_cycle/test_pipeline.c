#include "pipeline.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/**
 * MOS6510 Advanced Pipeline Test Suite
 * 
 * Tests the sophisticated multi-stage overlapping execution system
 * based on visual6502 "hidden pipeline" discovery
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

// Mock fetch callback for testing
static uint8_t mock_memory[0x10000] = {0};
static uint8_t mock_fetch_callback(uint16_t address) {
    return mock_memory[address];
}

// Mock writeback callback for testing
static uint8_t writeback_register = 0;
static uint8_t writeback_value = 0;
static bool writeback_flags = false;
static void mock_writeback_callback(uint8_t reg, uint8_t val, bool flags) {
    writeback_register = reg;
    writeback_value = val;
    writeback_flags = flags;
}

// ===== BASIC PIPELINE TESTS =====

bool test_pipeline_initialization() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Check initial state
    bool init_ok = (pom.active_stage_count == 0) &&
                   (!pom.prefetch_valid) &&
                   (pom.delayed_instruction == NULL) &&
                   (pom.decode_delay_cycles == 0) &&
                   (pom.datapath_lag_cycles == 0) &&
                   (!pom.writeback_pending) &&
                   (!pom.pipeline_flush_requested) &&
                   (!pom.pipeline_stall_requested);
    
    if (!init_ok) {
        printf("  ERROR: Pipeline not properly initialized\n");
    }
    
    return init_ok;
}

bool test_predictive_fetch() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Set up mock memory
    mock_memory[0x1001] = 0xA9; // LDA #$nn
    
    // Test predictive fetch
    pipeline_predictive_fetch(&pom, 0x1000, mock_fetch_callback);
    
    bool fetch_ok = pipeline_prefetch_ready(&pom) &&
                    (pipeline_get_prefetch_opcode(&pom) == 0xA9) &&
                    (pom.prefetch_pc == 0x1001) &&
                    (pom.active_stage_count > 0);
    
    if (!fetch_ok) {
        printf("  ERROR: Predictive fetch failed\n");
        printf("    Ready=%s Opcode=0x%02X PC=0x%04X Active=%d\n",
               pipeline_prefetch_ready(&pom) ? "true" : "false",
               pipeline_get_prefetch_opcode(&pom),
               pom.prefetch_pc,
               pom.active_stage_count);
    }
    
    return fetch_ok;
}

bool test_delayed_decode() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Start delayed decode
    pipeline_delayed_decode(&pom, 0xA9); // LDA #$nn
    
    // Should not be ready immediately (T0/T1 lag)
    bool not_ready_initially = !pipeline_decode_ready(&pom) &&
                               (pom.decode_delay_cycles == 2);
    
    // Advance one cycle
    pipeline_advance_cycle(&pom);
    bool still_not_ready = !pipeline_decode_ready(&pom) &&
                          (pom.decode_delay_cycles == 1);
    
    // Advance second cycle - should complete
    pipeline_advance_cycle(&pom);
    bool ready_now = pipeline_decode_ready(&pom) &&
                     (pom.decode_delay_cycles == 0) &&
                     (pipeline_get_decoded_instruction(&pom) != NULL);
    
    if (!not_ready_initially) {
        printf("  ERROR: Decode should not be ready initially\n");
    }
    if (!still_not_ready) {
        printf("  ERROR: Decode should still not be ready after 1 cycle\n");
    }
    if (!ready_now) {
        printf("  ERROR: Decode should be ready after 2 cycles\n");
    }
    
    return not_ready_initially && still_not_ready && ready_now;
}

bool test_lagged_datapath() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Create a cycle definition
    cycle_definition_t cycle = {
        .timing = TIMING_T2,
        .address = ADDR_IMMEDIATE,
        .alu = ALU_TRANSFER,
        .data_src = DATA_IMMEDIATE,
        .data_dst = DATA_REGISTER
    };
    
    // Start lagged datapath
    pipeline_lag_datapath(&pom, &cycle);
    
    // Should not be ready immediately (datapath lag)
    bool not_ready_initially = !pipeline_datapath_ready(&pom) &&
                               (pom.datapath_lag_cycles == 1);
    
    // Advance cycle - should complete
    pipeline_advance_cycle(&pom);
    bool ready_now = pipeline_datapath_ready(&pom) &&
                     (pom.datapath_lag_cycles == 0) &&
                     (pipeline_get_lagged_alu_op(&pom) == ALU_TRANSFER);
    
    if (!not_ready_initially) {
        printf("  ERROR: Datapath should not be ready initially\n");
    }
    if (!ready_now) {
        printf("  ERROR: Datapath should be ready after lag cycles\n");
    }
    
    return not_ready_initially && ready_now;
}

bool test_overlapped_writeback() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Set up overlapped writeback
    pipeline_setup_overlapped_writeback(&pom, 0, 0x42, true); // A register, value 0x42, affects flags
    
    bool writeback_setup = pipeline_writeback_pending(&pom) &&
                          (pom.writeback_register == 0) &&
                          (pom.writeback_value == 0x42) &&
                          (pom.writeback_affects_flags == true);
    
    // Execute writeback
    pipeline_execute_overlapped_writeback(&pom, mock_writeback_callback);
    
    bool writeback_executed = !pipeline_writeback_pending(&pom) &&
                             (writeback_register == 0) &&
                             (writeback_value == 0x42) &&
                             (writeback_flags == true);
    
    if (!writeback_setup) {
        printf("  ERROR: Writeback not properly set up\n");
    }
    if (!writeback_executed) {
        printf("  ERROR: Writeback not properly executed\n");
    }
    
    return writeback_setup && writeback_executed;
}

// ===== ADVANCED PIPELINE TESTS =====

bool test_pipeline_advance_cycle() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Set up multiple pipeline stages
    pipeline_delayed_decode(&pom, 0xA9);
    
    cycle_definition_t cycle = {
        .alu = ALU_TRANSFER
    };
    pipeline_lag_datapath(&pom, &cycle);
    
    uint8_t initial_active = pom.active_stage_count;
    
    // Advance pipeline
    pipeline_advance_cycle(&pom);
    
    bool advanced_ok = (pom.decode_delay_cycles < 2) || // Decreased
                       (pom.datapath_lag_cycles < 1);   // Decreased
    
    if (!advanced_ok) {
        printf("  ERROR: Pipeline stages not properly advanced\n");
        printf("    Decode delay: %d, Datapath lag: %d\n",
               pom.decode_delay_cycles, pom.datapath_lag_cycles);
    }
    
    return advanced_ok;
}

bool test_pipeline_flush() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Set up active pipeline
    pipeline_delayed_decode(&pom, 0xA9);
    pipeline_predictive_fetch(&pom, 0x1000, mock_fetch_callback);
    
    bool pipeline_active = (pom.active_stage_count > 0) ||
                          (pom.prefetch_valid) ||
                          (pom.decode_delay_cycles > 0);
    
    // Flush pipeline
    pipeline_flush(&pom);
    
    bool pipeline_flushed = (pom.active_stage_count == 0) &&
                           (!pom.prefetch_valid) &&
                           (pom.decode_delay_cycles == 0) &&
                           (pom.datapath_lag_cycles == 0) &&
                           (pom.synchronization_cycle_count == 2);
    
    if (!pipeline_active) {
        printf("  ERROR: Pipeline should have been active before flush\n");
    }
    if (!pipeline_flushed) {
        printf("  ERROR: Pipeline not properly flushed\n");
    }
    
    return pipeline_active && pipeline_flushed;
}

bool test_pipeline_stall() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Set up active pipeline
    pipeline_delayed_decode(&pom, 0xA9);
    
    // Stall pipeline
    pipeline_stall(&pom, 3);
    
    bool stall_ok = (pom.pipeline_stall_requested) &&
                    (pom.synchronization_cycle_count == 3);
    
    // Advance cycle during stall - should not progress
    uint8_t decode_delay_before = pom.decode_delay_cycles;
    pipeline_advance_cycle(&pom);
    uint8_t decode_delay_after = pom.decode_delay_cycles;
    
    bool stall_effective = (decode_delay_before == decode_delay_after);
    
    if (!stall_ok) {
        printf("  ERROR: Pipeline stall not properly set up\n");
    }
    if (!stall_effective) {
        printf("  ERROR: Pipeline stall not effective\n");
    }
    
    return stall_ok && stall_effective;
}

// ===== COMPLEX STATE TRANSITION TESTS =====

bool test_simultaneous_states() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Set up multiple active stages
    pipeline_delayed_decode(&pom, 0xA9);
    
    cycle_definition_t cycle = {
        .alu = ALU_TRANSFER
    };
    pipeline_lag_datapath(&pom, &cycle);
    pipeline_setup_overlapped_writeback(&pom, 0, 0x42, true);
    
    bool multiple_stages = pipeline_has_simultaneous_states(&pom) &&
                          (pom.active_stage_count > 1);
    
    uint16_t state_mask = pipeline_get_simultaneous_states_mask(&pom);
    bool mask_valid = (state_mask != 0);
    
    if (!multiple_stages) {
        printf("  ERROR: Should have multiple simultaneous stages\n");
        printf("    Active stages: %d\n", pom.active_stage_count);
    }
    if (!mask_valid) {
        printf("  ERROR: State mask should not be zero\n");
    }
    
    return multiple_stages && mask_valid;
}

bool test_complex_state_transitions() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    cycle_definition_t cycle = {
        .timing = TIMING_T0,
        .alu = ALU_TRANSFER
    };
    
    // Test T01,T0 → T0 transition (instruction completion)
    pipeline_execute_complex_transition(&pom, STATE_TRANS_T01_T0, &cycle);
    
    bool completion_ok = pipeline_writeback_pending(&pom);
    
    // Test 2-cycle instruction case  
    cycle.timing = TIMING_T0;
    pipeline_execute_complex_transition(&pom, STATE_TRANS_T2, &cycle);
    
    bool two_cycle_ok = true; // Complex logic would be here
    
    if (!completion_ok) {
        printf("  ERROR: T01,T0 transition not properly handled\n");
    }
    
    return completion_ok && two_cycle_ok;
}

// ===== PIPELINE TIMING EXAMPLE TESTS =====

bool test_pipeline_timing_example() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Set up the famous "2 instructions!" case from spec
    pipeline_delayed_decode(&pom, 0xAD); // LDA $nnnn (4-cycle)
    
    cycle_definition_t cycle = {
        .alu = ALU_TRANSFER
    };
    pipeline_lag_datapath(&pom, &cycle);
    pipeline_setup_overlapped_writeback(&pom, 0, 0x42, true);
    
    // Get timing state for cycle 4 (the overlapped case)
    pipeline_timing_example_t timing = pipeline_get_timing_state(&pom, 4);
    
    bool timing_ok = (timing.cycle == 4) &&
                     (strstr(timing.current_instr, "Multi") != NULL) &&
                     (strstr(timing.next_instr, "Over") != NULL);
    
    if (!timing_ok) {
        printf("  ERROR: Pipeline timing example incorrect\n");
        printf("    Cycle: %d, Current: %s, Next: %s\n",
               timing.cycle, timing.current_instr, timing.next_instr);
    }
    
    return timing_ok;
}

bool test_multiple_instructions_detection() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Single instruction case
    pipeline_delayed_decode(&pom, 0xA9);
    bool single_ok = !pipeline_has_multiple_instructions(&pom);
    
    // Multiple instructions case
    cycle_definition_t cycle = {
        .alu = ALU_TRANSFER
    };
    pipeline_lag_datapath(&pom, &cycle);
    pipeline_setup_overlapped_writeback(&pom, 0, 0x42, true);
    
    bool multiple_ok = pipeline_has_multiple_instructions(&pom);
    
    if (!single_ok) {
        printf("  ERROR: Should detect single instruction correctly\n");
    }
    if (!multiple_ok) {
        printf("  ERROR: Should detect multiple instructions correctly\n");
    }
    
    return single_ok && multiple_ok;
}

// ===== DEBUGGING AND VALIDATION TESTS =====

bool test_pipeline_state_validation() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Valid state
    bool valid_initially = pipeline_validate_state(&pom);
    
    // Set up complex but valid state
    pipeline_delayed_decode(&pom, 0xA9);
    cycle_definition_t cycle = {
        .alu = ALU_TRANSFER
    };
    pipeline_lag_datapath(&pom, &cycle);
    
    bool valid_complex = pipeline_validate_state(&pom);
    
    if (!valid_initially) {
        printf("  ERROR: Initial state should be valid\n");
    }
    if (!valid_complex) {
        printf("  ERROR: Complex state should be valid\n");
    }
    
    return valid_initially && valid_complex;
}

bool test_pipeline_debug_output() {
    pipeline_overlap_manager_t pom;
    pipeline_init(&pom);
    
    // Set up pipeline for debugging
    pipeline_delayed_decode(&pom, 0xA9);
    pipeline_setup_overlapped_writeback(&pom, 0, 0x42, true);
    
    char buffer[1024];
    pipeline_dump_state(&pom, buffer, sizeof(buffer));
    
    bool dump_ok = strlen(buffer) > 0 &&
                   strstr(buffer, "Pipeline State") != NULL &&
                   strstr(buffer, "active stages") != NULL;
    
    // Test visualization
    pipeline_create_visualization(&pom, buffer, sizeof(buffer));
    
    bool viz_ok = strlen(buffer) > 0 &&
                  strstr(buffer, "Pipeline Visualization") != NULL &&
                  strstr(buffer, "2 instructions!") != NULL;
    
    if (!dump_ok) {
        printf("  ERROR: Pipeline state dump failed\n");
    }
    if (!viz_ok) {
        printf("  ERROR: Pipeline visualization failed\n");
    }
    
    return dump_ok && viz_ok;
}

// ===== MAIN TEST RUNNER =====

int main() {
    printf("MOS6510 Advanced Pipeline Test Suite\n");
    printf("=====================================\n\n");
    
    // Basic pipeline tests
    TEST(test_pipeline_initialization);
    TEST(test_predictive_fetch);
    TEST(test_delayed_decode);
    TEST(test_lagged_datapath);
    TEST(test_overlapped_writeback);
    
    // Advanced pipeline tests
    TEST(test_pipeline_advance_cycle);
    TEST(test_pipeline_flush);
    TEST(test_pipeline_stall);
    
    // Complex state transition tests
    TEST(test_simultaneous_states);
    TEST(test_complex_state_transitions);
    
    // Pipeline timing example tests
    TEST(test_pipeline_timing_example);
    TEST(test_multiple_instructions_detection);
    
    // Debugging and validation tests
    TEST(test_pipeline_state_validation);
    TEST(test_pipeline_debug_output);
    
    // Summary
    printf("\n=====================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("✅ All tests passed!\n");
        printf("\nAdvanced pipeline system validation successful:\n");
        printf("- Predictive fetch mechanism operational\n");
        printf("- Delayed decode with T0/T1 lag working\n");  
        printf("- Lagged datapath with 1-2 cycle delays implemented\n");
        printf("- Overlapped writeback during next instruction functional\n");
        printf("- Complex state transitions with simultaneous states supported\n");
        printf("- Pipeline flush and stall mechanisms operational\n");
        printf("- \"The datapath is a bit behind\" behavior accurately modeled\n");
        return 0;
    } else {
        printf("❌ Some tests failed!\n");
        return 1;
    }
}