#include <iostream>
#include <iomanip>
#include <fstream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

extern "C" {
#include "tests/json_parser.h"
}

// Helper to print binary representation of a byte
void print_binary(uint8_t val, const std::string& label) {
    std::cout << label << " = 0x" << std::hex << (int)val << " (";
    for (int i = 7; i >= 0; i--) {
        std::cout << ((val >> i) & 1);
        if (i == 4) std::cout << " ";
    }
    std::cout << ")" << std::dec << std::endl;
}

int main() {
    std::cout << "=== Debugging Real Test Case ===\n";
    
    // Let's manually construct the failing test case "69 8f b3"
    // Expected P=0x6d, got P=0xad
    
    // Read the actual test file to get the exact test data
    std::ifstream file("tests/processor_tests/6502/v1/69.json");
    if (!file.is_open()) {
        std::cout << "Could not open test file!\n";
        return 1;
    }
    
    // Read just the beginning to find the failing test
    std::string json_content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();
    
    // Search for the failing test "69 8f b3"
    size_t pos = json_content.find("\"69 8f b3\"");
    if (pos == std::string::npos) {
        std::cout << "Could not find test case!\n";
        return 1;
    }
    
    // Find the start of this test object
    size_t start = json_content.rfind('{', pos);
    size_t end = json_content.find('}', pos);
    
    std::string test_json = json_content.substr(start, end - start + 1);
    std::cout << "Found test case:\n" << test_json.substr(0, 200) << "...\n\n";
    
    // Parse the test
    processor_test_t test;
    if (!json_parse_processor_test(test_json.c_str(), &test)) {
        std::cout << "Failed to parse test!\n";
        return 1;
    }
    
    // Extract the key values
    uint8_t initial_a = test.initial.a;
    uint8_t initial_p = test.initial.p;
    uint8_t operand = 0x8f;  // The immediate operand from "69 8f"
    uint8_t expected_a = test.final.a;
    uint8_t expected_p = test.final.p;
    
    std::cout << "Test case analysis:\n";
    print_binary(initial_a, "Initial A");
    print_binary(initial_p, "Initial P");
    print_binary(operand, "Operand");
    print_binary(expected_a, "Expected A");
    print_binary(expected_p, "Expected P");
    
    std::cout << "\nChecking initial state:\n";
    std::cout << "Decimal mode: " << ((initial_p & 0x08) ? "YES" : "NO") << std::endl;
    std::cout << "Carry in: " << ((initial_p & 0x01) ? "YES" : "NO") << std::endl;
    
    // Manual overflow calculation
    uint8_t a = initial_a;
    uint8_t data = operand;
    uint8_t carry_in = (initial_p & 0x01) ? 1 : 0;
    
    std::cout << "\nManual calculation:\n";
    if (initial_p & 0x08) {
        std::cout << "BCD mode calculation:\n";
        uint16_t lo_nibble = (a & 0x0F) + (data & 0x0F) + carry_in;
        uint16_t hi_nibble = (a >> 4) + (data >> 4);
        
        std::cout << "  Lo nibble: " << (a & 0x0F) << " + " << (data & 0x0F) << " + " << (int)carry_in << " = " << lo_nibble << std::endl;
        std::cout << "  Hi nibble: " << (a >> 4) << " + " << (data >> 4) << " = " << hi_nibble;
        
        if (lo_nibble > 9) {
            lo_nibble += 6;
            hi_nibble += 1;
            std::cout << " (adjusted: " << hi_nibble << ")";
        }
        std::cout << std::endl;
        
        if (hi_nibble > 9) {
            hi_nibble += 6;
            std::cout << "  Hi nibble adjusted to: " << hi_nibble << std::endl;
        }
        
        uint8_t result = ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
        print_binary(result, "BCD result");
        
        // Check overflow calculation
        bool overflow = (~(a ^ data) & (a ^ result) & 0x80) != 0;
        std::cout << "  Overflow calculation: (~(a ^ data) & (a ^ result) & 0x80) != 0\n";
        std::cout << "  a ^ data = " << std::hex << (int)(a ^ data) << std::dec << std::endl;
        std::cout << "  ~(a ^ data) = " << std::hex << (int)(~(a ^ data)) << std::dec << std::endl;
        std::cout << "  a ^ result = " << std::hex << (int)(a ^ result) << std::dec << std::endl;
        std::cout << "  (~(a ^ data) & (a ^ result) & 0x80) = " << std::hex << (int)((~(a ^ data) & (a ^ result) & 0x80)) << std::dec << std::endl;
        std::cout << "  Overflow: " << (overflow ? "YES" : "NO") << std::endl;
        std::cout << "  Expected overflow: " << ((expected_p & 0x40) ? "YES" : "NO") << std::endl;
    }
    
    return 0;
}