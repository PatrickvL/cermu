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

// Failure tracking by opcode
static uint32_t opcode_failures[256] = {0};
static uint32_t opcode_totals[256] = {0};

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

uint8_t test_detached_read(void* context) {
    (void)context;
    return 0xFF;
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
                    .cycle_tick = test_cycle_tick,
                    .detached_read = test_detached_read
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
                    .cycle_tick = test_cycle_tick,
                    .detached_read = test_detached_read
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
                    .cycle_tick = test_cycle_tick,
                    .detached_read = test_detached_read
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
        if (verbose_output) {
            printf("FAIL %s: PC - expected 0x%04X, got 0x%04X\n", test_name, expected->pc, actual.pc);
        }
        passed = false;
    }
    if (actual.s != expected->s) {
        if (verbose_output) {
            printf("FAIL %s: SP - expected 0x%02X, got 0x%02X\n", test_name, expected->s, actual.s);
        }
        passed = false;
    }
    if (actual.a != expected->a) {
        if (verbose_output) {
            printf("FAIL %s: A - expected 0x%02X, got 0x%02X\n", test_name, expected->a, actual.a);
        }
        passed = false;
    }
    if (actual.x != expected->x) {
        if (verbose_output) {
            printf("FAIL %s: X - expected 0x%02X, got 0x%02X\n", test_name, expected->x, actual.x);
        }
        passed = false;
    }
    if (actual.y != expected->y) {
        if (verbose_output) {
            printf("FAIL %s: Y - expected 0x%02X, got 0x%02X\n", test_name, expected->y, actual.y);
        }
        passed = false;
    }
    if (actual.p != expected->p) {
        if (verbose_output) {
            printf("FAIL %s: P - expected 0x%02X, got 0x%02X\n", test_name, expected->p, actual.p);
        }
        passed = false;
    }
    if (expected->has_cycles && adjusted_cycles != expected->cycles) {
        if (verbose_output) {
            printf("FAIL %s: Cycles - expected %u, got %u (raw: %u)\n", 
                   test_name, expected->cycles, adjusted_cycles, cycle_count);
        }
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
                if (verbose_output) {
                    printf("FAIL %s: Memory[0x%04X] - expected 0x%02X, got 0x%02X\n", 
                           test_name, addr + j, expected_value, actual_value);
                }
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
    
    // Read the actual opcode from memory at the current PC using bus_read
    uint8_t opcode = test_read(NULL, test->initial.pc);
    opcode_totals[opcode]++;

    // If this is a JAM (KIL) opcode, assert NMI so the handler can break out for the test
    switch (opcode) {
        case 0x02: case 0x12: case 0x22: case 0x32:
        case 0x42: case 0x52: case 0x62: case 0x72:
        case 0x92: case 0xB2: case 0xD2: case 0xF2:
            // Set NMI line using the control interface
            if (instance->type == CPU_TYPE_MOS6502 || instance->type == CPU_TYPE_MOS6510 || instance->type == CPU_TYPE_NES6502) {
                if (instance->cpu) {
                    // The control interface is attached to the CPU instance
                    fam65xx_t* famcpu = (fam65xx_t*)instance->cpu;
                    famcpu->control_interface.set_lines(famcpu->control_interface.context, SYS_MASK_NMI);
                }
            }
            break;
    }

    if (verbose_output) {
        printf("  Opcode at PC 0x%04X: 0x%02X\n", test->initial.pc, opcode);
    }
    
    // Execute one instruction
    bool step_result = step_cpu(instance);
    if (!step_result) {
        if (verbose_output) {
            printf("FAIL %s: Instruction execution failed on %s (opcode 0x%02X)\n", 
                   test->name, instance->name, opcode);
        }
        opcode_failures[opcode]++;
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
            printf("PASS %s on %s (opcode 0x%02X)\n", test->name, instance->name, opcode);
        }
    } else {
        opcode_failures[opcode]++;
        if (verbose_output) {
            printf("FAIL %s: State mismatch on %s (opcode 0x%02X)\n", 
                   test->name, instance->name, opcode);
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
    
    const char* test_paths[256];  // Support up to 256 test paths
    int test_path_count = 0;
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
            if (test_path_count < 256) {
                test_paths[test_path_count++] = argv[i];
            } else {
                printf("ERROR: Too many test paths (max 256)\n");
                return 1;
            }
        }
    }
    
    if (test_path_count == 0) {
        printf("ERROR: No test file or directory specified\n");
        print_usage(argv[0]);
        return 1;
    }    
    // Detect CPU type from first test path
    cpu_type_t cpu_type = (forced_cpu_type != CPU_TYPE_UNKNOWN) ? 
                          forced_cpu_type : detect_cpu_type(test_paths[0]);
    
    // Create CPU instance
    cpu_instance_t cpu_instance;
    if (!create_cpu_instance(&cpu_instance, cpu_type)) {
        printf("ERROR: Could not create CPU instance for type %d\n", cpu_type);
        return 1;
    }
    
    printf("=== Unified ProcessorTests Runner for MOS6502 Family ===\n");
    printf("CPU Type: %s\n", cpu_instance.name);
    printf("Test paths: %d specified\n", test_path_count);
    printf("Verbose: %s\n", verbose_output ? "enabled" : "disabled");
    printf("\n");
    
    // Process all test paths
    for (int path_idx = 0; path_idx < test_path_count; path_idx++) {
        const char* test_path = test_paths[path_idx];
        printf("Processing test path: %s\n", test_path);
        
        // Check if path is file or directory
        struct stat statbuf;
        if (stat(test_path, &statbuf) != 0) {
            printf("ERROR: Could not access path: %s\n", test_path);
            continue;  // Continue with next path instead of exiting
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
        }
    }
      printf("\n=== TEST SUMMARY for %s ===\n", cpu_instance.name);
    printf("Total tests run: %u\n", total_tests);
    printf("Tests passed: %u\n", passed_tests);
    printf("Tests failed: %u\n", total_tests - passed_tests);
    
    // Print opcode failure summary
    if (total_tests - passed_tests > 0) {
        printf("\n=== FAILURE BREAKDOWN BY OPCODE ===\n");
        uint32_t failing_opcodes = 0;
        for (int i = 0; i < 256; i++) {
            if (opcode_failures[i] > 0) {
                printf("0x%02X: %u/%u failed", i, opcode_failures[i], opcode_totals[i]);
                
                // Add common instruction name if known
                const char* inst_name = "";
                switch (i) {
                    case 0x00: inst_name = " (BRK)"; break;
                    case 0x01: inst_name = " (ORA zpg,X)"; break;
                    case 0x05: inst_name = " (ORA zpg)"; break;
                    case 0x06: inst_name = " (ASL zpg)"; break;
                    case 0x08: inst_name = " (PHP)"; break;
                    case 0x09: inst_name = " (ORA #)"; break;
                    case 0x0A: inst_name = " (ASL A)"; break;
                    case 0x0D: inst_name = " (ORA abs)"; break;
                    case 0x0E: inst_name = " (ASL abs)"; break;
                    case 0x10: inst_name = " (BPL)"; break;
                    case 0x11: inst_name = " (ORA zpg,Y)"; break;
                    case 0x15: inst_name = " (ORA zpg,X)"; break;
                    case 0x16: inst_name = " (ASL zpg,X)"; break;
                    case 0x18: inst_name = " (CLC)"; break;
                    case 0x19: inst_name = " (ORA abs,Y)"; break;
                    case 0x1D: inst_name = " (ORA abs,X)"; break;
                    case 0x1E: inst_name = " (ASL abs,X)"; break;
                    case 0x20: inst_name = " (JSR)"; break;
                    case 0x21: inst_name = " (AND zpg,X)"; break;
                    case 0x24: inst_name = " (BIT zpg)"; break;
                    case 0x25: inst_name = " (AND zpg)"; break;
                    case 0x26: inst_name = " (ROL zpg)"; break;
                    case 0x28: inst_name = " (PLP)"; break;
                    case 0x29: inst_name = " (AND #)"; break;
                    case 0x2A: inst_name = " (ROL A)"; break;
                    case 0x2C: inst_name = " (BIT abs)"; break;
                    case 0x2D: inst_name = " (AND abs)"; break;
                    case 0x2E: inst_name = " (ROL abs)"; break;
                    case 0x30: inst_name = " (BMI)"; break;
                    case 0x31: inst_name = " (AND zpg,Y)"; break;
                    case 0x35: inst_name = " (AND zpg,X)"; break;
                    case 0x36: inst_name = " (ROL zpg,X)"; break;
                    case 0x38: inst_name = " (SEC)"; break;
                    case 0x39: inst_name = " (AND abs,Y)"; break;
                    case 0x3D: inst_name = " (AND abs,X)"; break;
                    case 0x3E: inst_name = " (ROL abs,X)"; break;
                    case 0x40: inst_name = " (RTI)"; break;
                    case 0x41: inst_name = " (EOR zpg,X)"; break;
                    case 0x45: inst_name = " (EOR zpg)"; break;
                    case 0x46: inst_name = " (LSR zpg)"; break;
                    case 0x48: inst_name = " (PHA)"; break;
                    case 0x49: inst_name = " (EOR #)"; break;
                    case 0x4A: inst_name = " (LSR A)"; break;
                    case 0x4C: inst_name = " (JMP abs)"; break;
                    case 0x4D: inst_name = " (EOR abs)"; break;
                    case 0x4E: inst_name = " (LSR abs)"; break;
                    case 0x50: inst_name = " (BVC)"; break;
                    case 0x51: inst_name = " (EOR zpg,Y)"; break;
                    case 0x55: inst_name = " (EOR zpg,X)"; break;
                    case 0x56: inst_name = " (LSR zpg,X)"; break;
                    case 0x58: inst_name = " (CLI)"; break;
                    case 0x59: inst_name = " (EOR abs,Y)"; break;
                    case 0x5D: inst_name = " (EOR abs,X)"; break;
                    case 0x5E: inst_name = " (LSR abs,X)"; break;
                    case 0x60: inst_name = " (RTS)"; break;
                    case 0x61: inst_name = " (ADC zpg,X)"; break;
                    case 0x65: inst_name = " (ADC zpg)"; break;
                    case 0x66: inst_name = " (ROR zpg)"; break;
                    case 0x68: inst_name = " (PLA)"; break;
                    case 0x69: inst_name = " (ADC #)"; break;
                    case 0x6A: inst_name = " (ROR A)"; break;
                    case 0x6C: inst_name = " (JMP ind)"; break;
                    case 0x6D: inst_name = " (ADC abs)"; break;
                    case 0x6E: inst_name = " (ROR abs)"; break;
                    case 0x70: inst_name = " (BVS)"; break;
                    case 0x71: inst_name = " (ADC zpg,Y)"; break;
                    case 0x75: inst_name = " (ADC zpg,X)"; break;
                    case 0x76: inst_name = " (ROR zpg,X)"; break;
                    case 0x78: inst_name = " (SEI)"; break;
                    case 0x79: inst_name = " (ADC abs,Y)"; break;
                    case 0x7D: inst_name = " (ADC abs,X)"; break;
                    case 0x7E: inst_name = " (ROR abs,X)"; break;
                    case 0x81: inst_name = " (STA zpg,X)"; break;
                    case 0x84: inst_name = " (STY zpg)"; break;
                    case 0x85: inst_name = " (STA zpg)"; break;
                    case 0x86: inst_name = " (STX zpg)"; break;
                    case 0x88: inst_name = " (DEY)"; break;
                    case 0x8A: inst_name = " (TXA)"; break;
                    case 0x8C: inst_name = " (STY abs)"; break;
                    case 0x8D: inst_name = " (STA abs)"; break;
                    case 0x8E: inst_name = " (STX abs)"; break;
                    case 0x90: inst_name = " (BCC)"; break;
                    case 0x91: inst_name = " (STA zpg,Y)"; break;
                    case 0x94: inst_name = " (STY zpg,X)"; break;
                    case 0x95: inst_name = " (STA zpg,X)"; break;
                    case 0x96: inst_name = " (STX zpg,Y)"; break;
                    case 0x98: inst_name = " (TYA)"; break;
                    case 0x99: inst_name = " (STA abs,Y)"; break;
                    case 0x9A: inst_name = " (TXS)"; break;
                    case 0x9D: inst_name = " (STA abs,X)"; break;
                    case 0xA0: inst_name = " (LDY #)"; break;
                    case 0xA1: inst_name = " (LDA zpg,X)"; break;
                    case 0xA2: inst_name = " (LDX #)"; break;
                    case 0xA4: inst_name = " (LDY zpg)"; break;
                    case 0xA5: inst_name = " (LDA zpg)"; break;
                    case 0xA6: inst_name = " (LDX zpg)"; break;
                    case 0xA8: inst_name = " (TAY)"; break;
                    case 0xA9: inst_name = " (LDA #)"; break;
                    case 0xAA: inst_name = " (TAX)"; break;
                    case 0xAC: inst_name = " (LDY abs)"; break;
                    case 0xAD: inst_name = " (LDA abs)"; break;
                    case 0xAE: inst_name = " (LDX abs)"; break;
                    case 0xB0: inst_name = " (BCS)"; break;
                    case 0xB1: inst_name = " (LDA zpg,Y)"; break;
                    case 0xB4: inst_name = " (LDY zpg,X)"; break;
                    case 0xB5: inst_name = " (LDA zpg,X)"; break;
                    case 0xB6: inst_name = " (LDX zpg,Y)"; break;
                    case 0xB8: inst_name = " (CLV)"; break;
                    case 0xB9: inst_name = " (LDA abs,Y)"; break;
                    case 0xBA: inst_name = " (TSX)"; break;
                    case 0xBC: inst_name = " (LDY abs,X)"; break;
                    case 0xBD: inst_name = " (LDA abs,X)"; break;
                    case 0xBE: inst_name = " (LDX abs,Y)"; break;
                    case 0xC0: inst_name = " (CPY #)"; break;
                    case 0xC1: inst_name = " (CMP zpg,X)"; break;
                    case 0xC4: inst_name = " (CPY zpg)"; break;
                    case 0xC5: inst_name = " (CMP zpg)"; break;
                    case 0xC6: inst_name = " (DEC zpg)"; break;
                    case 0xC8: inst_name = " (INY)"; break;
                    case 0xC9: inst_name = " (CMP #)"; break;
                    case 0xCA: inst_name = " (DEX)"; break;
                    case 0xCC: inst_name = " (CPY abs)"; break;
                    case 0xCD: inst_name = " (CMP abs)"; break;
                    case 0xCE: inst_name = " (DEC abs)"; break;
                    case 0xD0: inst_name = " (BNE)"; break;
                    case 0xD1: inst_name = " (CMP zpg,Y)"; break;
                    case 0xD5: inst_name = " (CMP zpg,X)"; break;
                    case 0xD6: inst_name = " (DEC zpg,X)"; break;
                    case 0xD8: inst_name = " (CLD)"; break;
                    case 0xD9: inst_name = " (CMP abs,Y)"; break;
                    case 0xDD: inst_name = " (CMP abs,X)"; break;
                    case 0xDE: inst_name = " (DEC abs,X)"; break;
                    case 0xE0: inst_name = " (CPX #)"; break;
                    case 0xE1: inst_name = " (SBC zpg,X)"; break;
                    case 0xE4: inst_name = " (CPX zpg)"; break;
                    case 0xE5: inst_name = " (SBC zpg)"; break;
                    case 0xE6: inst_name = " (INC zpg)"; break;
                    case 0xE8: inst_name = " (INX)"; break;
                    case 0xE9: inst_name = " (SBC #)"; break;
                    case 0xEA: inst_name = " (NOP)"; break;
                    case 0xEC: inst_name = " (CPX abs)"; break;
                    case 0xED: inst_name = " (SBC abs)"; break;
                    case 0xEE: inst_name = " (INC abs)"; break;
                    case 0xF0: inst_name = " (BEQ)"; break;
                    case 0xF1: inst_name = " (SBC zpg,Y)"; break;
                    case 0xF5: inst_name = " (SBC zpg,X)"; break;
                    case 0xF6: inst_name = " (INC zpg,X)"; break;
                    case 0xF8: inst_name = " (SED)"; break;
                    case 0xF9: inst_name = " (SBC abs,Y)"; break;
                    case 0xFD: inst_name = " (SBC abs,X)"; break;
                    case 0xFE: inst_name = " (INC abs,X)"; break;
                    // Illegal opcodes
                    case 0x87: inst_name = " (SAX zpg) *ILLEGAL*"; break;
                    case 0xA7: inst_name = " (LAX zpg) *ILLEGAL*"; break;
                    case 0xC7: inst_name = " (DCP zpg) *ILLEGAL*"; break;
                    case 0xE7: inst_name = " (ISC zpg) *ILLEGAL*"; break;
                    default: 
                        if (opcode_totals[i] > 0) inst_name = " *ILLEGAL*"; 
                        break;
                }
                
                printf("%s\n", inst_name);
                failing_opcodes++;
            }
        }
        printf("Total failing opcodes: %u\n", failing_opcodes);
    }
    
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
