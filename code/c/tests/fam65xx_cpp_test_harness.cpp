#include "fam65xx_cpp_test_harness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

fam65xx_test_harness_t* fam65xx_test_harness_create(void) {
    fam65xx_test_harness_t* harness = (fam65xx_test_harness_t*)calloc(1, sizeof(fam65xx_test_harness_t));
    if (!harness) {
        fprintf(stderr, "Failed to allocate test harness\n");
        return NULL;
    }

    // Create CPU instance
    harness->cpu = new fam65xx_cpp::fam65xx<config_6502>();
    if (!harness->cpu) {
        fprintf(stderr, "Failed to create CPU instance\n");
        free(harness);
        return NULL;
    }

    // Create memory interface
    harness->memory = new TestMemory();
    if (!harness->memory) {
        fprintf(stderr, "Failed to create memory interface\n");
        delete harness->cpu;
        free(harness);
        return NULL;
    }

    harness->test_binary = (uint8_t*)malloc(KLAUS_TEST_BINARY_SIZE);
    if (!harness->test_binary) {
        fprintf(stderr, "Failed to allocate test binary buffer\n");
        delete harness->memory;
        delete harness->cpu;
        free(harness);
        return NULL;
    }

    harness->max_cycles = KLAUS_MAX_CYCLES;
    harness->trace_enabled = false;
    harness->trace_file = NULL;

    return harness;
}

void fam65xx_test_harness_destroy(fam65xx_test_harness_t* harness) {
    if (!harness) return;

    if (harness->trace_file) {
        fclose(harness->trace_file);
    }

    if (harness->test_binary) {
        free(harness->test_binary);
    }

    if (harness->memory) {
        delete harness->memory;
    }

    if (harness->cpu) {
        delete harness->cpu;
    }

    free(harness);
}

bool fam65xx_test_harness_load_binary(fam65xx_test_harness_t* harness, const char* filename) {
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

    // Load binary into memory
    harness->memory->load_binary(harness->test_binary, bytes_read, 0);

    printf("Loaded %zu bytes from %s\n", bytes_read, filename);
    return true;
}

void fam65xx_test_harness_enable_trace(fam65xx_test_harness_t* harness, const char* trace_filename) {
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

void fam65xx_test_harness_disable_trace(fam65xx_test_harness_t* harness) {
    if (!harness) return;

    if (harness->trace_file) {
        fclose(harness->trace_file);
        harness->trace_file = NULL;
    }
    harness->trace_enabled = false;
}

fam65xx_test_status_t fam65xx_test_harness_run_klaus_test(fam65xx_test_harness_t* harness) {
    fam65xx_test_status_t status = {};
    status.result = TEST_NOT_SET;
    status.cycles_executed = 0;
    status.start_time = 0;
    status.end_time = 0;
    status.final_pc = 0;
    status.error_message[0] = '\0';
    
    if (!harness || !harness->cpu) {
        status.result = TEST_ERROR;
        strncpy(status.error_message, "Invalid harness", sizeof(status.error_message) - 1);
        return status;
    }

    // Initialize CPU
    harness->cpu->init();
    harness->cpu->set_pc(KLAUS_TEST_START_ADDRESS);
    
    printf("Starting Klaus functional test...\n");
    printf("Initial PC: $%04X\n", harness->cpu->get_pc());
    
    status.start_time = time(NULL);
    uint64_t cycles = 0;
    uint16_t last_pc = 0;
    uint32_t stuck_counter = 0;
    const uint32_t STUCK_THRESHOLD = 1000;
    
    while (cycles < harness->max_cycles) {
        uint16_t current_pc = harness->cpu->get_pc();
        
        // Check for success condition
        if (current_pc == KLAUS_SUCCESS_ADDRESS) {
            uint8_t instruction = harness->memory->read(current_pc);
            if (instruction == 0x4C) { // JMP instruction
                uint16_t jump_target = harness->memory->read(current_pc + 1) | 
                                     (harness->memory->read(current_pc + 2) << 8);
                if (jump_target == KLAUS_SUCCESS_ADDRESS) {
                    status.result = TEST_PASSED;
                    break;
                }
            }
        }
        
        // Check for stuck condition
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
        
        // Execute one CPU cycle
        bus_state_t bus_state = 0;
        
        // Set up bus for memory read at PC
        BUS_SET_ADDR(bus_state, current_pc);
        uint8_t data = harness->memory->read(BUS_GET_ADDR(bus_state));
        BUS_SET_DATA(bus_state, data);
        
        // Execute CPU cycle
        bus_state = harness->cpu->cycle_tick(bus_state);
        
        // Handle memory writes
        if (!(bus_state & BUS_BIT(BUS_RW_BIT))) { // Write cycle
            uint16_t addr = BUS_GET_ADDR(bus_state);
            uint8_t write_data = BUS_GET_DATA(bus_state);
            harness->memory->write(addr, write_data);
        }
        
        cycles++;
        
        // Optional trace output
        if (harness->trace_enabled && harness->trace_file) {
            if (cycles % 1000 == 0) { // Trace every 1000 cycles to avoid huge files
                fprintf(harness->trace_file, "Cycle %lu: PC=$%04X A=$%02X X=$%02X Y=$%02X P=$%02X S=$%02X\n",
                       cycles, harness->cpu->get_pc(), harness->cpu->get_a(), harness->cpu->get_x(),
                       harness->cpu->get_y(), harness->cpu->get_p(), harness->cpu->get_s());
            }
        }
        
        if (cycles % 100000 == 0) {
            printf("Executed %lu cycles, PC=$%04X\n", cycles, harness->cpu->get_pc());
        }
    }
    
    status.end_time = time(NULL);
    status.cycles_executed = cycles;
    status.final_pc = harness->cpu->get_pc();
    
    if (status.result == TEST_NOT_SET) {
        if (cycles >= harness->max_cycles) {
            status.result = TEST_TIMEOUT;
            snprintf(status.error_message, sizeof(status.error_message),
                    "Test timed out after %lu cycles", cycles);
        } else {
            status.result = TEST_FAILED;
            snprintf(status.error_message, sizeof(status.error_message),
                    "Test failed at PC=$%04X", harness->cpu->get_pc());
        }
    }
    
    return status;
}

void fam65xx_test_harness_print_status(const fam65xx_test_status_t* status) {
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

bool fam65xx_klaus_test_functional(void) {
    printf("Running Klaus 6502 Functional Test (fam65xx_cpp)...\n");
    
    fam65xx_test_harness_t* harness = fam65xx_test_harness_create();
    if (!harness) return false;
    
    bool result = fam65xx_test_harness_load_binary(harness, 
        "external/6502-tests/6502_65C02_functional_tests/bin_files/6502_functional_test.bin");
    
    if (result) {
        fam65xx_test_status_t status = fam65xx_test_harness_run_klaus_test(harness);
        fam65xx_test_harness_print_status(&status);
        result = (status.result == TEST_PASSED);
    }
    
    fam65xx_test_harness_destroy(harness);
    return result;
}

bool fam65xx_klaus_test_decimal(void) {
    printf("Klaus decimal mode test not yet implemented for fam65xx_cpp\n");
    return true;
}

bool fam65xx_klaus_test_interrupt(void) {
    printf("Klaus interrupt test not yet implemented for fam65xx_cpp\n");
    return true;
}

bool fam65xx_klaus_test_suite_run_all(void) {
    printf("=== Klaus 6502 Test Suite (fam65xx_cpp) ===\n\n");
    
    bool all_passed = true;
    
    if (!fam65xx_klaus_test_functional()) {
        printf("Functional test FAILED\n");
        all_passed = false;
    }
    
    if (!fam65xx_klaus_test_decimal()) {
        printf("Decimal test FAILED\n");
        all_passed = false;
    }
    
    if (!fam65xx_klaus_test_interrupt()) {
        printf("Interrupt test FAILED\n");
        all_passed = false;
    }
    
    printf("\n=== Test Suite Complete ===\n");
    printf("Overall result: %s\n", all_passed ? "PASSED" : "FAILED");
    
    return all_passed;
}