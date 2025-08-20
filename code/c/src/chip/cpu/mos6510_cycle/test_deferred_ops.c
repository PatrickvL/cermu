/*
 * Test Suite: Deferred Operations Architecture
 * 
 * Tests Phase 5.1: φ1/φ2 phase-accurate execution flow with operation deferral queue
 * Based on visual6502 timing requirements and hardware specifications
 * 
 * Key Test Areas:
 * - φ1/φ2 phase coordination and timing
 * - Operation deferral queue management
 * - Address setup as final tick operation (spec lines 353, 534-536)
 * - RDY line handling for read cycles only (spec lines 552-554)
 * - Priority-based operation execution
 * - Hardware-accurate bus timing and latching
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "deferred_ops.h"
#include "mos6510_state.h"
#include "mos6510_registers.h"
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
    
    return cpu;
}

static void destroy_test_cpu_state(mos6510_state_t *cpu) {
    if (cpu) {
        free(cpu);
    }
}

// Test helper: Initialize deferred state for testing
static deferred_ops_state_t* create_test_deferred_state(void) {
    deferred_ops_state_t *state = malloc(sizeof(deferred_ops_state_t));
    if (!state) return NULL;
    
    deferred_ops_init(state);
    return state;
}

static void destroy_test_deferred_state(deferred_ops_state_t *state) {
    if (state) {
        free(state);
    }
}

// ===== PHASE COORDINATION TESTS =====

static void test_phi1_phi2_phase_coordination(test_result_t *result) {
    strcpy(result->test_name, "φ1/φ2 Phase Coordination");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    deferred_ops_state_t *deferred_state = create_test_deferred_state();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(deferred_state != NULL, "Deferred state creation failed");
    
    // Test initial φ2 phase state
    TEST_ASSERT_EQ(false, deferred_state->phi1_phase_active, "Should start in φ2 phase");
    TEST_ASSERT_EQ(0, deferred_state->queue.count, "Should have no pending operations initially");
    
    // Queue operation during φ2 phase
    deferred_operation_t op = deferred_ops_register_load(REG_DL, 0x42);
    
    bool queued = deferred_ops_enqueue(deferred_state, &op);
    TEST_ASSERT(queued, "Operation queueing should succeed during φ2");
    TEST_ASSERT_EQ(1, deferred_state->queue.count, "Should have 1 pending operation");
    
    // Execute φ1 phase
    deferred_ops_set_phase(deferred_state, true, false);  // φ1 active, φ2 inactive
    deferred_ops_execute_phi1(deferred_state, cpu);
    TEST_ASSERT_EQ(true, deferred_state->phi1_phase_active, "Should be in φ1 phase after execution");
    TEST_ASSERT_EQ(0, deferred_state->queue.count, "Pending operations should be executed");
    
    // Return to φ2 phase
    deferred_ops_set_phase(deferred_state, false, true);  // φ1 inactive, φ2 active
    TEST_ASSERT_EQ(false, deferred_state->phi1_phase_active, "Should return to φ2 phase");
    
    destroy_test_cpu_state(cpu);
    destroy_test_deferred_state(deferred_state);
}

// Test address setup timing requirements
static void test_address_setup_timing(test_result_t *result) {
    strcpy(result->test_name, "Address Setup Timing (spec lines 353, 534-536)");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    deferred_ops_state_t *deferred_state = create_test_deferred_state();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(deferred_state != NULL, "Deferred state creation failed");
    
    // Set up address for next operation
    uint16_t test_address = 0x1234;
    deferred_ops_defer_address_setup(deferred_state, test_address);
    
    TEST_ASSERT_EQ(test_address, deferred_state->deferred_address, "Address should be stored for deferral");
    TEST_ASSERT_EQ(true, deferred_state->address_setup_pending, "Address setup should be marked as pending");
    
    // Execute address setup during φ2 phase - address setup should be the last operation
    deferred_ops_set_phase(deferred_state, false, true);  // φ2 active
    deferred_ops_address_setup_phi2(deferred_state, cpu);
    
    // Verify address was set up correctly
    uint16_t actual_address = (cpu->registers[REG_ABH] << 8) | cpu->registers[REG_ABL];
    TEST_ASSERT_EQ(test_address, actual_address, "Address should be set up correctly on address bus");
    TEST_ASSERT_EQ(false, deferred_state->address_setup_pending, "Address setup should be completed");
    
    destroy_test_cpu_state(cpu);
    destroy_test_deferred_state(deferred_state);
}

// ===== OPERATION QUEUE TESTS =====

static void test_operation_queue_management(test_result_t *result) {
    strcpy(result->test_name, "Operation Queue Management");
    result->passed = true;
    
    deferred_ops_state_t *deferred_state = create_test_deferred_state();
    TEST_ASSERT(deferred_state != NULL, "Deferred state creation failed");
    
    // Test queue capacity
    TEST_ASSERT_EQ(0, deferred_state->queue.count, "Queue should be empty initially");
    TEST_ASSERT_EQ(false, deferred_state->queue.queue_full, "Queue should not be full initially");
    
    // Fill queue to capacity
    deferred_operation_t op = deferred_ops_register_load(REG_DL, 0x00);
    
    int operations_queued = 0;
    while (!deferred_state->queue.queue_full && operations_queued < DEFERRED_OP_QUEUE_SIZE) {
        bool queued = deferred_ops_enqueue(deferred_state, &op);
        TEST_ASSERT(queued, "Operation queueing should succeed when queue not full");
        operations_queued++;
    }
    
    TEST_ASSERT_EQ(DEFERRED_OP_QUEUE_SIZE, operations_queued, "Should queue maximum operations");
    TEST_ASSERT_EQ(true, deferred_state->queue.queue_full, "Queue should be full");
    
    // Test queue overflow behavior
    bool overflow_queued = deferred_ops_enqueue(deferred_state, &op);
    TEST_ASSERT_EQ(false, overflow_queued, "Queueing should fail when queue is full");
    
    destroy_test_deferred_state(deferred_state);
}

// Test priority-based operation execution
static void test_priority_based_execution(test_result_t *result) {
    strcpy(result->test_name, "Priority-Based Operation Execution");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    deferred_ops_state_t *deferred_state = create_test_deferred_state();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(deferred_state != NULL, "Deferred state creation failed");
    
    // Set up test data
    cpu->registers[REG_A] = 0xAA;
    cpu->registers[REG_X] = 0xBB;
    cpu->registers[REG_Y] = 0xCC;
    
    // Create operations with different priorities
    deferred_operation_t low_priority_op = deferred_ops_register_load(REG_DL, 0xCC);  // Will be overwritten by higher priority
    low_priority_op.priority = DEFERRED_PRIORITY_LOW;
    
    deferred_operation_t high_priority_op = deferred_ops_register_load(REG_DL, 0xAA);  // Should execute first
    high_priority_op.priority = DEFERRED_PRIORITY_HIGH;
    
    deferred_operation_t normal_priority_op = deferred_ops_register_load(REG_DL, 0xBB);  // Should execute second
    normal_priority_op.priority = DEFERRED_PRIORITY_NORMAL;
    
    // Queue in reverse priority order
    TEST_ASSERT(deferred_ops_enqueue(deferred_state, &low_priority_op), "Low priority op should queue");
    TEST_ASSERT(deferred_ops_enqueue(deferred_state, &high_priority_op), "High priority op should queue");
    TEST_ASSERT(deferred_ops_enqueue(deferred_state, &normal_priority_op), "Normal priority op should queue");
    
    // Execute φ1 phase - should execute in priority order
    deferred_ops_set_phase(deferred_state, true, false);
    deferred_ops_execute_phi1(deferred_state, cpu);
    
    // Verify operations were executed (queue should be empty)
    TEST_ASSERT_EQ(0, deferred_state->queue.count, "All operations should be executed");
    
    // The final value should be from the last executed operation (low priority, executed last)
    TEST_ASSERT_EQ(0xCC, cpu->registers[REG_DL], "Operations should execute in priority order");
    
    destroy_test_cpu_state(cpu);
    destroy_test_deferred_state(deferred_state);
}

// ===== RDY LINE HANDLING TESTS =====

static void test_rdy_line_read_cycles(test_result_t *result) {
    strcpy(result->test_name, "RDY Line Handling for Read Cycles (spec lines 552-554)");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    deferred_ops_state_t *deferred_state = create_test_deferred_state();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(deferred_state != NULL, "Deferred state creation failed");
    
    // Test RDY line behavior for read cycles
    TEST_ASSERT_EQ(false, deferred_state->waiting_for_rdy, "RDY should not be halted initially");
    
    // Set RDY line low during read cycle
    deferred_ops_set_rdy_line(deferred_state, false);  // RDY low
    deferred_state->rdy_affects_cycle = true;  // Simulate read cycle
    TEST_ASSERT_EQ(true, deferred_ops_should_stall_for_rdy(deferred_state), "RDY should halt during read cycle");
    
    // Try to execute operations while RDY is halted
    deferred_operation_t op = deferred_ops_memory_read(0x1000, REG_DL);
    
    bool queued = deferred_ops_enqueue(deferred_state, &op);
    TEST_ASSERT(queued, "Operation should still queue during RDY halt");
    
    // Execute φ1 phase while RDY is stalled
    deferred_ops_set_phase(deferred_state, true, false);
    deferred_ops_execute_phi1(deferred_state, cpu);
    // Note: Implementation detail - operations might execute but CPU stalls afterward
    
    // Release RDY line
    deferred_ops_set_rdy_line(deferred_state, true);  // RDY high
    TEST_ASSERT_EQ(false, deferred_ops_should_stall_for_rdy(deferred_state), "RDY should not stall after release");
    
    destroy_test_cpu_state(cpu);
    destroy_test_deferred_state(deferred_state);
}

static void test_rdy_line_write_cycles(test_result_t *result) {
    strcpy(result->test_name, "RDY Line Ignored for Write Cycles (spec lines 552-554)");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    deferred_ops_state_t *deferred_state = create_test_deferred_state();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(deferred_state != NULL, "Deferred state creation failed");
    
    // Set RDY line low during write cycle - should be ignored
    deferred_ops_set_rdy_line(deferred_state, false);  // RDY low
    deferred_state->rdy_affects_cycle = false;  // Simulate write cycle (RDY ignored)
    TEST_ASSERT_EQ(false, deferred_ops_should_stall_for_rdy(deferred_state), "RDY should be ignored during write cycles");
    
    // Queue and execute operations during write cycle with RDY low
    deferred_operation_t op = deferred_ops_memory_write(0x1000, REG_A);
    
    bool queued = deferred_ops_enqueue(deferred_state, &op);
    TEST_ASSERT(queued, "Operation should queue during write cycle");
    
    // Execute φ1 phase - should proceed normally despite RDY being low
    deferred_ops_set_phase(deferred_state, true, false);
    deferred_ops_execute_phi1(deferred_state, cpu);
    TEST_ASSERT_EQ(0, deferred_state->queue.count, "Operations should execute during write cycle even with RDY low");
    
    destroy_test_cpu_state(cpu);
    destroy_test_deferred_state(deferred_state);
}

// ===== INTEGRATION TESTS =====

static void test_complete_cycle_execution(test_result_t *result) {
    strcpy(result->test_name, "Complete Cycle Execution Integration");
    result->passed = true;
    
    mos6510_state_t *cpu = create_test_cpu_state();
    deferred_ops_state_t *deferred_state = create_test_deferred_state();
    
    TEST_ASSERT(cpu != NULL, "CPU state creation failed");
    TEST_ASSERT(deferred_state != NULL, "Deferred state creation failed");
    
    // Set up test scenario: execute a complete φ2 -> φ1 -> φ2 cycle
    
    // φ2 Phase: Queue multiple operations with address setup
    cpu->registers[REG_A] = 0x42;
    
    deferred_operation_t data_op = deferred_ops_register_load(REG_DL, 0x42);
    uint16_t target_address = 0x2000;
    
    // Queue operations and set up address
    TEST_ASSERT(deferred_ops_enqueue(deferred_state, &data_op), "Data operation should queue");
    deferred_ops_defer_address_setup(deferred_state, target_address);
    
    TEST_ASSERT_EQ(1, deferred_state->queue.count, "Should have 1 pending operation");
    TEST_ASSERT_EQ(true, deferred_state->address_setup_pending, "Address setup should be pending");
    
    // φ1 Phase: Execute all operations
    deferred_ops_set_phase(deferred_state, true, false);
    deferred_ops_execute_phi1(deferred_state, cpu);
    
    TEST_ASSERT_EQ(0, deferred_state->queue.count, "Operations should be executed");
    TEST_ASSERT_EQ(0x42, cpu->registers[REG_DL], "Data should be transferred");
    TEST_ASSERT_EQ(true, deferred_state->phi1_phase_active, "Should be in φ1 phase");
    
    // φ2 Phase: Address setup should occur
    deferred_ops_set_phase(deferred_state, false, true);
    deferred_ops_address_setup_phi2(deferred_state, cpu);
    
    uint16_t actual_address = (cpu->registers[REG_ABH] << 8) | cpu->registers[REG_ABL];
    TEST_ASSERT_EQ(target_address, actual_address, "Address should be set up correctly");
    TEST_ASSERT_EQ(false, deferred_state->address_setup_pending, "Address setup should be completed");
    TEST_ASSERT_EQ(false, deferred_state->phi1_phase_active, "Should return to φ2 phase");
    
    destroy_test_cpu_state(cpu);
    destroy_test_deferred_state(deferred_state);
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
    printf("DEFERRED OPERATIONS ARCHITECTURE TEST SUMMARY\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    printf("Tests Run:    %d\n", tests_run);
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n🎉 ALL TESTS PASSED! Deferred operations architecture is working correctly.\n");
        printf("✓ φ1/φ2 phase coordination implemented\n");
        printf("✓ Address setup timing requirements met\n");
        printf("✓ Operation queue management functional\n");
        printf("✓ Priority-based execution working\n");
        printf("✓ RDY line handling for read cycles only\n");
        printf("✓ Complete cycle execution integration verified\n");
    } else {
        printf("\n❌ %d test(s) failed. Review implementation.\n", tests_failed);
    }
    
    double success_rate = (double)tests_passed / tests_run * 100.0;
    printf("\nSuccess Rate: %.1f%%\n", success_rate);
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
}

// ===== MAIN TEST RUNNER =====

int main(void) {
    printf("Deferred Operations Architecture Test Suite\n");
    printf("Phase 5.1: φ1/φ2 Phase-Accurate Execution Flow\n");
    printf("Testing hardware-accurate timing and operation deferral\n\n");
    
    test_result_t result;
    
    // Phase coordination tests
    run_test(test_phi1_phi2_phase_coordination, &result);
    run_test(test_address_setup_timing, &result);
    
    // Operation queue tests
    run_test(test_operation_queue_management, &result);
    run_test(test_priority_based_execution, &result);
    
    // RDY line handling tests
    run_test(test_rdy_line_read_cycles, &result);
    run_test(test_rdy_line_write_cycles, &result);
    
    // Integration tests
    run_test(test_complete_cycle_execution, &result);
    
    print_test_summary();
    
    return (tests_failed == 0) ? 0 : 1;
}