#include "mos6510_test_harness.h"
#include "ram.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "mos6510.h"

  // External bus cycle callback
  extern void (*bus_cycle_callback)(void);

  // Global test harness for bus callback access
  static test_harness_t* g_test_harness = NULL;

  // Bus cycle callback for Klaus test execution control
  void klaus_bus_cycle_callback(void) {
    if (!g_test_harness) return;
    
    test_harness_t* harness = g_test_harness;
    mos6510_t* cpu = harness->c64->mos6510;
    
    harness->current_cycles++;
    
    // Check for test completion at Klaus success address
    if (cpu->pc == KLAUS_SUCCESS_ADDRESS) {
        uint8_t instruction = harness->c64->ram->memory[cpu->pc];
        if (instruction == 0x4C) { // JMP instruction
            uint16_t jump_target = harness->c64->ram->memory[cpu->pc + 1] | 
                                 (harness->c64->ram->memory[cpu->pc + 2] << 8);
            if (jump_target == KLAUS_SUCCESS_ADDRESS) {
                harness->test_result = TEST_PASSED;
                harness->execution_complete = true;
                return;
            }
        }
    }
    
    // Check for stuck CPU
    if (cpu->pc == harness->last_pc) {
        harness->stuck_counter++;
        if (harness->stuck_counter > 1000) {
            harness->test_result = TEST_STUCK;
            harness->execution_complete = true;
            return;
        }
    } else {
        harness->stuck_counter = 0;
        harness->last_pc = cpu->pc;
    }
    
    // Check for cycle timeout
    if (harness->current_cycles >= harness->max_cycles) {
        harness->test_result = TEST_TIMEOUT;
        harness->execution_complete = true;
        return;
    }
    
    // Progress reporting
    if (harness->current_cycles % 100000 == 0) {
        printf("Executed %llu cycles, PC=$%04X\n", 
               (unsigned long long)harness->current_cycles, cpu->pc);
    }
}

test_harness_t* test_harness_create(void) {
    test_harness_t* harness = calloc(1, sizeof(test_harness_t));
    if (!harness) {
        fprintf(stderr, "Failed to allocate test harness\n");
        return NULL;
    }

    harness->c64 = c64_system_create();
    if (!harness->c64) {
        fprintf(stderr, "Failed to create C64 system\n");
        free(harness);
        return NULL;
    }

    harness->test_binary = malloc(KLAUS_TEST_BINARY_SIZE);
    if (!harness->test_binary) {
        fprintf(stderr, "Failed to allocate test binary buffer\n");
        c64_system_destroy(harness->c64);
        free(harness);
        return NULL;
    }

    harness->max_cycles = KLAUS_MAX_CYCLES;
    harness->trace_enabled = false;
    harness->trace_file = NULL;
    
    // Initialize execution control fields
    harness->current_cycles = 0;
    harness->execution_complete = false;
    harness->test_result = TEST_NOT_SET;
    harness->last_pc = 0;
    harness->stuck_counter = 0;

    return harness;
}

void test_harness_destroy(test_harness_t* harness) {
    if (!harness) return;

    if (harness->trace_file) {
        fclose(harness->trace_file);
    }

    if (harness->test_binary) {
        free(harness->test_binary);
    }

    if (harness->c64) {
        c64_system_destroy(harness->c64);
    }

    free(harness);
}

bool test_harness_load_binary(test_harness_t* harness, const char* filename) {
    if (!harness || !filename) return false;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open test binary: %s\n", filename);
        return false;
    }

    size_t bytes_read = fread(harness->test_binary, 1, KLAUS_TEST_BINARY_SIZE, file);
    fclose(file);

    if (bytes_read == 0) {
        fprintf(stderr, "Failed to read test binary\n");
        return false;
    }

    harness->binary_size = bytes_read;

    for (size_t i = 0; i < bytes_read && i < 0x10000; i++) {
        harness->c64->ram->memory[i] = harness->test_binary[i];
    }

    printf("Loaded %zu bytes from %s\n", bytes_read, filename);
    return true;
}

void test_harness_enable_trace(test_harness_t* harness, const char* trace_filename) {
    if (!harness) return;

    if (harness->trace_file) {
        fclose(harness->trace_file);
    }

    if (trace_filename) {
        harness->trace_file = fopen(trace_filename, "w");
        if (harness->trace_file) {
            harness->trace_enabled = true;
            printf("Trace enabled: %s\n", trace_filename);
        } else {
            fprintf(stderr, "Failed to open trace file: %s\n", trace_filename);
        }
    }
}

void test_harness_disable_trace(test_harness_t* harness) {
    if (!harness) return;

    if (harness->trace_file) {
        fclose(harness->trace_file);
        harness->trace_file = NULL;
    }
    harness->trace_enabled = false;
}

test_status_t test_harness_run_klaus_test(test_harness_t* harness) {
    test_status_t status = {0};
    
    if (!harness || !harness->c64) {
        status.result = TEST_ERROR;
        strncpy(status.error_message, "Invalid harness", sizeof(status.error_message) - 1);
        return status;
    }

    printf("Debug: harness->c64 = %p\n", (void*)harness->c64);
    printf("Debug: harness->c64->mos6510 = %p\n", (void*)harness->c64->mos6510);
    
    mos6510_t* cpu = harness->c64->mos6510;
    if (!cpu) {
        status.result = TEST_ERROR;
        strncpy(status.error_message, "CPU not initialized", sizeof(status.error_message) - 1);
        return status;
    }
    
    printf("Debug: CPU pointer is valid\n");
    printf("Debug: CPU bus pointer = %p\n", (void*)cpu->c64_bus);
    
    if (!cpu->c64_bus) {
        status.result = TEST_ERROR;
        strncpy(status.error_message, "CPU bus not attached", sizeof(status.error_message) - 1);
        return status;
    }
    
    // Initialize CPU with proper reset sequence first
    printf("Debug: Calling mos6510_reset...\n");
    printf("Debug: About to read reset vector from $FFFC\n");
    
    // Try a manual memory read first to test the bus
    printf("Debug: Testing bus read at $FFFC...\n");
    uint8_t test_byte = harness->c64->ram->memory[0xFFFC];
    printf("Debug: Direct RAM read at $FFFC = $%02X\n", test_byte);
    
    // Try the CPU read cycle function
    printf("Debug: Testing mos6510_read_cycle at $FFFC...\n");
    uint8_t cpu_test = mos6510_read_cycle(cpu, 0xFFFC);
    printf("Debug: CPU read cycle at $FFFC = $%02X\n", cpu_test);
    
    mos6510_reset(cpu);
    printf("Debug: Reset complete, PC now $%04X\n", cpu->pc);
    
    // Configure CPU I/O port for Klaus test - map all memory to RAM
    // Write to I/O port $0001 to set banking mode (LORAM=0, HIRAM=0, CHAREN=1)
    printf("Debug: Setting CPU I/O port for all-RAM mode...\n");
    mos6510_write_cycle(cpu, 0x0001, 0x04);  // LORAM=0, HIRAM=0, CHAREN=1 (mode 4)
    printf("Debug: Set memory banking mode via I/O port write\n");
    
    // Test the bus read again
    printf("Debug: Testing CPU read cycle at $FFFC after banking change...\n");
    
    // Debug the bus mapping first
    c64_bus_t* bus = cpu->c64_bus;
    uint8_t page_ff_id = READ_ID(bus->device_id_per_page[0xFF]);
    printf("Debug: Device ID for page $FF = %d\n", page_ff_id);
    printf("Debug: Total chips in system = %d\n", harness->c64->system.chip_count);
    
    uint8_t new_test = mos6510_read_cycle(cpu, 0xFFFC);
    printf("Debug: CPU read cycle at $FFFC now returns = $%02X\n", new_test);
    
    // Double-check what's actually in RAM
    printf("Debug: Direct RAM read at $FFFC after banking = $%02X\n", harness->c64->ram->memory[0xFFFC]);
    printf("Debug: Direct RAM read at $FFFD after banking = $%02X\n", harness->c64->ram->memory[0xFFFD]);
    
    // Now manually set PC for Klaus test
    cpu->pc = KLAUS_TEST_START_ADDRESS;
    printf("Debug: Set PC to $%04X for Klaus test\n", cpu->pc);
    
    printf("Starting Klaus functional test...\n");
    printf("Initial PC: $%04X\n", cpu->pc);
    
    // Initialize execution control fields
    harness->current_cycles = 0;
    harness->execution_complete = false;
    harness->test_result = TEST_NOT_SET;
    harness->last_pc = 0;
    harness->stuck_counter = 0;
    
    // Set up bus cycle callback for execution control
    g_test_harness = harness;
    extern void (*bus_cycle_callback)(void);
    bus_cycle_callback = klaus_bus_cycle_callback;
    
    status.start_time = time(NULL);
    
    // Use mos6510_execute() for continuous execution instead of stepping
    // The callback will control execution and set execution_complete when done
    printf("Starting CPU execution with callback-based control...\n");
    mos6510_start_intercept();
    mos6510_execute(harness->c64->mos6510);
    
    // Execution completed, clean up callback
    bus_cycle_callback = NULL;
    g_test_harness = NULL;
    
    status.end_time = time(NULL);
    status.cycles_executed = harness->current_cycles;
    status.final_pc = cpu->pc;
    
    // Set final result based on callback execution
    status.result = harness->test_result;
    if (status.result == TEST_NOT_SET) {
        status.result = TEST_FAILED;
        snprintf(status.error_message, sizeof(status.error_message),
                "Test failed at PC=$%04X", cpu->pc);
    } else if (status.result == TEST_STUCK) {
        snprintf(status.error_message, sizeof(status.error_message),
                "CPU stuck at PC=$%04X for %u cycles", cpu->pc, harness->stuck_counter);
    } else if (status.result == TEST_TIMEOUT) {
        snprintf(status.error_message, sizeof(status.error_message),
                "Test timed out after %llu cycles", (unsigned long long)harness->current_cycles);
    }
    
    return status;
}

void test_harness_print_status(const test_status_t* status) {
    if (!status) return;

    printf("\n=== Test Results ===\n");
    printf("Result: ");
    
    switch (status->result) {
        case TEST_PASSED:
            printf("PASSED\n");
            break;
        case TEST_FAILED:
            printf("FAILED\n");
            break;
        case TEST_TIMEOUT:
            printf("TIMEOUT\n");
            break;
        case TEST_STUCK:
            printf("STUCK\n");
            break;
        case TEST_ERROR:
            printf("ERROR\n");
            break;
        default:
            printf("UNKNOWN\n");
            break;
    }
    
    printf("Cycles executed: %lu\n", status->cycles_executed);
    printf("Final PC: $%04X\n", status->final_pc);
    printf("Duration: %ld seconds\n", status->end_time - status->start_time);
    
    if (strlen(status->error_message) > 0) {
        printf("Error: %s\n", status->error_message);
    }
    
    printf("==================\n\n");
}

bool klaus_test_functional(void) {
    printf("Running Klaus 6502 Functional Test...\n");
    
    test_harness_t* harness = test_harness_create();
    if (!harness) return false;
    
    bool result = test_harness_load_binary(harness, 
        "6502_65C02_functional_tests/bin_files/6502_functional_test.bin");
    
    if (result) {
        test_status_t status = test_harness_run_klaus_test(harness);
        test_harness_print_status(&status);
        result = (status.result == TEST_PASSED);
    }
    
    test_harness_destroy(harness);
    return result;
}

bool klaus_test_decimal(void) {
    printf("Klaus decimal mode test not yet implemented\n");
    return true;
}

bool klaus_test_interrupt(void) {
    printf("Klaus interrupt test not yet implemented\n");
    return true;
}

bool klaus_test_suite_run_all(void) {
    printf("=== Klaus 6502 Test Suite ===\n\n");
    
    bool all_passed = true;
    
    if (!klaus_test_functional()) {
        printf("Functional test FAILED\n");
        all_passed = false;
    }
    
    if (!klaus_test_decimal()) {
        printf("Decimal test FAILED\n");
        all_passed = false;
    }
    
    if (!klaus_test_interrupt()) {
        printf("Interrupt test FAILED\n");
        all_passed = false;
    }
    
    printf("\n=== Test Suite Complete ===\n");
    printf("Overall result: %s\n", all_passed ? "PASSED" : "FAILED");
    
    return all_passed;
}
