/*
 * Test Suite: MOS6510 Core Tick Function
 * 
 * Tests Phase 5.2: Complete cycle-accurate CPU tick function implementation
 * Based on visual6502 timing requirements and hardware specifications
 * 
 * Key Test Areas:
 * - φ1/φ2 phase execution coordination
 * - Main tick function integration with all subsystems
 * - Timing state advancement and pipeline coordination
 * - PLA decode and instruction execution flow
 * - Interrupt processing integration
 * - Address setup as final operation
 * - Complete CPU cycle execution validation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "mos6510_tick.h"
#include "mos6510_state.h"
#include "mos6510_registers.h"
#include "deferred_ops.h"
#include "interrupt_recognition.h"
#include "nmi_skipping.h"
#include "pipeline.h"
#include "timing_states.h"
#include "pla_lookup.h"
#include "instruction_table.h"
#include "internal_bus.h"

// Test configuration
#define MAX_TEST_NAME_LEN 128
#define MAX_ERROR_MSG_LEN 256
#define TEST_BUFFER_SIZE 512

// Test statistics
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Test result reporting
typedef struct {
    bool passed;
    char test_name[MAX_TEST_NAME_LEN];
    char error_message[MAX_ERROR_MSG_LEN];
} test_result_t;

// Helper macros for test validation
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            snprintf(result->error_message, MAX_ERROR_MSG_LEN, \
                    "ASSERTION FAILED: %s at line %d", message, __LINE__); \
            result->passed = false; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            snprintf(result->error_message, MAX_ERROR_MSG_LEN, \
                    "ASSERTION FAILED: %s - Expected: %d, Actual: %d at line %d", \
                    message, (int)(expected), (int)(actual), __LINE__); \
            result->passed = false; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_NEQ(not_expected, actual, message) \
    do { \
        if ((not_expected) == (actual)) { \
            snprintf(result->error_message, MAX_ERROR_MSG_LEN, \
                    "ASSERTION FAILED: %s - Should not equal: %d at line %d", \
                    message, (int)(not_expected), __LINE__); \
            result->passed = false; \
            return; \
        } \
    } while (0)

// Test helper: Create minimal CPU state for testing
static mos6510_state_t* create_test_cpu_state(void) {
    mos6510_state_t *cpu = malloc(sizeof(mos6510_state_t));
    if (!cpu) return NULL;
    
    memset(cpu, 0, sizeof(mos6510_state_t));
    
    // Initialize internal bus
    internal_bus_init(&cpu->internal_bus);
    
    // Set initial register values for testing
    cpu->registers[REG_PCL] = 0x00;
    cpu->registers[REG_PCH] = 0x80;  // Start at $8000
    cpu->registers[REG_A] = 0x00;
    cpu->registers[REG_X] = 0x00;
    cpu->registers[REG_Y] = 0x00;
    cpu->registers[REG_SP] = 0xFF;
    cpu->registers[REG_P] = FLAG_UNUSED | FLAG_I;  // Default processor state
    
    // Initialize timing state
    cpu->timing_state = TIMING_T1F;
    cpu->instruction_register = 0xEA;  // NOP for testing
    cpu->cycle_position = 0;
    
    return cpu;
}

static void destroy_test_cpu_state(mos6510_state_t *cpu) {
    if (cpu) {
        free(cpu);
    }
}

// Test helper: Initialize tick context for testing
static tick_context_t* create_test_tick_context(void) {
    tick_context_t *context = malloc(sizeof(tick_context_t));
    if (!context) return NULL;
    
    mos6510_tick_context_init(context, false, true);  // No debug, enable validation
    return context;
}

static void destroy_test_tick_context(tick_context_t *context) {
    if (context) {
        free(context);
    }
}

// ===== TICK CONTEXT TESTS =====

static void test_tick_context_initialization(test_result_t *result) {
    strcpy(result->test_name, "Tick Context Initialization");
    result->passed = true;
    
    tick_context_t *context = create_test_tick_context();
    TEST_ASSERT(context != NULL, "Context creation failed");
    
    // Verify initial state
    TEST_ASSERT_EQ(TICK_PHASE_PHI2, context->current_phase, "Should start in φ2 phase");
    TEST_ASSERT_EQ(TICK_PHASE_PHI1, context->next_phase, "Next phase should be φ1");
    TEST_ASSERT_EQ(false, context->debug_enabled, "Debug should be disabled");
    TEST_ASSERT_EQ(true, context->validation_enabled, "Validation should be enabled");
    TEST_ASSERT_EQ(0, context->current_cycle, "Should start at cycle 0");
    TEST_ASSERT_EQ(0, context->statistics.total_ticks, "Should have no ticks initially");
    
    destroy_test_tick_context(context);
}

static void test_tick_context_reset(test_result_t *result) {
    strcpy(result->test_name, "Tick Context Reset");
    result->passed = true;
    
    tick_context_t *context = create_test_tick_context();
    TEST_ASSERT(context != NULL, "Context creation failed");
    
    // Modify context state
    context->current_cycle = 100;
    context->statistics.total_ticks = 50;
    context->error_count = 5;
    
    // Reset context
    mos6510_tick_context_reset(context);
    
    // Verify reset state
    TEST_ASSERT_EQ(TICK_PHASE_PHI2, context->current_phase, "Should reset to φ2 phase");
    TEST_ASSERT_EQ(TICK_PHASE_PHI1, context->next_phase, "Next phase should be φ1");
    TEST_ASSERT_EQ(0, context->current_cycle, "Cycle should reset to 0");
    TEST_ASSERT_EQ(50, context->statistics.total_ticks, "Statistics should be preserved");
    TEST_ASSERT_EQ(0, context->error_count, "Error count should reset");
    
    destroy_test_tick_context(context);
}

// ===== MAIN TICK FUNCTION TESTS =====

static void test_main_tick_function_basic(test_result_t *result) {
    strcpy(result->test_name, "Main Tick Function Basic Operation");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    tick_context_t *context = create_test_tick_context();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(context != NULL, "Tick context creation failed");
    
    // Execute one tick starting from φ2 phase
    TEST_ASSERT_EQ(TICK_PHASE_PHI2, context->current_phase, "Should start in φ2 phase");
    
    bool tick_success = mos6510_tick(cpu, context);
    TEST_ASSERT(tick_success, "Tick execution should succeed");
    
    // Verify phase transition
    TEST_ASSERT_EQ(TICK_PHASE_PHI1, context->current_phase, "Should advance to φ1 phase");
    TEST_ASSERT_EQ(1, context->statistics.total_ticks, "Should have executed 1 tick");
    TEST_ASSERT_EQ(1, cpu->total_cycles, "CPU cycle count should increment");
    TEST_ASSERT_EQ(true, context->cycle_complete, "Cycle should be marked as complete");
    
    destroy_test_cpu_state(cpu);
    destroy_test_tick_context(context);
}

static void test_phi1_phi2_phase_alternation(test_result_t *result) {
    strcpy(result->test_name, "φ1/φ2 Phase Alternation");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    tick_context_t *context = create_test_tick_context();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(context != NULL, "Tick context creation failed");
    
    // Execute multiple ticks to verify phase alternation
    for (int i = 0; i < 4; i++) {
        tick_phase_t expected_phase = (i % 2 == 0) ? TICK_PHASE_PHI2 : TICK_PHASE_PHI1;
        TEST_ASSERT_EQ(expected_phase, context->current_phase, "Phase should alternate correctly");
        
        bool tick_success = mos6510_tick(cpu, context);
        TEST_ASSERT(tick_success, "Tick execution should succeed");
    }
    
    TEST_ASSERT_EQ(4, context->statistics.total_ticks, "Should have executed 4 ticks");
    TEST_ASSERT_EQ(4, cpu->total_cycles, "CPU cycle count should match tick count");
    
    destroy_test_cpu_state(cpu);
    destroy_test_tick_context(context);
}

// ===== PHASE-SPECIFIC EXECUTION TESTS =====

static void test_phi1_phase_execution(test_result_t *result) {
    strcpy(result->test_name, "φ1 Phase Execution");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    tick_context_t *context = create_test_tick_context();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(context != NULL, "Tick context creation failed");
    
    // Start in φ2 phase, then advance to φ1
    bool tick_success = mos6510_tick(cpu, context);
    TEST_ASSERT(tick_success, "First tick should succeed");
    TEST_ASSERT_EQ(TICK_PHASE_PHI1, context->current_phase, "Should be in φ1 phase");
    
    // Execute φ1 phase
    tick_success = mos6510_tick(cpu, context);
    TEST_ASSERT(tick_success, "φ1 phase tick should succeed");
    
    // Verify φ1 phase operations were executed
    TEST_ASSERT_EQ(true, context->deferred_ops_executed, "φ1 deferred operations should be executed");
    TEST_ASSERT_EQ(TICK_PHASE_PHI2, context->current_phase, "Should advance to φ2 phase");
    
    destroy_test_cpu_state(cpu);
    destroy_test_tick_context(context);
}

static void test_phi2_phase_execution(test_result_t *result) {
    strcpy(result->test_name, "φ2 Phase Execution");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    tick_context_t *context = create_test_tick_context();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(context != NULL, "Tick context creation failed");
    
    // Execute φ2 phase
    TEST_ASSERT_EQ(TICK_PHASE_PHI2, context->current_phase, "Should start in φ2 phase");
    
    bool tick_success = mos6510_tick(cpu, context);
    TEST_ASSERT(tick_success, "φ2 phase tick should succeed");
    
    // Verify φ2 phase operations were executed
    TEST_ASSERT_EQ(true, context->interrupts_processed, "φ2 interrupt processing should be completed");
    TEST_ASSERT_EQ(true, context->timing_advanced, "φ2 timing advancement should be completed");
    TEST_ASSERT_EQ(true, context->pla_decoded, "φ2 PLA decode should be completed");
    TEST_ASSERT_EQ(true, context->address_setup_completed, "φ2 address setup should be completed");
    
    destroy_test_cpu_state(cpu);
    destroy_test_tick_context(context);
}

// ===== TIMING STATE ADVANCEMENT TESTS =====

static void test_timing_state_advancement(test_result_t *result) {
    strcpy(result->test_name, "Timing State Advancement");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    tick_context_t *context = create_test_tick_context();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(context != NULL, "Tick context creation failed");
    
    // Set initial timing state
    cpu->timing_state = TIMING_T1F;
    cpu->cycle_position = 0;
    
    // Execute φ2 phase to trigger timing advancement
    bool tick_success = mos6510_tick(cpu, context);
    TEST_ASSERT(tick_success, "Tick should succeed");
    TEST_ASSERT_EQ(true, context->timing_advanced, "Timing should be advanced");
    
    // Verify timing state progression
    TEST_ASSERT_EQ(TIMING_T2, cpu->timing_state, "Should advance from T1F to T2");
    TEST_ASSERT_EQ(1, cpu->cycle_position, "Cycle position should increment");
    
    destroy_test_cpu_state(cpu);
    destroy_test_tick_context(context);
}

// ===== PLA DECODE TESTS =====

static void test_pla_decode_integration(test_result_t *result) {
    strcpy(result->test_name, "PLA Decode Integration");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    tick_context_t *context = create_test_tick_context();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(context != NULL, "Tick context creation failed");
    
    // Set test opcode
    cpu->instruction_register = 0xEA;  // NOP
    
    // Execute φ2 phase to trigger PLA decode
    bool tick_success = mos6510_tick(cpu, context);
    TEST_ASSERT(tick_success, "Tick should succeed");
    TEST_ASSERT_EQ(true, context->pla_decoded, "PLA decode should be completed");
    
    // Verify PLA decode was performed
    const instruction_definition_t* instruction = pla_lookup(0xEA);
    TEST_ASSERT(instruction != NULL, "PLA lookup should return valid instruction");
    
    destroy_test_cpu_state(cpu);
    destroy_test_tick_context(context);
}

// ===== INTEGRATION TESTS =====

static void test_complete_tick_cycle_integration(test_result_t *result) {
    strcpy(result->test_name, "Complete Tick Cycle Integration");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    tick_context_t *context = create_test_tick_context();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(context != NULL, "Tick context creation failed");
    
    // Execute a complete φ2 -> φ1 -> φ2 cycle
    uint64_t initial_cycles = cpu->total_cycles;
    
    // φ2 Phase execution
    TEST_ASSERT_EQ(TICK_PHASE_PHI2, context->current_phase, "Should start in φ2");
    bool tick_success = mos6510_tick(cpu, context);
    TEST_ASSERT(tick_success, "φ2 tick should succeed");
    
    TEST_ASSERT_EQ(TICK_PHASE_PHI1, context->current_phase, "Should advance to φ1");
    TEST_ASSERT_EQ(true, context->interrupts_processed, "φ2 interrupt processing complete");
    TEST_ASSERT_EQ(true, context->address_setup_completed, "φ2 address setup complete");
    
    // φ1 Phase execution
    tick_success = mos6510_tick(cpu, context);
    TEST_ASSERT(tick_success, "φ1 tick should succeed");
    
    TEST_ASSERT_EQ(TICK_PHASE_PHI2, context->current_phase, "Should return to φ2");
    TEST_ASSERT_EQ(true, context->deferred_ops_executed, "φ1 deferred operations complete");
    
    // Verify complete cycle
    TEST_ASSERT_EQ(initial_cycles + 2, cpu->total_cycles, "Should have executed 2 cycles");
    TEST_ASSERT_EQ(2, context->statistics.total_ticks, "Should have 2 total ticks");
    
    destroy_test_cpu_state(cpu);
    destroy_test_tick_context(context);
}

static void test_subsystem_coordination(test_result_t *result) {
    strcpy(result->test_name, "Subsystem Coordination");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    tick_context_t *context = create_test_tick_context();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(context != NULL, "Tick context creation failed");
    
    // Execute multiple ticks to verify subsystem coordination
    for (int i = 0; i < 10; i++) {
        bool tick_success = mos6510_tick(cpu, context);
        TEST_ASSERT(tick_success, "Each tick should succeed");
        TEST_ASSERT_EQ(0, context->error_count, "No errors should occur");
        
        // Verify subsystem operations based on phase
        if (context->current_phase == TICK_PHASE_PHI1) {
            // Previous tick was φ1 - verify φ1 operations
            TEST_ASSERT_EQ(true, context->deferred_ops_executed, "φ1 operations should execute");
        } else {
            // Previous tick was φ2 - verify φ2 operations  
            TEST_ASSERT_EQ(true, context->interrupts_processed, "φ2 interrupt processing should execute");
            TEST_ASSERT_EQ(true, context->address_setup_completed, "φ2 address setup should execute");
        }
    }
    
    TEST_ASSERT_EQ(10, context->statistics.total_ticks, "Should execute all 10 ticks");
    TEST_ASSERT_EQ(10, cpu->total_cycles, "CPU cycles should match");
    
    destroy_test_cpu_state(cpu);
    destroy_test_tick_context(context);
}

// ===== ERROR HANDLING TESTS =====

static void test_error_handling(test_result_t *result) {
    strcpy(result->test_name, "Error Handling");
    result->passed = true;
    
    tick_context_t *context = create_test_tick_context();
    TEST_ASSERT(context != NULL, "Context creation failed");
    
    // Test with NULL CPU (should fail gracefully)
    bool tick_success = mos6510_tick(NULL, context);
    TEST_ASSERT_EQ(false, tick_success, "Tick with NULL CPU should fail");
    
    // Test with NULL context (should fail gracefully)
    mos6510_state_t *cpu = create_test_cpu_state();
    TEST_ASSERT(cpu != NULL, "CPU creation failed");
    
    tick_success = mos6510_tick(cpu, NULL);
    TEST_ASSERT_EQ(false, tick_success, "Tick with NULL context should fail");
    
    destroy_test_cpu_state(cpu);
    destroy_test_tick_context(context);
}

// ===== VALIDATION TESTS =====

static void test_state_validation(test_result_t *result) {
    strcpy(result->test_name, "State Validation");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    tick_context_t *context = create_test_tick_context();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(context != NULL, "Tick context creation failed");
    
    // Enable validation
    context->validation_enabled = true;
    
    // Execute tick with validation enabled
    bool tick_success = mos6510_tick(cpu, context);
    TEST_ASSERT(tick_success, "Tick with validation should succeed");
    TEST_ASSERT_EQ(0, context->error_count, "Validation should pass without errors");
    
    destroy_test_cpu_state(cpu);
    destroy_test_tick_context(context);
}

// ===== TEST EXECUTION FRAMEWORK =====

static void run_test(void (*test_func)(test_result_t *), test_result_t *result) {
    tests_run++;
    result->passed = true;
    result->error_message[0] = '\0';
    
    test_func(result);
    
    if (result->passed) {
        tests_passed++;
        printf("✓ PASS: %s\n", result->test_name);
    } else {
        tests_failed++;
        printf("✗ FAIL: %s\n", result->test_name);
        printf("  Error: %s\n", result->error_message);
    }
}

static void print_test_summary(void) {
    printf("\n" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    printf("MOS6510 CORE TICK FUNCTION TEST SUMMARY\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    printf("Tests Run:    %d\n", tests_run);
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n🎉 ALL TESTS PASSED! Core tick function is working correctly.\n");
        printf("✓ Tick context management functional\n");
        printf("✓ Main tick function integration successful\n");
        printf("✓ φ1/φ2 phase alternation working correctly\n");
        printf("✓ Phase-specific execution validated\n");
        printf("✓ Timing state advancement implemented\n");
        printf("✓ PLA decode integration functional\n");
        printf("✓ Complete cycle integration verified\n");
        printf("✓ Subsystem coordination working\n");
        printf("✓ Error handling robust\n");
        printf("✓ State validation implemented\n");
    } else {
        printf("\n❌ %d test(s) failed. Review implementation.\n", tests_failed);
    }
    
    double success_rate = (double)tests_passed / tests_run * 100.0;
    printf("\nSuccess Rate: %.1f%%\n", success_rate);
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
}

// ===== MAIN TEST RUNNER =====

int main(void) {
    printf("MOS6510 Core Tick Function Test Suite\n");
    printf("Phase 5.2: Complete cycle-accurate CPU tick function\n");
    printf("Testing hardware-accurate timing and subsystem coordination\n\n");
    
    test_result_t result;
    
    // Tick context tests
    run_test(test_tick_context_initialization, &result);
    run_test(test_tick_context_reset, &result);
    
    // Main tick function tests
    run_test(test_main_tick_function_basic, &result);
    run_test(test_phi1_phi2_phase_alternation, &result);
    
    // Phase-specific execution tests
    run_test(test_phi1_phase_execution, &result);
    run_test(test_phi2_phase_execution, &result);
    
    // Subsystem coordination tests
    run_test(test_timing_state_advancement, &result);
    run_test(test_pla_decode_integration, &result);
    
    // Integration tests
    run_test(test_complete_tick_cycle_integration, &result);
    run_test(test_subsystem_coordination, &result);
    
    // Error handling and validation tests
    run_test(test_error_handling, &result);
    run_test(test_state_validation, &result);
    
    print_test_summary();
    
    return (tests_failed == 0) ? 0 : 1;
}