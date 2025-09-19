#include "tests/fam65xx_cpp_test_harness.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace fam65xx_cpp;

int main() {
    std::cout << "=== RTS ProcessorTests Debug ===\n";
    
    // Test only RTS instruction (0x60)
    const std::string test_file = "tests/processor_tests/6502/v1/60.json";
    std::ifstream file(test_file);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open " << test_file << std::endl;
        return 1;
    }
    
    json test_data;
    file >> test_data;
    
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;
    
    std::cout << "Processing RTS instruction tests...\n";
    
    // Process each test case
    for (const auto& test_case : test_data) {
        total_tests++;
        
        // Create CPU and test harness
        fam65xx<config_6502> cpu;
        TestHarness<config_6502> harness(cpu);
        
        // Set up initial state from test case
        const auto& initial = test_case["initial"];
        harness.set_state(initial);
        
        // Set up memory from test case
        if (test_case.contains("initial") && test_case["initial"].contains("ram")) {
            for (const auto& ram_entry : test_case["initial"]["ram"]) {
                uint16_t addr = ram_entry[0];
                uint8_t value = ram_entry[1];
                harness.write_memory(addr, value);
            }
        }
        
        // Run the instruction
        harness.run_instruction();
        
        // Check final state
        const auto& final_expected = test_case["final"];
        bool test_passed = harness.check_state(final_expected);
        
        if (!test_passed && failed_tests < 5) {  // Show first 5 failures
            std::cout << "\n=== FAILURE #" << (failed_tests + 1) << " ===\n";
            
            // Show initial state
            std::cout << "Initial state:\n";
            std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') 
                      << static_cast<int>(initial["pc"]) << std::dec << "\n";
            std::cout << "  A: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(initial["a"]) << std::dec << "\n";
            std::cout << "  X: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(initial["x"]) << std::dec << "\n";
            std::cout << "  Y: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(initial["y"]) << std::dec << "\n";
            std::cout << "  S: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(initial["s"]) << std::dec << "\n";
            std::cout << "  P: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(initial["p"]) << std::dec << "\n";
            
            // Show stack contents
            if (test_case["initial"].contains("ram")) {
                std::cout << "Stack contents:\n";
                for (const auto& ram_entry : test_case["initial"]["ram"]) {
                    uint16_t addr = ram_entry[0];
                    uint8_t value = ram_entry[1];
                    if (addr >= 0x0100 && addr <= 0x01FF) {
                        std::cout << "  [0x" << std::hex << std::setw(4) << std::setfill('0') 
                                  << addr << "] = 0x" << std::setw(2) << std::setfill('0') 
                                  << static_cast<int>(value) << std::dec << "\n";
                    }
                }
            }
            
            // Show expected vs actual final state
            std::cout << "Expected final state:\n";
            std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') 
                      << static_cast<int>(final_expected["pc"]) << std::dec << "\n";
            std::cout << "  S: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(final_expected["s"]) << std::dec << "\n";
            
            std::cout << "Actual final state:\n";
            std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') 
                      << harness.get_pc() << std::dec << "\n";
            std::cout << "  S: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(harness.get_s()) << std::dec << "\n";
        }
        
        if (test_passed) {
            passed_tests++;
        } else {
            failed_tests++;
        }
        
        // Progress indicator
        if (total_tests % 1000 == 0) {
            std::cout << "Processed " << total_tests << " tests...\n";
        }
    }
    
    std::cout << "\n=== RTS TEST RESULTS ===\n";
    std::cout << "Total tests: " << total_tests << "\n";
    std::cout << "Passed: " << passed_tests << " (" 
              << (100.0 * passed_tests / total_tests) << "%)\n";
    std::cout << "Failed: " << failed_tests << " (" 
              << (100.0 * failed_tests / total_tests) << "%)\n";
    
    return (failed_tests == 0) ? 0 : 1;
}