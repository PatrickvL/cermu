#include <iostream>
#include <fstream>
#include <string>

extern "C" {
#include "tests/json_parser.h"
}

int main() {
    std::cout << "=== JSON Parsing Debug ===" << std::endl;
    
    // Read our test file
    std::ifstream file("test_single_pha.json");
    std::string json_content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();
    
    std::cout << "JSON content:" << std::endl;
    std::cout << json_content << std::endl;
    
    // Parse the test
    processor_test_t test;
    bool parse_result = json_parse_processor_test(json_content.c_str(), &test);
    
    std::cout << "\nParse result: " << (parse_result ? "SUCCESS" : "FAILED") << std::endl;
    
    if (parse_result) {
        std::cout << "Test name: " << test.name << std::endl;
        std::cout << "Initial PC: 0x" << std::hex << test.initial.pc << std::endl;
        std::cout << "Initial A: 0x" << std::hex << (int)test.initial.a << std::endl;
        std::cout << "Initial SP: 0x" << std::hex << (int)test.initial.s << std::endl;
        std::cout << "Initial P: 0x" << std::hex << (int)test.initial.p << std::endl;
        
        std::cout << "Initial RAM sections: " << (int)test.initial.ram_count << std::endl;
        for (int i = 0; i < test.initial.ram_count; i++) {
            std::cout << "  Section " << i << ": addr=0x" << std::hex << test.initial.ram[i].address 
                      << ", bytes=" << (int)test.initial.ram[i].byte_count << std::endl;
            for (int j = 0; j < test.initial.ram[i].byte_count; j++) {
                std::cout << "    [0x" << std::hex << (test.initial.ram[i].address + j) 
                          << "] = 0x" << std::hex << (int)test.initial.ram[i].bytes[j] << std::endl;
            }
        }
        
        std::cout << "Expected final PC: 0x" << std::hex << test.final.pc << std::endl;
        std::cout << "Expected final SP: 0x" << std::hex << (int)test.final.s << std::endl;
    }
    
    return parse_result ? 0 : 1;
}