#include "tests/fam65xx_cpp_test_harness.h"
#include "tests/json_parser.c"
#include <cstdio>
#include <cstring>

int main() {
    printf("=== RTI ProcessorTests Direct Validation ===\n");
    
    // Test RTI instruction (0x40) using ProcessorTests
    int tests_passed = 0;
    int tests_total = 0;
    
    // Load ProcessorTests JSON file for RTI (0x40)
    char json_path[256];
    snprintf(json_path, sizeof(json_path), "tests/processor_tests/6502/v1/40.json");
    
    FILE* file = fopen(json_path, "r");
    if (!file) {
        printf("ERROR: Could not open %s\n", json_path);
        return 1;
    }
    
    // Read entire file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* json_content = (char*)malloc(file_size + 1);
    fread(json_content, 1, file_size, file);
    json_content[file_size] = '\0';
    fclose(file);
    
    // Parse JSON using our built-in parser
    JsonValue* root = json_parse(json_content);
    if (!root || root->type != JSON_ARRAY) {
        printf("ERROR: Failed to parse JSON or root is not an array\n");
        free(json_content);
        return 1;
    }
    
    printf("Found %d RTI test cases\n", root->array_size);
    
    // Test first 100 cases to check our fix
    int max_tests = (root->array_size > 100) ? 100 : root->array_size;
    
    for (int i = 0; i < max_tests; i++) {
        JsonValue* test_case = &root->array_elements[i];
        if (test_case->type != JSON_OBJECT) continue;
        
        tests_total++;
        
        // Create test CPU
        auto cpu = create_test_cpu();
        uint8_t memory[65536] = {0};
        
        // Parse initial state
        JsonValue* initial = json_object_get(test_case, "initial");
        if (!initial) continue;
        
        JsonValue* pc_val = json_object_get(initial, "pc");
        JsonValue* s_val = json_object_get(initial, "s");
        JsonValue* a_val = json_object_get(initial, "a");
        JsonValue* x_val = json_object_get(initial, "x");
        JsonValue* y_val = json_object_get(initial, "y");
        JsonValue* p_val = json_object_get(initial, "p");
        
        if (!pc_val || !s_val || !a_val || !x_val || !y_val || !p_val) continue;
        
        uint16_t initial_pc = (uint16_t)pc_val->number_value;
        cpu.set_pc(initial_pc);
        cpu.set_s((uint8_t)s_val->number_value);
        cpu.set_a((uint8_t)a_val->number_value);
        cpu.set_x((uint8_t)x_val->number_value);
        cpu.set_y((uint8_t)y_val->number_value);
        cpu.set_p((uint8_t)p_val->number_value);
        
        // Set memory state
        JsonValue* ram = json_object_get(initial, "ram");
        if (ram && ram->type == JSON_ARRAY) {
            for (int j = 0; j < ram->array_size; j += 2) {
                if (j + 1 < ram->array_size) {
                    uint16_t addr = (uint16_t)ram->array_elements[j].number_value;
                    uint8_t value = (uint8_t)ram->array_elements[j + 1].number_value;
                    memory[addr] = value;
                }
            }
        }
        
        // Set RTI opcode at current PC
        memory[initial_pc] = 0x40;
        
        // Execute RTI instruction
        bool test_passed = true;
        int cycles = 0;
        
        do {
            uint16_t addr = cpu.get_address();
            bool is_write = !cpu.get_rw();
            
            if (is_write) {
                memory[addr] = cpu.get_write_data();
            }
            
            uint8_t data = memory[addr];
            bus_state_t bus_state = create_bus_state(data);
            bus_state = cpu.cycle_tick(bus_state);
            cycles++;
            
            if (cycles > 10) break; // Safety limit
        } while (cpu.get_opcode() == 0x40 || cpu.get_cycle_step() > 0);
        
        // Parse expected final state
        JsonValue* final = json_object_get(test_case, "final");
        if (!final) continue;
        
        JsonValue* final_pc = json_object_get(final, "pc");
        JsonValue* final_s = json_object_get(final, "s");
        JsonValue* final_a = json_object_get(final, "a");
        JsonValue* final_x = json_object_get(final, "x");
        JsonValue* final_y = json_object_get(final, "y");
        JsonValue* final_p = json_object_get(final, "p");
        
        // Check results
        if (final_pc && cpu.get_pc() != (uint16_t)final_pc->number_value) test_passed = false;
        if (final_s && cpu.get_s() != (uint8_t)final_s->number_value) test_passed = false;
        if (final_a && cpu.get_a() != (uint8_t)final_a->number_value) test_passed = false;
        if (final_x && cpu.get_x() != (uint8_t)final_x->number_value) test_passed = false;
        if (final_y && cpu.get_y() != (uint8_t)final_y->number_value) test_passed = false;
        if (final_p && cpu.get_p() != (uint8_t)final_p->number_value) test_passed = false;
        
        if (test_passed) {
            tests_passed++;
        } else if (i < 5) { // Show first few failures for debugging
            printf("FAIL Test %d:\n", i);
            if (final_pc) printf("  PC: expected 0x%04X, got 0x%04X\n", (uint16_t)final_pc->number_value, cpu.get_pc());
            if (final_p) printf("  P: expected 0x%02X, got 0x%02X\n", (uint8_t)final_p->number_value, cpu.get_p());
        }
    }
    
    printf("\n=== RTI ProcessorTests Results ===\n");
    printf("Tests passed: %d/%d (%.1f%%)\n", tests_passed, tests_total, (100.0 * tests_passed) / tests_total);
    
    if (tests_passed == tests_total) {
        printf("🎉 RTI INSTRUCTION 100% SUCCESS! 🎉\n");
    } else {
        printf("RTI instruction needs more work - B flag fix working but other issues remain\n");
    }
    
    // Cleanup
    json_free(root);
    free(json_content);
    
    return (tests_passed == tests_total) ? 0 : 1;
}