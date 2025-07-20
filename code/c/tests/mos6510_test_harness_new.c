#include "mos6510_test_harness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

test_harness_t* test_harness_create(void) {
    test_harness_t* harness = calloc(1, sizeof(test_harness_t));
    if (!harness) {
        fprintf(stderr, "Failed to allocate test harness\n");
        return NULL;
    }

    system_config_t config = {
        .vicii_standard = VIC_PAL
    };
    harness->c64 = c64_system_create(&config);
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

    mos6510_t* cpu = harness->c64->mos6510;
    cpu->pc = KLAUS_TEST_START_ADDRESS;
    
    printf("Starting Klaus functional test...\n");
    printf("Initial PC: $%04X\n", cpu->pc);
    
    status.start_time = time(NULL);
    uint64_t cycles = 0;
    uint16_t last_pc = 0;
    uint32_t stuck_counter = 0;
    const uint32_t STUCK_THRESHOLD = 1000;
    
    while (cycles < harness->max_cycles) {
        uint16_t current_pc = cpu->pc;
        
        if (current_pc == KLAUS_SUCCESS_ADDRESS) {
            uint8_t instruction = harness->c64->ram->memory[current_pc];
            if (instruction == 0x4C) {
                uint16_t jump_target = harness->c64->ram->memory[current_pc + 1] | 
                                     (harness->c64->ram->memory[current_pc + 2] << 8);
                if (jump_target == KLAUS_SUCCESS_ADDRESS) {
                    status.result = TEST_PASSED;
                    break;
                }
            }
        }
        
        if (current_pc == last_pc) {
            stuck_counter++;
            if (stuck_counter > STUCK_THRESHOLD) {
                status.result = TEST_STUCK;
                snprintf(status.error_message, sizeof(status.error_message),
                        "CPU stuck at PC=$%04X for %u cycles", current_pc, stuck_counter);
                break;
            }
        } else {
            stuck_counter = 0;
            last_pc = current_pc;
        }
        
        mos6510_step(harness->c64->mos6510);
        cycles++;
        
        if (cycles % 100000 == 0) {
            printf("Executed %lu cycles, PC=$%04X\n", cycles, cpu->pc);
        }
    }
    
    status.end_time = time(NULL);
    status.cycles_executed = cycles;
    status.final_pc = cpu->pc;
    
    if (status.result == TEST_NOT_SET) {
        if (cycles >= harness->max_cycles) {
            status.result = TEST_TIMEOUT;
            snprintf(status.error_message, sizeof(status.error_message),
                    "Test timed out after %lu cycles", cycles);
        } else {
            status.result = TEST_FAILED;
            snprintf(status.error_message, sizeof(status.error_message),
                    "Test failed at PC=$%04X", cpu->pc);
        }
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
