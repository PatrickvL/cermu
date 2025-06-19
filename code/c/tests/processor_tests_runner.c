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
#include "../src/chip/cpu/mos6502/mos6502.h"
#include "../src/chip/cpu/mos6510/mos6510.h"
#include "../src/chip/cpu/nes6502/nes6502.h"
#include "../src/core/control_lines_interface.h"

// ============================================================================
// CPU TYPE DETECTION AND MANAGEMENT
// ============================================================================

typedef enum {
    CPU_TYPE_UNKNOWN = 0,
    CPU_TYPE_MOS6502,
    CPU_TYPE_MOS6510, 
    CPU_TYPE_NES6502
} cpu_type_t;

typedef struct {
    cpu_type_t type;
    void* cpu;
    chip_descriptor_t* descriptor;
    const char* name;
} cpu_instance_t;

// Test infrastructure
static uint8_t test_memory[65536];
static uint32_t cycle_count;
static uint32_t total_tests = 0;
static uint32_t passed_tests = 0;
static bool verbose_output = false;

// ============================================================================
// MEMORY AND CONTROL INTERFACES (SHARED)
// ============================================================================

uint8_t test_read(void* context, uint16_t address) {
    (void)context;
    return test_memory[address];
}

void test_write(void* context, uint16_t address, uint8_t value) {
    (void)context;
    test_memory[address] = value;
}

uint8_t test_io_read(void* context, uint8_t port_value, uint8_t ddr) {
    (void)context; (void)port_value; (void)ddr;
    return 0xFF;
}

void test_io_write(void* context, uint8_t port_value, uint8_t ddr) {
    (void)context; (void)port_value; (void)ddr;
}

void test_cycle_tick(void* context) {
    (void)context;
    cycle_count++;
}

static uint32_t dummy_get_control_lines(void *context) {
    (void)context;
    return (1U << 29);  // RDY active (bit 29), no IRQ/NMI
}

static void dummy_set_control_lines(void *context, uint32_t lines) {
    (void)context; (void)lines;
}

static const control_lines_interface_t control_interface = {
    .get_lines = dummy_get_control_lines,
    .set_lines = dummy_set_control_lines,
    .context = NULL
};

// ============================================================================
// CPU TYPE DETECTION
// ============================================================================

cpu_type_t detect_cpu_type(const char* test_path) {
    // Detect CPU type from test path
    if (strstr(test_path, "6502") && !strstr(test_path, "nes6502")) {
        return CPU_TYPE_MOS6502;  // Standard 6502 with decimal mode
    } else if (strstr(test_path, "nes6502")) {
        return CPU_TYPE_NES6502;  // NES 6502 without decimal mode
    } else if (strstr(test_path, "6510")) {
        return CPU_TYPE_MOS6510;  // C64 6510 without decimal mode
    }
    
    // Default fallback based on folder structure analysis
    printf("WARNING: Could not detect CPU type from path '%s', using MOS6502\n", test_path);
    return CPU_TYPE_MOS6502;
}

// ============================================================================
// CPU INSTANCE MANAGEMENT
// ============================================================================

bool create_cpu_instance(cpu_instance_t* instance, cpu_type_t type) {
    instance->type = type;
    
    switch (type) {
        case CPU_TYPE_MOS6502:
            instance->cpu = mos6502_descriptor.create(&mos6502_descriptor);
            instance->descriptor = &mos6502_descriptor;
            instance->name = "MOS6502";
            
            if (instance->cpu) {
                bus_cycle_ops_t bus_ops = {
                    .context = NULL,
                    .bus_read = test_read,
                    .bus_write = test_write,
                    .cycle_tick = test_cycle_tick
                };
                mos6502_attach_bus((mos6502_t*)instance->cpu, &bus_ops);
                mos6502_attach_control_lines((mos6502_t*)instance->cpu, &control_interface);
            }
            break;
            
        case CPU_TYPE_MOS6510:
            instance->cpu = mos6510_descriptor.create(&mos6510_descriptor);
            instance->descriptor = &mos6510_descriptor;
            instance->name = "MOS6510";
            
            if (instance->cpu) {
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
                mos6510_attach_bus_interface((mos6510_t*)instance->cpu, &bus_ops);
                mos6510_attach_io_interface((mos6510_t*)instance->cpu, &io_interface);
                mos6510_attach_control_lines_interface((mos6510_t*)instance->cpu, &control_interface);
            }
            break;
            
        case CPU_TYPE_NES6502:
            instance->cpu = nes6502_descriptor.create(&nes6502_descriptor);
            instance->descriptor = &nes6502_descriptor;
            instance->name = "NES6502";
            
            if (instance->cpu) {
                bus_cycle_ops_t bus_ops = {
                    .context = NULL,
                    .bus_read = test_read,
                    .bus_write = test_write,
                    .cycle_tick = test_cycle_tick
                };
                nes6502_attach_bus((nes6502_t*)instance->cpu, &bus_ops);
                nes6502_attach_control_lines((nes6502_t*)instance->cpu, &control_interface);
            }
            break;
            
        default:
            return false;
    }
    
    return instance->cpu != NULL;
}

void destroy_cpu_instance(cpu_instance_t* instance) {
    if (instance->cpu && instance->descriptor) {
        instance->descriptor->destroy(instance->cpu);
        instance->cpu = NULL;
    }
}

// ============================================================================
// CPU STATE MANAGEMENT (POLYMORPHIC)
// ============================================================================

void set_cpu_state(cpu_instance_t* instance, const cpu_state_t* state) {
    // All CPUs use the same fam65xx_t base structure - access directly
    fam65xx_t* cpu_base = NULL;
    
    switch (instance->type) {
        case CPU_TYPE_MOS6502: {
            mos6502_t* cpu = (mos6502_t*)instance->cpu;
            cpu_base = &cpu->base;
            break;
        }
        case CPU_TYPE_MOS6510: {
            mos6510_t* cpu = (mos6510_t*)instance->cpu;
            cpu_base = &cpu->base;
            break;
        }        case CPU_TYPE_NES6502: {
            nes6502_t* cpu = (nes6502_t*)instance->cpu;
            cpu_base = &cpu->base;
            break;
        }
        default:
            break;
    }
    
    // Direct struct member access - all CPUs share the same base layout
    if (cpu_base) {
        cpu_base->pc = state->pc;
        cpu_base->a = state->a;
        cpu_base->x = state->x;
        cpu_base->y = state->y;
        cpu_base->sp = state->s;
        cpu_base->p = state->p;
    }
}

void get_cpu_state(cpu_instance_t* instance, cpu_state_t* state) {
    // All CPUs use the same fam65xx_t base structure - access directly
    fam65xx_t* cpu_base = NULL;
    
    switch (instance->type) {
        case CPU_TYPE_MOS6502: {
            mos6502_t* cpu = (mos6502_t*)instance->cpu;
            cpu_base = &cpu->base;
            break;
        }
        case CPU_TYPE_MOS6510: {
            mos6510_t* cpu = (mos6510_t*)instance->cpu;
            cpu_base = &cpu->base;
            break;
        }
        case CPU_TYPE_NES6502: {
            nes6502_t* cpu = (nes6502_t*)instance->cpu;
            cpu_base = &cpu->base;
            break;
        }
        default:
            break;
    }
    
    // Direct struct member access - all CPUs share the same base layout
    if (cpu_base) {
        state->pc = cpu_base->pc;
        state->a = cpu_base->a;
        state->x = cpu_base->x;
        state->y = cpu_base->y;
        state->s = cpu_base->sp;
        state->p = cpu_base->p;
    }
}

bool step_cpu(cpu_instance_t* instance) {
    switch (instance->type) {
        case CPU_TYPE_MOS6502:
            return mos6502_step((mos6502_t*)instance->cpu);
        case CPU_TYPE_MOS6510:
            return mos6510_step((mos6510_t*)instance->cpu);
        case CPU_TYPE_NES6502:
            return nes6502_step((nes6502_t*)instance->cpu);
        default:
            return false;
    }
}

// ============================================================================
// TEST EXECUTION
// ============================================================================

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

bool compare_cpu_state(cpu_instance_t* instance, const cpu_state_t* expected, const char* test_name) {
    bool passed = true;
    cpu_state_t actual;
    get_cpu_state(instance, &actual);
    
    // Adjust cycle count for intercept mechanism overhead
    uint32_t adjusted_cycles = (cycle_count > 0) ? cycle_count - 1 : 0;
    
    if (actual.pc != expected->pc) {
        printf("FAIL %s: PC - expected 0x%04X, got 0x%04X\n", test_name, expected->pc, actual.pc);
        passed = false;
    }
    if (actual.s != expected->s) {
        printf("FAIL %s: SP - expected 0x%02X, got 0x%02X\n", test_name, expected->s, actual.s);
        passed = false;
    }
    if (actual.a != expected->a) {
        printf("FAIL %s: A - expected 0x%02X, got 0x%02X\n", test_name, expected->a, actual.a);
        passed = false;
    }
    if (actual.x != expected->x) {
        printf("FAIL %s: X - expected 0x%02X, got 0x%02X\n", test_name, expected->x, actual.x);
        passed = false;
    }
    if (actual.y != expected->y) {
        printf("FAIL %s: Y - expected 0x%02X, got 0x%02X\n", test_name, expected->y, actual.y);
        passed = false;
    }
    if (actual.p != expected->p) {
        printf("FAIL %s: P - expected 0x%02X, got 0x%02X\n", test_name, expected->p, actual.p);
        passed = false;
    }
    if (expected->has_cycles && adjusted_cycles != expected->cycles) {
        printf("FAIL %s: Cycles - expected %u, got %u (raw: %u)\n", 
               test_name, expected->cycles, adjusted_cycles, cycle_count);
        passed = false;
    }
    
    return passed;
}

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

bool run_processor_test(cpu_instance_t* instance, const processor_test_t* test) {
    total_tests++;
    
    if (verbose_output) {
        printf("Running test: %s on %s\n", test->name, instance->name);
    }
    
    // Setup memory from initial state
    setup_memory_from_state(&test->initial);
    
    // Setup CPU with initial state
    set_cpu_state(instance, &test->initial);
    cycle_count = 0;
    
    // Execute one instruction
    bool step_result = step_cpu(instance);
    if (!step_result) {
        printf("FAIL %s: Instruction execution failed on %s\n", test->name, instance->name);
        return false;
    }
    
    // Compare CPU state
    bool cpu_state_ok = compare_cpu_state(instance, &test->final, test->name);
    
    // Compare memory state
    bool memory_state_ok = compare_memory_state(&test->final, test->name);
    
    bool passed = cpu_state_ok && memory_state_ok;
    
    if (passed) {
        passed_tests++;
        if (verbose_output) {
            printf("PASS %s on %s\n", test->name, instance->name);
        }
    }
    
    return passed;
}

// ============================================================================
// FILE PROCESSING (REUSED FROM ORIGINAL)
// ============================================================================

bool run_tests_from_file(cpu_instance_t* instance, const char* filepath) {
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
                        run_processor_test(instance, &test);
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
            run_processor_test(instance, &test);
        } else {
            printf("ERROR: Failed to parse test in file: %s\n", filepath);
        }
    }
    
    free(json_content);
    return true;
}

#ifdef _WIN32
void run_tests_from_directory(cpu_instance_t* instance, const char* dirpath) {
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
            run_tests_from_directory(instance, filepath);
        } else {
            // Check if it's a JSON file
            size_t len = strlen(find_data.cFileName);
            if (len > 5 && strcmp(find_data.cFileName + len - 5, ".json") == 0) {
                printf("Processing file: %s\n", filepath);
                run_tests_from_file(instance, filepath);
            }
        }
    } while (FindNextFileA(hFind, &find_data));
    
    FindClose(hFind);
}
#else
void run_tests_from_directory(cpu_instance_t* instance, const char* dirpath) {
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
                run_tests_from_directory(instance, filepath);
            } else if (S_ISREG(statbuf.st_mode)) {
                // Check if it's a JSON file
                size_t len = strlen(entry->d_name);
                if (len > 5 && strcmp(entry->d_name + len - 5, ".json") == 0) {
                    printf("Processing file: %s\n", filepath);
                    run_tests_from_file(instance, filepath);
                }
            }
        }
    }
    
    closedir(dir);
}
#endif

// ============================================================================
// MAIN PROGRAM
// ============================================================================

void print_usage(const char* program_name) {
    printf("Unified ProcessorTests Runner for MOS6502 Family CPUs\n");
    printf("Usage: %s [options] <test_file_or_directory>\n", program_name);
    printf("Options:\n");
    printf("  -v, --verbose    Enable verbose output\n");
    printf("  -h, --help       Show this help message\n");
    printf("  --cpu <type>     Force CPU type (mos6502, mos6510, nes6502)\n");
    printf("\n");
    printf("Supported CPU Types:\n");
    printf("  MOS6502   - Standard 6502 with decimal mode\n");
    printf("  MOS6510   - C64 6510 without decimal mode\n");
    printf("  NES6502   - NES 6502 without decimal mode\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s processor_tests/6502/v1/\n", program_name);
    printf("  %s --cpu nes6502 processor_tests/nes6502/v1/69.json\n", program_name);
    printf("  %s -v processor_tests/6502/v1/69.json\n", program_name);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* test_path = NULL;
    cpu_type_t forced_cpu_type = CPU_TYPE_UNKNOWN;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose_output = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--cpu") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "mos6502") == 0) {
                forced_cpu_type = CPU_TYPE_MOS6502;
            } else if (strcmp(argv[i], "mos6510") == 0) {
                forced_cpu_type = CPU_TYPE_MOS6510;
            } else if (strcmp(argv[i], "nes6502") == 0) {
                forced_cpu_type = CPU_TYPE_NES6502;
            } else {
                printf("ERROR: Unknown CPU type: %s\n", argv[i]);
                return 1;
            }
        } else {
            test_path = argv[i];
        }
    }
    
    if (!test_path) {
        printf("ERROR: No test file or directory specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Detect CPU type
    cpu_type_t cpu_type = (forced_cpu_type != CPU_TYPE_UNKNOWN) ? 
                          forced_cpu_type : detect_cpu_type(test_path);
    
    // Create CPU instance
    cpu_instance_t cpu_instance;
    if (!create_cpu_instance(&cpu_instance, cpu_type)) {
        printf("ERROR: Could not create CPU instance for type %d\n", cpu_type);
        return 1;
    }
    
    printf("=== Unified ProcessorTests Runner for MOS6502 Family ===\n");
    printf("CPU Type: %s\n", cpu_instance.name);
    printf("Test path: %s\n", test_path);
    printf("Verbose: %s\n", verbose_output ? "enabled" : "disabled");
    printf("\n");
    
    // Check if path is file or directory
    struct stat statbuf;
    if (stat(test_path, &statbuf) != 0) {
        printf("ERROR: Could not access path: %s\n", test_path);
        destroy_cpu_instance(&cpu_instance);
        return 1;
    }
    
#ifdef _WIN32
    if (statbuf.st_mode & _S_IFDIR) {
#else
    if (S_ISDIR(statbuf.st_mode)) {
#endif
        run_tests_from_directory(&cpu_instance, test_path);
#ifdef _WIN32
    } else if (statbuf.st_mode & _S_IFREG) {
#else
    } else if (S_ISREG(statbuf.st_mode)) {
#endif
        run_tests_from_file(&cpu_instance, test_path);
    } else {
        printf("ERROR: Invalid path type: %s\n", test_path);
        destroy_cpu_instance(&cpu_instance);
        return 1;
    }
    
    printf("\n=== TEST SUMMARY for %s ===\n", cpu_instance.name);
    printf("Total tests run: %u\n", total_tests);
    printf("Tests passed: %u\n", passed_tests);
    printf("Tests failed: %u\n", total_tests - passed_tests);
    
    destroy_cpu_instance(&cpu_instance);
    
    if (total_tests == 0) {
        printf("\nNo tests found in specified path!\n");
        return 1;
    } else if (passed_tests == total_tests) {
        printf("\nALL TESTS PASSED - %s matches ProcessorTests ground truth!\n", cpu_instance.name);
        return 0;
    } else {
        printf("\nSOME TESTS FAILED - %s implementation differs from ground truth\n", cpu_instance.name);
        return 1;
    }
}
