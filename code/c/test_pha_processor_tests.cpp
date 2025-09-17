#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstring>

#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "external/processor-tests/ProcessorTestsRunner.h"

using namespace std;

int main() {
    cout << "=== PHA (0x48) ProcessorTests Validation ===" << endl;
    cout << "Testing with hardware-verified test vectors" << endl << endl;

    // Test PHA specifically
    ProcessorTestsRunner runner;
    
    // Test 6502 core with PHA instruction
    auto results = runner.run_instruction_tests<config_6502>("6502", 0x48, 1);  // Just 1 test case
    
    cout << "PHA (0x48) Results:" << endl;
    cout << "  Passed: " << results.passed << "/" << results.total << endl;
    cout << "  Success rate: " << (results.total > 0 ? (100.0 * results.passed / results.total) : 0.0) << "%" << endl;
    
    if (results.passed == results.total) {
        cout << "\n🎉 PHA implementation PERFECT! Stack operations fix is working!" << endl;
    } else {
        cout << "\n❌ PHA still failing. Need to debug further." << endl;
        
        // Run a few more test cases for detailed analysis
        cout << "\nRunning extended test for analysis..." << endl;
        auto extended_results = runner.run_instruction_tests<config_6502>("6502", 0x48, 10);
        cout << "Extended results: " << extended_results.passed << "/" << extended_results.total << endl;
    }
    
    return 0;
}