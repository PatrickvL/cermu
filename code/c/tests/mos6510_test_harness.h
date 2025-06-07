#ifndef MOS6510_TEST_HARNESS_H
#define MOS6510_TEST_HARNESS_H

#include "mos6510.h"
#include "c64.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// Klaus2m5 6502 Functional Test Harness for MOS6510
// ============================================================================

// Test configuration constants
#define KLAUS_TEST_START_ADDRESS 0x0400  // Entry point of functional test
#define KLAUS_SUCCESS_ADDRESS    0x346C  // Success loop: JMP start (run again)
#define KLAUS_MAX_CYCLES         100000000ULL  // Maximum cycles before timeout
#define KLAUS_TEST_BINARY_SIZE   65536   // 64KB test binary

// Test result types
typedef enum {
    TEST_NOT_SET,
    TEST_PASSED,
    TEST_FAILED,
    TEST_TIMEOUT,
    TEST_STUCK,
    TEST_ERROR
} test_result_t;

// Test harness structure
typedef struct {
    c64_t* c64;                     // C64 system instance
    uint8_t* test_binary;           // Klaus test binary data
    size_t binary_size;             // Size of loaded binary
    uint64_t max_cycles;            // Maximum allowed cycles
    bool trace_enabled;             // Enable instruction tracing
    FILE* trace_file;               // Trace output file
    
    // Execution control
    uint64_t current_cycles;        // Current cycle count
    bool execution_complete;        // Flag to stop execution
    test_result_t test_result;      // Current test result
    uint16_t last_pc;               // Last PC for stuck detection
    uint32_t stuck_counter;         // Counter for stuck detection
} test_harness_t;

// Test execution status
typedef struct {
    test_result_t result;
    uint64_t cycles_executed;
    time_t start_time;
    time_t end_time;
    uint16_t final_pc;
    char error_message[256];
} test_status_t;

// Function declarations
test_harness_t* test_harness_create(void);
void test_harness_destroy(test_harness_t* harness);
bool test_harness_load_binary(test_harness_t* harness, const char* filename);
test_status_t test_harness_run_klaus_test(test_harness_t* harness);
void test_harness_enable_trace(test_harness_t* harness, const char* trace_filename);
void test_harness_disable_trace(test_harness_t* harness);
void test_harness_print_status(const test_status_t* status);

// Klaus test-specific functions
bool klaus_test_suite_run_all(void);
bool klaus_test_functional(void);
bool klaus_test_decimal(void);
bool klaus_test_interrupt(void);

#endif // MOS6510_TEST_HARNESS_H
