#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <dirent.h>
#endif

#include "json_parser.h"
#include "../src/chip/cpu/mos6510/mos6510.h"
#include "../src/chip/cpu/mos6510/mos6510_cycles.h"
#include "../src/core/control_lines_interface.h"

// Test infrastructure
static uint8_t test_memory[65536];
static uint32_t cycle_count;
static uint32_t total_tests = 0;
static uint32_t passed_tests = 0;
static bool verbose_output = false;

// Memory interface for CPU
uint8_t test_read(void* context, uint16_t address) {
    (void)context;
    return test_memory[address];
}

void test_write(void* context, uint16_t address, uint8_t value) {
    (void)context;
    test_memory[address] = value;
}

uint8_t test_io_read(void* context) {
    (void)context;
    return 0xFF;
}

void test_io_write(void* context, uint8_t ddr, uint8_t port_data, uint8_t effective_output) {
    (void)context; (void)ddr; (void)port_data; (void)effective_output;
}

void test_cycle_tick(void* context) {
    (void)context;
    cycle_count++;
}

static uint32_t dummy_get_control_lines(void *context) {
    (void)context;
    return (1U << 5);  // RDY active, no IRQ/NMI
}

static void dummy_set_control_lines(void *context, uint32_t lines) {
    (void)context; (void)lines;
}

static const control_lines_interface_t control_interface = {
    .get_lines = dummy_get_control_lines,
    .set_lines = dummy_set_control_lines,
    .context = NULL
};

// Setup CPU with initial state
mos6510_t* setup_cpu_from_state(const cpu_state_t* initial_state) {
    mos6510_t* cpu = (mos6510_t*)mos6510_descriptor.create(&mos6510_descriptor);
    if (!cpu) return NULL;
    
    bus_cycle_ops_t bus_ops = {
        .context = NULL,
        .bus_read = test_read,
        .bus_write = test_write,
        .cycle_tick = test_cycle_tick
    };
    
    mos6510_io_port_interface_t io_interface = {
        .context = NULL,
        .read_external_pins = test_io_read,
        .output_pins_changed = test_io_write
    };
    
    mos6510_attach_bus_interface(cpu, &bus_ops);
    mos6510_attach_io_interface(cpu, &io_interface);
    mos6510_attach_control_lines_interface(cpu, &control_interface);
    
    // Set initial CPU state
    cpu->pc = initial_state->pc;
    cpu->sp = initial_state->s;
    cpu->a = initial_state->a;
    cpu->x = initial_state->x;
    cpu->y = initial_state->y;
    cpu->p = initial_state->p;
    
    cycle_count = 0;
    return cpu;
}

// Setup memory from RAM array
void setup_memory_from_state(const cpu_state_t* state) {
    // Clear memory
    memset(test_memory, 0, sizeof(test_memory));
    
    // Setup RAM entries
    for (uint8_t i = 0; i < state->ram_count; i++) {
        uint16_t addr = state->ram[i].address;
        for (uint8_t j = 0; j < state->ram[i].byte_count; j++) {
            test_memory[addr + j] = state->ram[i].bytes[j];
        }
    }
}

// Compare CPU state with expected final state
bool compare_cpu_state(const mos6510_t* cpu, const cpu_state_t* expected, const char* test_name) {
    bool passed = true;
    
    // Adjust cycle count for intercept mechanism overhead
    uint32_t adjusted_cycles = (cycle_count > 0) ? cycle_count - 1 : 0;
    
    if (cpu->pc != expected->pc) {
        printf("FAIL %s: PC - expected 0x%04X, got 0x%04X\n", test_name, expected->pc, cpu->pc);
        passed = false;
    }
    if (cpu->sp != expected->s) {
        printf("FAIL %s: SP - expected 0x%02X, got 0x%02X\n", test_name, expected->s, cpu->sp);
        passed = false;
    }
    if (cpu->a != expected->a) {
        printf("FAIL %s: A - expected 0x%02X, got 0x%02X\n", test_name, expected->a, cpu->a);
        passed = false;
    }
    if (cpu->x != expected->x) {
        printf("FAIL %s: X - expected 0x%02X, got 0x%02X\n", test_name, expected->x, cpu->x);
        passed = false;
    }
    if (cpu->y != expected->y) {
        printf("FAIL %s: Y - expected 0x%02X, got 0x%02X\n", test_name, expected->y, cpu->y);
        passed = false;
    }
    if (cpu->p != expected->p) {
        printf("FAIL %s: P - expected 0x%02X, got 0x%02X\n", test_name, expected->p, cpu->p);
        passed = false;
    }
    if (expected->has_cycles && adjusted_cycles != expected->cycles) {
        printf("FAIL %s: Cycles - expected %u, got %u (raw: %u)\n", 
               test_name, expected->cycles, adjusted_cycles, cycle_count);
        passed = false;
    }
    
    return passed;
}

// Compare memory state with expected final RAM
bool compare_memory_state(const cpu_state_t* expected, const char* test_name) {
    bool passed = true;
    
    for (uint8_t i = 0; i < expected->ram_count; i++) {
        uint16_t addr = expected->ram[i].address;
        for (uint8_t j = 0; j < expected->ram[i].byte_count; j++) {
            uint8_t expected_value = expected->ram[i].bytes[j];
            uint8_t actual_value = test_memory[addr + j];
            
            if (actual_value != expected_value) {
                printf("FAIL %s: Memory[0x%04X] - expected 0x%02X, got 0x%02X\n", 
                       test_name, addr + j, expected_value, actual_value);
                passed = false;
            }
        }
    }
    
    return passed;
}

// Run a single processor test
bool run_processor_test(const processor_test_t* test) {
    total_tests++;
    
    if (verbose_output) {
        printf("Running test: %s\n", test->name);
    }
    
    // Setup memory from initial state
    setup_memory_from_state(&test->initial);
    
    // Setup CPU with initial state
    mos6510_t* cpu = setup_cpu_from_state(&test->initial);
    if (!cpu) {
        printf("FAIL %s: Could not create CPU\n", test->name);
        return false;
    }
    
    // Execute one instruction
    bool step_result = mos6510_step(cpu);
    if (!step_result) {
        printf("FAIL %s: Instruction execution failed\n", test->name);
        mos6510_descriptor.destroy(cpu);
        return false;
    }
    
    // Compare CPU state
    bool cpu_state_ok = compare_cpu_state(cpu, &test->final, test->name);
    
    // Compare memory state
    bool memory_state_ok = compare_memory_state(&test->final, test->name);
    
    bool passed = cpu_state_ok && memory_state_ok;
    
    if (passed) {
        passed_tests++;
        if (verbose_output) {
            printf("PASS %s\n", test->name);
        }
    }
    
    mos6510_descriptor.destroy(cpu);
    return passed;
}

// Load and run tests from a JSON file
bool run_tests_from_file(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        printf("ERROR: Could not open file: %s\n", filepath);
        return false;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Read entire file
    char* json_content = malloc(file_size + 1);
    if (!json_content) {
        printf("ERROR: Could not allocate memory for file: %s\n", filepath);
        fclose(file);
        return false;
    }
    
    size_t read_size = fread(json_content, 1, file_size, file);
    json_content[read_size] = '\0';
    fclose(file);
    
    // Check if this is an array of tests or a single test
    char* pos = json_content;
    pos = (char*)json_skip_whitespace(pos);
    
    if (*pos == '[') {
        // Array of tests
        pos++; // Skip opening bracket
        
        while (*pos) {
            pos = (char*)json_skip_whitespace(pos);
            if (*pos == ']') break;
            
            if (*pos == '{') {
                // Find the end of this test object
                const char* test_end = json_find_object_end(pos);
                if (!test_end) break;
                
                // Extract this test
                size_t test_len = test_end - pos + 1;
                char* test_json = malloc(test_len + 1);
                if (test_json) {
                    strncpy(test_json, pos, test_len);
                    test_json[test_len] = '\0';
                    
                    // Parse and run the test
                    processor_test_t test;
                    if (json_parse_processor_test(test_json, &test)) {
                        run_processor_test(&test);
                    } else {
                        printf("ERROR: Failed to parse test in file: %s\n", filepath);
                    }
                    
                    free(test_json);
                }
                
                pos = (char*)test_end + 1;
            } else {
                break;
            }
            
            // Skip comma if present
            pos = (char*)json_skip_whitespace(pos);
            if (*pos == ',') pos++;
        }
    } else {
        // Single test
        processor_test_t test;
        if (json_parse_processor_test(json_content, &test)) {
            run_processor_test(&test);
        } else {
            printf("ERROR: Failed to parse test in file: %s\n", filepath);
        }
    }
    
    free(json_content);
    return true;
}

#ifdef _WIN32
// Windows directory traversal
void run_tests_from_directory(const char* dirpath) {
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", dirpath);
    
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("ERROR: Could not open directory: %s\n", dirpath);
        return;
    }
    
    do {
        if (find_data.cFileName[0] == '.') continue; // Skip . and ..
        
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s\\%s", dirpath, find_data.cFileName);
        
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recursively process subdirectory
            run_tests_from_directory(filepath);
        } else {
            // Check if it's a JSON file
            size_t len = strlen(find_data.cFileName);
            if (len > 5 && strcmp(find_data.cFileName + len - 5, ".json") == 0) {
                printf("Processing file: %s\n", filepath);
                run_tests_from_file(filepath);
            }
        }
    } while (FindNextFileA(hFind, &find_data));
    
    FindClose(hFind);
}
#else
// Unix/Linux directory traversal
void run_tests_from_directory(const char* dirpath) {
    DIR* dir = opendir(dirpath);
    if (!dir) {
        printf("ERROR: Could not open directory: %s\n", dirpath);
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; // Skip hidden files
        
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);
        
        struct stat statbuf;
        if (stat(filepath, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                // Recursively process subdirectory
                run_tests_from_directory(filepath);
            } else if (S_ISREG(statbuf.st_mode)) {
                // Check if it's a JSON file
                size_t len = strlen(entry->d_name);
                if (len > 5 && strcmp(entry->d_name + len - 5, ".json") == 0) {
                    printf("Processing file: %s\n", filepath);
                    run_tests_from_file(filepath);
                }
            }
        }
    }
    
    closedir(dir);
}
#endif

void print_usage(const char* program_name) {
    printf("ProcessorTests Runner for MOS6510 CPU\n");
    printf("Usage: %s [options] <test_file_or_directory>\n", program_name);
    printf("Options:\n");
    printf("  -v, --verbose    Enable verbose output\n");
    printf("  -h, --help       Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s processor_tests/6502/v1/\n", program_name);
    printf("  %s -v processor_tests/6502/v1/00.json\n", program_name);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* test_path = NULL;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose_output = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            test_path = argv[i];
        }
    }
    
    if (!test_path) {
        printf("ERROR: No test file or directory specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    printf("=== ProcessorTests Runner for MOS6510 CPU ===\n");
    printf("Using ground truth data from TomHarte/ProcessorTests\n");
    printf("Test path: %s\n", test_path);
    printf("Verbose: %s\n", verbose_output ? "enabled" : "disabled");
    printf("\n");
    
    // Check if path is file or directory
    struct stat statbuf;
    if (stat(test_path, &statbuf) != 0) {
        printf("ERROR: Could not access path: %s\n", test_path);
        return 1;
    }
    
#ifdef _WIN32
    if (statbuf.st_mode & _S_IFDIR) {
#else
    if (S_ISDIR(statbuf.st_mode)) {
#endif
        run_tests_from_directory(test_path);
#ifdef _WIN32
    } else if (statbuf.st_mode & _S_IFREG) {
#else
    } else if (S_ISREG(statbuf.st_mode)) {
#endif
        run_tests_from_file(test_path);
    } else {
        printf("ERROR: Invalid path type: %s\n", test_path);
        return 1;
    }
    
    printf("\n=== TEST SUMMARY ===\n");
    printf("Total tests run: %u\n", total_tests);
    printf("Tests passed: %u\n", passed_tests);
    printf("Tests failed: %u\n", total_tests - passed_tests);
    
    if (total_tests == 0) {
        printf("\nNo tests found in specified path!\n");
        return 1;
    } else if (passed_tests == total_tests) {
        printf("\nALL TESTS PASSED - MOS6510 CPU matches ProcessorTests ground truth!\n");
        return 0;
    } else {
        printf("\nSOME TESTS FAILED - CPU implementation differs from ground truth\n");
        return 1;
    }
}