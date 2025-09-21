#include <iostream>
#include <iomanip>
#include <fstream>
#include "tests/fam65xx_cpp_test_harness.cpp"

int main() {
    std::cout << "=== Priority 5: Memory Shift/Rotate Operations Analysis ===" << std::endl;
    
    // All memory shift/rotate opcodes
    struct MemoryShiftRotateOp {
        uint8_t opcode;
        const char* name;
        const char* mode;
    };
    
    MemoryShiftRotateOp ops[] = {
        // ASL memory operations
        {0x06, "ASL", "zp"},      // ASL $nn
        {0x16, "ASL", "zp,x"},    // ASL $nn,X  
        {0x0E, "ASL", "abs"},     // ASL $nnnn
        {0x1E, "ASL", "abs,x"},   // ASL $nnnn,X
        
        // LSR memory operations  
        {0x46, "LSR", "zp"},      // LSR $nn
        {0x56, "LSR", "zp,x"},    // LSR $nn,X
        {0x4E, "LSR", "abs"},     // LSR $nnnn  
        {0x5E, "LSR", "abs,x"},   // LSR $nnnn,X
        
        // ROL memory operations
        {0x26, "ROL", "zp"},      // ROL $nn
        {0x36, "ROL", "zp,x"},    // ROL $nn,X
        {0x2E, "ROL", "abs"},     // ROL $nnnn
        {0x3E, "ROL", "abs,x"},   // ROL $nnnn,X
        
        // ROR memory operations  
        {0x66, "ROR", "zp"},      // ROR $nn
        {0x76, "ROR", "zp,x"},    // ROR $nn,X
        {0x6E, "ROR", "abs"},     // ROR $nnnn
        {0x7E, "ROR", "abs,x"},   // ROR $nnnn,X
    };
    
    std::cout << "Testing " << (sizeof(ops) / sizeof(ops[0])) << " memory shift/rotate operations:" << std::endl;
    std::cout << std::endl;
    
    int total_tests = 0;
    int passed_tests = 0;
    
    for (const auto& op : ops) {
        std::cout << std::hex << std::uppercase << std::setfill('0');
        std::cout << "0x" << std::setw(2) << (int)op.opcode << " " << op.name << " " << op.mode << ": ";
        
        // Test this opcode with ProcessorTests
        std::string json_file = "../external/ProcessorTests/6502/v1/" + 
                               std::string(1, std::tolower(op.opcode >> 4, std::locale())) +
                               std::string(1, std::tolower(op.opcode & 0xF, std::locale())) + ".json";
        
        // Convert hex digits to proper hex characters
        char hex_chars[3];
        sprintf(hex_chars, "%02x", op.opcode);
        json_file = "../external/ProcessorTests/6502/v1/" + std::string(hex_chars) + ".json";
        
        std::ifstream file(json_file);
        if (!file.good()) {
            std::cout << "JSON file not found: " << json_file << std::endl;
            continue;
        }
        
        auto result = run_processor_tests_for_opcode(op.opcode, 10); // Test 10 cases
        total_tests += result.total;
        passed_tests += result.passed;
        
        double success_rate = (result.total > 0) ? (100.0 * result.passed / result.total) : 0.0;
        std::cout << std::dec << result.passed << "/" << result.total << " (" 
                  << std::fixed << std::setprecision(1) << success_rate << "%)";
        
        if (result.passed == result.total) {
            std::cout << " ✅";
        } else {
            std::cout << " ❌";
        }
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Overall Results: " << passed_tests << "/" << total_tests << " tests passed";
    double overall_rate = (total_tests > 0) ? (100.0 * passed_tests / total_tests) : 0.0;
    std::cout << " (" << std::fixed << std::setprecision(1) << overall_rate << "%)" << std::endl;
    
    if (passed_tests == total_tests) {
        std::cout << "✅ ALL MEMORY SHIFT/ROTATE OPERATIONS WORKING!" << std::endl;
    } else {
        std::cout << "❌ " << (total_tests - passed_tests) << " tests failed" << std::endl;
        std::cout << "Priority 5 needs fixes for memory shift/rotate operations." << std::endl;
    }
    
    return (passed_tests == total_tests) ? 0 : 1;
}