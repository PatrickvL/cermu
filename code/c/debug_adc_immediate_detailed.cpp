#include <iostream>
#include <iomanip>
#include <fstream>
#include "json_parser.h"
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== ADC Immediate (0x69) Detailed Analysis ===" << std::endl;
    
    // Load the test file for ADC immediate
    std::ifstream file("tests/processor_tests/6502/v1/69.json");
    if (!file.is_open()) {
        std::cerr << "Cannot open 69.json test file" << std::endl;
        return 1;
    }
    
    std::string json_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    // Parse JSON
    json_value* root = json_parse(json_content.c_str(), json_content.length());
    if (!root || root->type != json_array) {
        std::cerr << "Invalid JSON format" << std::endl;
        return 1;
    }
    
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;
    
    // Analyze first 10 failing cases in detail
    int detailed_count = 0;
    
    for (unsigned int i = 0; i < root->u.array.length && detailed_count < 10; i++) {
        json_value* test_case = root->u.array.values[i];
        if (!test_case || test_case->type != json_object) continue;
        
        total_tests++;
        
        // Extract test case data
        json_value* initial_val = nullptr;
        json_value* final_val = nullptr;
        json_value* cycles_val = nullptr;
        
        for (unsigned int j = 0; j < test_case->u.object.length; j++) {
            if (strcmp(test_case->u.object.values[j].name, "initial") == 0) {
                initial_val = test_case->u.object.values[j].value;
            }
            if (strcmp(test_case->u.object.values[j].name, "final") == 0) {
                final_val = test_case->u.object.values[j].value;
            }
            if (strcmp(test_case->u.object.values[j].name, "cycles") == 0) {
                cycles_val = test_case->u.object.values[j].value;
            }
        }
        
        if (!initial_val || !final_val || !cycles_val) continue;
        
        // Setup CPU with initial state
        using CpuType = fam65xx<cpu_config<CpuVariant::NMOS_6502>>;
        CpuType cpu;
        cpu.reset();
        
        // Extract initial state
        for (unsigned int j = 0; j < initial_val->u.object.length; j++) {
            const char* name = initial_val->u.object.values[j].name;
            json_value* value = initial_val->u.object.values[j].value;
            
            if (strcmp(name, "pc") == 0 && value->type == json_integer) {
                cpu.registers[static_cast<int>(Reg::PCL)] = value->u.integer & 0xFF;
                cpu.registers[static_cast<int>(Reg::PCH)] = (value->u.integer >> 8) & 0xFF;
            }
            else if (strcmp(name, "s") == 0 && value->type == json_integer) {
                cpu.registers[static_cast<int>(Reg::S)] = value->u.integer;
            }
            else if (strcmp(name, "a") == 0 && value->type == json_integer) {
                cpu.registers[static_cast<int>(Reg::A)] = value->u.integer;
            }
            else if (strcmp(name, "x") == 0 && value->type == json_integer) {
                cpu.registers[static_cast<int>(Reg::X)] = value->u.integer;
            }
            else if (strcmp(name, "y") == 0 && value->type == json_integer) {
                cpu.registers[static_cast<int>(Reg::Y)] = value->u.integer;
            }
            else if (strcmp(name, "p") == 0 && value->type == json_integer) {
                cpu.registers[static_cast<int>(Reg::P)] = value->u.integer;
            }
            else if (strcmp(name, "ram") == 0 && value->type == json_array) {
                for (unsigned int k = 0; k < value->u.array.length; k += 2) {
                    if (k + 1 < value->u.array.length) {
                        json_value* addr_val = value->u.array.values[k];
                        json_value* data_val = value->u.array.values[k + 1];
                        if (addr_val->type == json_integer && data_val->type == json_integer) {
                            cpu.memory[addr_val->u.integer] = data_val->u.integer;
                        }
                    }
                }
            }
        }
        
        // Store initial state for comparison
        uint16_t initial_pc = (cpu.registers[static_cast<int>(Reg::PCH)] << 8) | cpu.registers[static_cast<int>(Reg::PCL)];
        uint8_t initial_a = cpu.registers[static_cast<int>(Reg::A)];
        uint8_t initial_p = cpu.registers[static_cast<int>(Reg::P)];
        uint8_t immediate_value = cpu.memory[initial_pc + 1];
        
        // Execute until completion
        bus_state_t bus_state = 0;
        int cycle_count = 0;
        int max_cycles = 10;
        
        while (cycle_count < max_cycles && !cpu.isInstructionComplete()) {
            bus_state = cpu.cycle_tick(bus_state);
            cycle_count++;
        }
        
        // Extract expected final state
        uint16_t expected_pc = 0;
        uint8_t expected_a = 0;
        uint8_t expected_p = 0;
        int expected_cycles = 0;
        
        for (unsigned int j = 0; j < final_val->u.object.length; j++) {
            const char* name = final_val->u.object.values[j].name;
            json_value* value = final_val->u.object.values[j].value;
            
            if (strcmp(name, "pc") == 0 && value->type == json_integer) {
                expected_pc = value->u.integer;
            }
            else if (strcmp(name, "a") == 0 && value->type == json_integer) {
                expected_a = value->u.integer;
            }
            else if (strcmp(name, "p") == 0 && value->type == json_integer) {
                expected_p = value->u.integer;
            }
        }
        
        if (cycles_val->type == json_array && cycles_val->u.array.length > 0) {
            expected_cycles = cycles_val->u.array.length;
        }
        
        // Get actual final state
        uint16_t actual_pc = (cpu.registers[static_cast<int>(Reg::PCH)] << 8) | cpu.registers[static_cast<int>(Reg::PCL)];
        uint8_t actual_a = cpu.registers[static_cast<int>(Reg::A)];
        uint8_t actual_p = cpu.registers[static_cast<int>(Reg::P)];
        
        // Check if test passed
        bool test_passed = (actual_pc == expected_pc) && 
                          (actual_a == expected_a) && 
                          (actual_p == expected_p) && 
                          (cycle_count == expected_cycles);
        
        if (test_passed) {
            passed_tests++;
        } else {
            failed_tests++;
            
            if (detailed_count < 10) {
                std::cout << "\n--- FAILURE #" << (detailed_count + 1) << " ---" << std::endl;
                std::cout << "Initial: A=$" << std::hex << std::setfill('0') << std::setw(2) << (int)initial_a 
                         << " P=$" << std::setw(2) << (int)initial_p 
                         << " PC=$" << std::setw(4) << initial_pc
                         << " DATA=$" << std::setw(2) << (int)immediate_value << std::endl;
                
                std::cout << "Expected: A=$" << std::setw(2) << (int)expected_a 
                         << " P=$" << std::setw(2) << (int)expected_p 
                         << " PC=$" << std::setw(4) << expected_pc
                         << " CYC=" << std::dec << expected_cycles << std::endl;
                
                std::cout << "Actual:   A=$" << std::hex << std::setw(2) << (int)actual_a 
                         << " P=$" << std::setw(2) << (int)actual_p 
                         << " PC=$" << std::setw(4) << actual_pc
                         << " CYC=" << std::dec << cycle_count << std::endl;
                
                // Manual ADC calculation for comparison
                uint16_t carry = (initial_p & 0x01) ? 1 : 0;
                uint16_t manual_result = initial_a + immediate_value + carry;
                uint8_t manual_a = manual_result & 0xFF;
                uint8_t manual_p = initial_p & 0x3C; // Keep N, V, B, D flags
                if (manual_result > 0xFF) manual_p |= 0x01; // Carry
                if (manual_a == 0) manual_p |= 0x02; // Zero
                if (manual_a & 0x80) manual_p |= 0x80; // Negative
                // Overflow check
                if (((initial_a ^ immediate_value) & 0x80) == 0 && ((initial_a ^ manual_a) & 0x80) != 0) {
                    manual_p |= 0x40; // Overflow
                }
                
                std::cout << "Manual:   A=$" << std::setw(2) << (int)manual_a 
                         << " P=$" << std::setw(2) << (int)manual_p << std::endl;
                
                detailed_count++;
            }
        }
        
        if (total_tests >= 100) break; // Limit for initial analysis
    }
    
    json_value_free(root);
    
    std::cout << "\n=== ADC IMMEDIATE ANALYSIS SUMMARY ===" << std::endl;
    std::cout << "Total tests analyzed: " << total_tests << std::endl;
    std::cout << "Passed: " << passed_tests << std::endl;
    std::cout << "Failed: " << failed_tests << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(1) 
              << (100.0 * passed_tests / total_tests) << "%" << std::endl;
    
    return 0;
}