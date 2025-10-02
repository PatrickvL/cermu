/*
 * fam65xx_processor_tests_runner.c
 * 
 * Comprehensive ProcessorTests runner using the fam65xx 6502 core
 * Tests cycle and hardware accuracy against ProcessorTests ground truth
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

#define CHIPS_IMPL
#include "../src/chip/cpu/fam65xx_cpp/opcode_gen/fam65xx.h"
#include "json_parser.h"

// Test harness with memory and CPU state
typedef struct {
    fam65xx_t cpu;
    uint8_t memory[65536];
    uint32_t cycle_count;
    bool verbose;
} test_harness_t;

// Test results tracking
typedef struct {
    uint32_t total_tests;
    uint32_t passed_tests;
    uint32_t failed_tests;
    uint32_t cycle_mismatches;
    uint32_t state_mismatches;
    uint32_t opcode_failures[256];
    uint32_t opcode_totals[256];
} test_results_t;

static test_results_t g_results = {0};

// Debug and performance tracking
typedef struct {
    clock_t start_time;
    uint64_t total_cycles;
    uint64_t total_instructions;
    bool interactive_mode;
    bool performance_mode;
} debug_context_t;

static debug_context_t g_debug = {0};

// Memory callbacks for the CPU
static uint8_t test_mem_read(void* user_data, uint16_t addr, uint8_t bus_state) {
    test_harness_t* harness = (test_harness_t*)user_data;
    // For ProcessorTests, return the actual memory value, ignoring bus_state
    // This ensures clean reads without floating bit complications
    (void)bus_state;  // Suppress unused parameter warning
    return harness->memory[addr];
}

static void test_mem_write(void* user_data, uint16_t addr, uint8_t data) {
    test_harness_t* harness = (test_harness_t*)user_data;
    harness->memory[addr] = data;
}

// Initialize test harness
static void init_test_harness(test_harness_t* harness, bool verbose) {
    memset(harness, 0, sizeof(test_harness_t));
    harness->verbose = verbose;
    
    // Clear memory
    memset(harness->memory, 0, sizeof(harness->memory));
    
    // Initialize CPU with memory callbacks
    fam65xx_desc_t desc = {
        .mem_read = test_mem_read,
        .mem_write = test_mem_write,
        .mem_user_data = harness
    };
    
    fam65xx_init(&harness->cpu, &desc);
    
    harness->cycle_count = 0;
}

// Set up CPU state from ProcessorTests initial state
static void setup_cpu_state(test_harness_t* harness, const cpu_state_t* initial) {
    // Clear memory first
    memset(harness->memory, 0, sizeof(harness->memory));
    
    // Set up memory from RAM entries
    for (int i = 0; i < initial->ram_count; i++) {
        uint16_t addr = initial->ram[i].address;
        if (harness->verbose) {
            printf("  RAM[%d]: addr=0x%04X, byte_count=%d\n", i, addr, initial->ram[i].byte_count);
        }
        for (int j = 0; j < initial->ram[i].byte_count; j++) {
            harness->memory[addr + j] = initial->ram[i].bytes[j];
            if (harness->verbose) {
                printf("  Setting memory[0x%04X] = 0x%02X\n", addr + j, initial->ram[i].bytes[j]);
            }
        }
    }
    
    // Set CPU registers AFTER memory setup
    fam65xx_set_pc(&harness->cpu, initial->pc);
    fam65xx_set_a(&harness->cpu, initial->a);
    fam65xx_set_x(&harness->cpu, initial->x);
    fam65xx_set_y(&harness->cpu, initial->y);
    fam65xx_set_s(&harness->cpu, initial->s);
    fam65xx_set_p(&harness->cpu, initial->p);
    
    // Set up CPU for immediate instruction execution
    // Clear any interrupt/reset state from initialization
    harness->cpu.brk_flags = 0;
    // For ProcessorTests: CPU starts by fetching instruction at PC
    // This will read the opcode and set up the instruction execution
    harness->cpu.IR = 256;  // Start at fetch cycle to read first opcode
    
    // Reset cycle count
    harness->cycle_count = 0;
    
    if (harness->verbose) {
        printf("  Setup: PC=0x%04X A=0x%02X X=0x%02X Y=0x%02X S=0x%02X P=0x%02X\n",
               initial->pc, initial->a, initial->x, initial->y, initial->s, initial->p);
        printf("  Memory at PC: 0x%02X\n", harness->memory[initial->pc]);
    }
}

// Execute one instruction and count cycles
static bool execute_instruction(test_harness_t* harness) {
    uint16_t initial_pc = fam65xx_pc(&harness->cpu);
    uint32_t initial_cycles = harness->cycle_count;
    uint64_t pins = FAM65XX_RDY;  // Ready signal active
    
    // Execute cycles until instruction completes
    int max_cycles = 20;  // Increase safety limit for complex instructions
    bool instruction_started = false;
    uint32_t instruction_cycles = 0;  // Count only instruction execution cycles
    
    if (harness->verbose) {
        printf("  Execution start: PC=0x%04X, IR=0x%04X, A=0x%02X, P=0x%02X\n",
               fam65xx_pc(&harness->cpu), harness->cpu.IR, fam65xx_a(&harness->cpu), fam65xx_p(&harness->cpu));
    }
    
    for (int i = 0; i < max_cycles; i++) {
        if (harness->verbose) {
            printf("    Cycle %d: IR=0x%04X, PC=0x%04X, A=0x%02X\n",
                   i, harness->cpu.IR, fam65xx_pc(&harness->cpu), fam65xx_a(&harness->cpu));
        }
        
        // Execute one CPU cycle
        pins = fam65xx_tick(&harness->cpu, pins);
        harness->cycle_count++;
        
        if (harness->verbose) {
            printf("    After tick: IR=0x%04X, PC=0x%04X, A=0x%02X, SYNC=%d\n",
                   harness->cpu.IR, fam65xx_pc(&harness->cpu), fam65xx_a(&harness->cpu),
                   (pins & FAM65XX_SYNC) ? 1 : 0);
        }
        
        // Check if we're at the start of a new instruction (SYNC high)
        if (pins & FAM65XX_SYNC) {
            if (instruction_started) {
                // We've completed the previous instruction and started fetch for next
                // ProcessorTests expects PC to point to next instruction, but not advanced by fetch
                // So decrement PC to compensate for the _FETCH() that advanced it
                uint16_t corrected_pc = fam65xx_pc(&harness->cpu) - 1;
                fam65xx_set_pc(&harness->cpu, corrected_pc);
                
                if (harness->verbose) {
                    printf("  Instruction completed, PC corrected from 0x%04X to 0x%04X\n",
                           fam65xx_pc(&harness->cpu) + 1, corrected_pc);
                }
                break;
            } else {
                // This is the start of our instruction
                instruction_started = true;
                instruction_cycles = 1;  // Count this cycle
                if (harness->verbose) {
                    printf("  Instruction started with opcode 0x%02X\n", harness->cpu.opcode);
                }
            }
        } else if (instruction_started) {
            // Count cycles that are part of instruction execution (not the final fetch)
            instruction_cycles++;
        }
        
        // Safety check for infinite loops
        if (i == max_cycles - 1) {
            if (harness->verbose) {
                printf("ERROR: Instruction execution exceeded max cycles (PC=0x%04X, IR=0x%02X, pins=0x%lX)\n",
                       fam65xx_pc(&harness->cpu), harness->cpu.IR, pins);
            }
            return false;
        }
    }
    
    // Update cycle count with only instruction cycles (excluding final fetch)
    harness->cycle_count = initial_cycles + instruction_cycles;
    
    return true;
}

// Compare CPU states
static bool compare_cpu_state(test_harness_t* harness, const cpu_state_t* expected, const char* test_name) {
    bool match = true;
    
    // Check registers
    if (fam65xx_pc(&harness->cpu) != expected->pc) {
        if (harness->verbose) {
            printf("FAIL %s: PC mismatch - expected 0x%04X, got 0x%04X\n", 
                   test_name, expected->pc, fam65xx_pc(&harness->cpu));
        }
        match = false;
    }
    
    if (fam65xx_a(&harness->cpu) != expected->a) {
        if (harness->verbose) {
            printf("FAIL %s: A mismatch - expected 0x%02X, got 0x%02X\n", 
                   test_name, expected->a, fam65xx_a(&harness->cpu));
        }
        match = false;
    }
    
    if (fam65xx_x(&harness->cpu) != expected->x) {
        if (harness->verbose) {
            printf("FAIL %s: X mismatch - expected 0x%02X, got 0x%02X\n", 
                   test_name, expected->x, fam65xx_x(&harness->cpu));
        }
        match = false;
    }
    
    if (fam65xx_y(&harness->cpu) != expected->y) {
        if (harness->verbose) {
            printf("FAIL %s: Y mismatch - expected 0x%02X, got 0x%02X\n", 
                   test_name, expected->y, fam65xx_y(&harness->cpu));
        }
        match = false;
    }
    
    if (fam65xx_s(&harness->cpu) != expected->s) {
        if (harness->verbose) {
            printf("FAIL %s: S mismatch - expected 0x%02X, got 0x%02X\n", 
                   test_name, expected->s, fam65xx_s(&harness->cpu));
        }
        match = false;
    }
    
    if (fam65xx_p(&harness->cpu) != expected->p) {
        if (harness->verbose) {
            printf("FAIL %s: P mismatch - expected 0x%02X, got 0x%02X\n", 
                   test_name, expected->p, fam65xx_p(&harness->cpu));
            
            // Detailed flag analysis
            uint8_t exp_p = expected->p;
            uint8_t got_p = fam65xx_p(&harness->cpu);
            printf("  Flag breakdown: N=%d/%d V=%d/%d U=%d/%d B=%d/%d D=%d/%d I=%d/%d Z=%d/%d C=%d/%d\n",
                   (got_p & 0x80) ? 1 : 0, (exp_p & 0x80) ? 1 : 0,
                   (got_p & 0x40) ? 1 : 0, (exp_p & 0x40) ? 1 : 0,
                   (got_p & 0x20) ? 1 : 0, (exp_p & 0x20) ? 1 : 0,
                   (got_p & 0x10) ? 1 : 0, (exp_p & 0x10) ? 1 : 0,
                   (got_p & 0x08) ? 1 : 0, (exp_p & 0x08) ? 1 : 0,
                   (got_p & 0x04) ? 1 : 0, (exp_p & 0x04) ? 1 : 0,
                   (got_p & 0x02) ? 1 : 0, (exp_p & 0x02) ? 1 : 0,
                   (got_p & 0x01) ? 1 : 0, (exp_p & 0x01) ? 1 : 0);
        }
        match = false;
    }
    
    // Check memory
    for (int i = 0; i < expected->ram_count; i++) {
        uint16_t addr = expected->ram[i].address;
        for (int j = 0; j < expected->ram[i].byte_count; j++) {
            uint8_t expected_val = expected->ram[i].bytes[j];
            uint8_t actual_val = harness->memory[addr + j];
            
            if (actual_val != expected_val) {
                if (harness->verbose) {
                    printf("FAIL %s: Memory[0x%04X] mismatch - expected 0x%02X, got 0x%02X\n", 
                           test_name, addr + j, expected_val, actual_val);
                }
                match = false;
            }
        }
    }
    
    return match;
}

// Enhanced CPU state printing with better formatting
static void print_cpu_state_detailed(test_harness_t* harness, const char* context) {
    printf("%s: A:%02X X:%02X Y:%02X SP:%02X P:%02X PC:%04X Cycles:%u IR:%04X\n",
           context,
           fam65xx_a(&harness->cpu),
           fam65xx_x(&harness->cpu),
           fam65xx_y(&harness->cpu),
           fam65xx_s(&harness->cpu),
           fam65xx_p(&harness->cpu),
           fam65xx_pc(&harness->cpu),
           harness->cycle_count,
           harness->cpu.IR);
}

// Interactive debug session
static void run_debug_session(test_harness_t* harness) {
    char command[32];
    
    printf("\n=== INTERACTIVE DEBUG SESSION ===\n");
    printf("Commands: step, cycle, reset, state, quit\n");
    print_cpu_state_detailed(harness, "Initial");
    
    while (1) {
        printf("debug> ");
        if (!fgets(command, sizeof(command), stdin)) {
            break;
        }
        
        // Remove newline
        command[strcspn(command, "\n")] = 0;
        
        if (strcmp(command, "quit") == 0 || strcmp(command, "q") == 0) {
            break;
        } else if (strcmp(command, "step") == 0 || strcmp(command, "s") == 0) {
            if (execute_instruction(harness)) {
                print_cpu_state_detailed(harness, "After step");
            } else {
                printf("ERROR: Instruction execution failed\n");
            }
        } else if (strcmp(command, "cycle") == 0 || strcmp(command, "c") == 0) {
            uint64_t pins = FAM65XX_RDY;
            pins = fam65xx_tick(&harness->cpu, pins);
            harness->cycle_count++;
            print_cpu_state_detailed(harness, "After cycle");
        } else if (strcmp(command, "reset") == 0 || strcmp(command, "r") == 0) {
            init_test_harness(harness, harness->verbose);
            print_cpu_state_detailed(harness, "After reset");
        } else if (strcmp(command, "state") == 0) {
            print_cpu_state_detailed(harness, "Current");
            // Show memory around PC
            uint16_t pc = fam65xx_pc(&harness->cpu);
            printf("Memory around PC:\n");
            for (int i = -2; i <= 5; i++) {
                uint16_t addr = pc + i;
                printf("  %04X: %02X %s\n", addr, harness->memory[addr],
                       (i == 0) ? "<-- PC" : "");
            }
        } else if (strlen(command) > 0) {
            printf("Unknown command. Available: step, cycle, reset, state, quit\n");
        }
    }
    
    printf("Debug session ended.\n");
}

// Performance benchmarking
static void run_performance_benchmark(test_harness_t* harness, int num_instructions) {
    printf("\n=== PERFORMANCE BENCHMARK ===\n");
    
    clock_t start_time = clock();
    uint32_t start_cycles = harness->cycle_count;
    
    init_test_harness(harness, false);  // Non-verbose for benchmark
    
    // Set up a simple test program
    harness->memory[0x1000] = 0xEA;  // NOP
    harness->memory[0x1001] = 0xA9;  // LDA #$42
    harness->memory[0x1002] = 0x42;
    harness->memory[0x1003] = 0x4C;  // JMP $1000 (loop)
    harness->memory[0x1004] = 0x00;
    harness->memory[0x1005] = 0x10;
    
    fam65xx_set_pc(&harness->cpu, 0x1000);
    
    int executed = 0;
    for (int i = 0; i < num_instructions; i++) {
        if (execute_instruction(harness)) {
            executed++;
        } else {
            printf("Execution failed at instruction %d\n", i);
            break;
        }
    }
    
    clock_t end_time = clock();
    uint32_t end_cycles = harness->cycle_count;
    
    double execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    uint32_t total_cycles = end_cycles - start_cycles;
    
    printf("Executed %d instructions in %.3f seconds\n", executed, execution_time);
    printf("Total cycles: %u\n", total_cycles);
    printf("Performance: %.0f instructions/second\n", executed / execution_time);
    printf("Average cycles/instruction: %.2f\n", (double)total_cycles / executed);
    printf("CPU frequency equivalent: %.2f MHz (assuming 1 MHz = 1M cycles/sec)\n",
           total_cycles / execution_time / 1000000.0);
}

// Optimization analysis
static void run_optimization_analysis(void) {
    printf("\n=== OPTIMIZATION ANALYSIS ===\n");
    printf("fam65xx Core Architecture Analysis:\n");
    printf("- Hardware-accurate cycle timing\n");
    printf("- Memory callback system for flexibility\n");
    printf("- Comprehensive flag handling\n");
    printf("- Support for all 6502 addressing modes\n");
    printf("- ProcessorTests compatible execution model\n");
    
    printf("\nMemory Efficiency:\n");
    printf("- CPU state: %zu bytes\n", sizeof(fam65xx_t));
    printf("- Test harness: %zu bytes\n", sizeof(test_harness_t));
    printf("- Memory array: 64KB\n");
    
    printf("\nOptimization Opportunities:\n");
    printf("- Opcode dispatch optimization\n");
    printf("- Memory access pattern analysis\n");
    printf("- Cycle counting accuracy vs speed tradeoffs\n");
    printf("- Instruction pipeline simulation\n");
    
    printf("\nProcessorTests Integration:\n");
    printf("- JSON parsing and validation\n");
    printf("- Cycle-accurate execution verification\n");
    printf("- Register and memory state comparison\n");
    printf("- Comprehensive test coverage analysis\n");
}

// Run a single test case
static bool run_single_test(test_harness_t* harness, const processor_test_t* test) {
    g_results.total_tests++;
    
    if (harness->verbose) {
        printf("Running test: %s\n", test->name);
    }
    
    // Setup initial state
    setup_cpu_state(harness, &test->initial);
    
    // Get opcode for statistics - but verify what we're actually reading
    uint8_t opcode_from_memory = harness->memory[test->initial.pc];
    uint8_t opcode_from_pc = harness->memory[fam65xx_pc(&harness->cpu)];
    g_results.opcode_totals[opcode_from_memory]++;
    
    if (harness->verbose) {
        printf("  Expected PC: 0x%04X, Actual PC: 0x%04X\n", test->initial.pc, fam65xx_pc(&harness->cpu));
        printf("  Opcode at expected PC (0x%04X): 0x%02X\n", test->initial.pc, opcode_from_memory);
        printf("  Opcode at actual PC (0x%04X): 0x%02X\n", fam65xx_pc(&harness->cpu), opcode_from_pc);
    }
    
    uint8_t opcode = opcode_from_memory;  // Use expected for statistics
    
    // Execute instruction
    uint32_t cycles_before = harness->cycle_count;
    if (!execute_instruction(harness)) {
        if (harness->verbose) {
            printf("FAIL %s: Instruction execution failed\n", test->name);
        }
        g_results.failed_tests++;
        g_results.opcode_failures[opcode]++;
        return false;
    }
    
    uint32_t cycles_executed = harness->cycle_count - cycles_before;
    
    // Compare final state
    bool state_match = compare_cpu_state(harness, &test->final, test->name);
    bool cycle_match = true;
    
    // Check cycle count if provided
    if (test->final.has_cycles && cycles_executed != test->final.cycles) {
        if (harness->verbose) {
            printf("FAIL %s: Cycle mismatch - expected %u, got %u\n", 
                   test->name, test->final.cycles, cycles_executed);
        }
        cycle_match = false;
        g_results.cycle_mismatches++;
    }
    
    if (state_match && cycle_match) {
        g_results.passed_tests++;
        if (harness->verbose) {
            printf("PASS %s (cycles: %u)\n", test->name, cycles_executed);
        }
        return true;
    } else {
        g_results.failed_tests++;
        if (!state_match) g_results.state_mismatches++;
        g_results.opcode_failures[opcode]++;
        if (harness->verbose) {
            printf("FAIL %s: %s%s%s\n", test->name, 
                   state_match ? "" : "state ",
                   (state_match || cycle_match) ? "" : "and ",
                   cycle_match ? "" : "cycle");
        }
        return false;
    }
}

// Process a single JSON test file
static bool process_test_file(const char* filepath, bool verbose) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        printf("ERROR: Cannot open file %s\n", filepath);
        return false;
    }
    
    // Read entire file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* json_content = malloc(file_size + 1);
    if (!json_content) {
        fclose(file);
        return false;
    }
    
    fread(json_content, 1, file_size, file);
    json_content[file_size] = '\0';
    fclose(file);
    
    // Initialize test harness
    test_harness_t harness;
    init_test_harness(&harness, verbose);
    
    // Parse and run tests
    const char* pos = json_content;
    while (*pos) {
        // Skip whitespace
        pos = json_skip_whitespace(pos);
        if (!*pos) break;
        
        if (*pos == '[') {
            // Array of tests
            pos++; // Skip '['
            while (*pos) {
                pos = json_skip_whitespace(pos);
                if (*pos == ']') break;
                
                if (*pos == '{') {
                    // Find end of test object
                    const char* test_end = json_find_object_end(pos);
                    if (!test_end) break;
                    
                    // Extract test JSON
                    size_t test_len = test_end - pos + 1;
                    char* test_json = malloc(test_len + 1);
                    memcpy(test_json, pos, test_len);
                    test_json[test_len] = '\0';
                    
                    // Parse and run test
                    processor_test_t test;
                    if (json_parse_processor_test(test_json, &test)) {
                        run_single_test(&harness, &test);
                    } else if (verbose) {
                        printf("ERROR: Failed to parse test\n");
                    }
                    
                    free(test_json);
                    pos = test_end + 1;
                } else {
                    break;
                }
                
                // Skip comma
                pos = json_skip_whitespace(pos);
                if (*pos == ',') pos++;
            }
            break;
        } else if (*pos == '{') {
            // Single test
            processor_test_t test;
            if (json_parse_processor_test(json_content, &test)) {
                run_single_test(&harness, &test);
            } else if (verbose) {
                printf("ERROR: Failed to parse test\n");
            }
            break;
        } else {
            break;
        }
    }
    
    free(json_content);
    return true;
}

// Process directory recursively
static void process_directory(const char* dirpath, bool verbose) {
    DIR* dir = opendir(dirpath);
    if (!dir) {
        printf("ERROR: Cannot open directory %s\n", dirpath);
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);
        
        struct stat st;
        if (stat(filepath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                process_directory(filepath, verbose);
            } else if (S_ISREG(st.st_mode)) {
                // Check for .json extension
                char* ext = strrchr(entry->d_name, '.');
                if (ext && strcmp(ext, ".json") == 0) {
                    if (verbose) {
                        printf("Processing file: %s\n", filepath);
                    }
                    process_test_file(filepath, verbose);
                }
            }
        }
    }
    
    closedir(dir);
}

// Print usage information
static void print_usage(const char* program_name) {
    printf("fam65xx ProcessorTests Runner - Hardware-accurate 6502 verification\n");
    printf("Usage: %s [options] <test_file_or_directory|command>\n", program_name);
    printf("Options:\n");
    printf("  -v, --verbose      Enable verbose output\n");
    printf("  -d, --debug        Enable interactive debug session\n");
    printf("  -p, --perf [NUM]   Run performance benchmark (default: 1000 instructions)\n");
    printf("  -a, --analyze      Run optimization analysis\n");
    printf("  -h, --help         Show this help message\n");
    printf("\nCommands (instead of test files):\n");
    printf("  debug              Interactive debug session\n");
    printf("  perf [NUM]         Performance benchmark\n");
    printf("  analyze            Optimization analysis\n");
    printf("\nExamples:\n");
    printf("  %s processor_tests/6502/v1/\n", program_name);
    printf("  %s -v processor_tests/6502/v1/69.json\n", program_name);
    printf("  %s debug\n", program_name);
    printf("  %s -d -v single_test.json\n", program_name);
    printf("  %s perf 5000\n", program_name);
    printf("  %s analyze\n", program_name);
}

// Print detailed results
static void print_results(void) {
    printf("\n=== FAM65XX PROCESSOR TESTS RESULTS ===\n");
    printf("Total tests run: %u\n", g_results.total_tests);
    printf("Tests passed: %u\n", g_results.passed_tests);
    printf("Tests failed: %u\n", g_results.failed_tests);
    
    if (g_results.total_tests > 0) {
        double pass_rate = (double)g_results.passed_tests / g_results.total_tests * 100.0;
        printf("Pass rate: %.2f%%\n", pass_rate);
    }
    
    if (g_results.failed_tests > 0) {
        printf("\nFailure breakdown:\n");
        printf("State mismatches: %u\n", g_results.state_mismatches);
        printf("Cycle mismatches: %u\n", g_results.cycle_mismatches);
        
        printf("\nFailing opcodes:\n");
        int failing_opcodes = 0;
        for (int i = 0; i < 256; i++) {
            if (g_results.opcode_failures[i] > 0) {
                printf("0x%02X: %u/%u failed\n", i, g_results.opcode_failures[i], g_results.opcode_totals[i]);
                failing_opcodes++;
            }
        }
        printf("Total failing opcodes: %d\n", failing_opcodes);
    }
    
    if (g_results.passed_tests == g_results.total_tests) {
        printf("\n🎉 ALL TESTS PASSED - fam65xx is hardware-accurate! 🎉\n");
    } else {
        printf("\n❌ SOME TESTS FAILED - implementation differs from hardware\n");
    }
}

// Main function
int main(int argc, char* argv[]) {
    bool verbose = false;
    bool debug_mode = false;
    bool perf_mode = false;
    bool analyze_mode = false;
    int perf_instructions = 1000;
    char* test_path = NULL;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug_mode = true;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--perf") == 0) {
            perf_mode = true;
            // Check if next argument is a number
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                perf_instructions = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--analyze") == 0) {
            analyze_mode = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "debug") == 0) {
            debug_mode = true;
        } else if (strcmp(argv[i], "perf") == 0) {
            perf_mode = true;
            // Check if next argument is a number
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                perf_instructions = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "analyze") == 0) {
            analyze_mode = true;
        } else if (!test_path) {
            test_path = argv[i];
        }
    }
    
    printf("=== fam65xx ProcessorTests Runner (Enhanced) ===\n");
    
    // Handle special modes
    if (analyze_mode) {
        run_optimization_analysis();
        return 0;
    }
    
    if (perf_mode) {
        test_harness_t harness;
        init_test_harness(&harness, verbose);
        run_performance_benchmark(&harness, perf_instructions);
        return 0;
    }
    
    if (debug_mode && !test_path) {
        // Interactive debug mode without test file
        test_harness_t harness;
        init_test_harness(&harness, verbose);
        
        // Set up a simple test program for debugging
        harness.memory[0x1000] = 0xEA;  // NOP
        harness.memory[0x1001] = 0xA9;  // LDA #$42
        harness.memory[0x1002] = 0x42;
        harness.memory[0x1003] = 0x8D;  // STA $2000
        harness.memory[0x1004] = 0x00;
        harness.memory[0x1005] = 0x20;
        harness.memory[0x1006] = 0xAD;  // LDA $2000
        harness.memory[0x1007] = 0x00;
        harness.memory[0x1008] = 0x20;
        harness.memory[0x1009] = 0x4C;  // JMP $1000 (loop)
        harness.memory[0x100A] = 0x00;
        harness.memory[0x100B] = 0x10;
        
        fam65xx_set_pc(&harness.cpu, 0x1000);
        
        run_debug_session(&harness);
        return 0;
    }
    
    // Regular test execution
    if (!test_path) {
        printf("ERROR: No test file or directory specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    printf("Test path: %s\n", test_path);
    printf("Verbose: %s\n", verbose ? "enabled" : "disabled");
    printf("Debug mode: %s\n\n", debug_mode ? "enabled" : "disabled");
    
    clock_t start_time = clock();
    
    // Check if path is file or directory
    struct stat st;
    if (stat(test_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            process_directory(test_path, verbose);
        } else if (S_ISREG(st.st_mode)) {
            process_test_file(test_path, verbose);
            
            // If debug mode is enabled, start debug session after tests
            if (debug_mode) {
                test_harness_t debug_harness;
                init_test_harness(&debug_harness, verbose);
                printf("\n=== Starting debug session after test completion ===\n");
                run_debug_session(&debug_harness);
            }
        } else {
            printf("ERROR: Invalid path type: %s\n", test_path);
            return 1;
        }
    } else {
        printf("ERROR: Cannot access path: %s\n", test_path);
        return 1;
    }
    
    clock_t end_time = clock();
    double execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("\nExecution time: %.3f seconds\n", execution_time);
    print_results();
    
    return (g_results.passed_tests == g_results.total_tests) ? 0 : 1;
}