#ifndef FAM65XX_CPP_TEST_HARNESS_H
#define FAM65XX_CPP_TEST_HARNESS_H

#include "../src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "../src/core/system_lines.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// Klaus2m5 6502 Functional Test Harness for fam65xx_cpp Implementation
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

// Memory interface for the test harness
class TestMemory {
private:
    uint8_t memory[65536];
    
public:
    TestMemory() {
        // Initialize memory to zero
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0;
        }
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
    
    void load_binary(const uint8_t* data, size_t size, uint16_t start_addr = 0) {
        for (size_t i = 0; i < size && (start_addr + i) < 65536; i++) {
            memory[start_addr + i] = data[i];
        }
    }
    
    uint8_t* get_memory_ptr() { return memory; }
};

// Test harness structure for fam65xx_cpp
typedef struct {
    fam65xx_cpp::fam65xx<config_6502>* cpu;  // CPU instance
    TestMemory* memory;                 // Test memory
    uint8_t* test_binary;              // Klaus test binary data
    size_t binary_size;                // Size of loaded binary
    uint64_t max_cycles;               // Maximum allowed cycles
    bool trace_enabled;                // Enable instruction tracing
    FILE* trace_file;                  // Trace output file
    
    // Execution control
    uint64_t current_cycles;           // Current cycle count
    bool execution_complete;           // Flag to stop execution
    test_result_t test_result;         // Current test result
    uint16_t last_pc;                  // Last PC for stuck detection
    uint32_t stuck_counter;            // Counter for stuck detection
} fam65xx_test_harness_t;

// Test execution status
typedef struct {
    test_result_t result;
    uint64_t cycles_executed;
    time_t start_time;
    time_t end_time;
    uint16_t final_pc;
    char error_message[256];
} fam65xx_test_status_t;

// Function declarations
fam65xx_test_harness_t* fam65xx_test_harness_create(void);
void fam65xx_test_harness_destroy(fam65xx_test_harness_t* harness);
bool fam65xx_test_harness_load_binary(fam65xx_test_harness_t* harness, const char* filename);
fam65xx_test_status_t fam65xx_test_harness_run_klaus_test(fam65xx_test_harness_t* harness);
void fam65xx_test_harness_enable_trace(fam65xx_test_harness_t* harness, const char* trace_filename);
void fam65xx_test_harness_disable_trace(fam65xx_test_harness_t* harness);
void fam65xx_test_harness_print_status(const fam65xx_test_status_t* status);

// Klaus test-specific functions for fam65xx_cpp
bool fam65xx_klaus_test_suite_run_all(void);
bool fam65xx_klaus_test_functional(void);
bool fam65xx_klaus_test_decimal(void);
bool fam65xx_klaus_test_interrupt(void);

#endif // FAM65XX_CPP_TEST_HARNESS_H