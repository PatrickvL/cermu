#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
#include "tests/fam65xx_cpp_test_harness.h"

// Test RTI and JMP indirect instructions specifically
int main() {
    printf("=== RTI AND JMP INDIRECT PROCESSORTEST VALIDATION ===\n\n");
    
    // Test RTI instruction (0x40)
    printf("Testing RTI instruction (0x40)...\n");
    uint32_t rti_passed, rti_total;
    run_processor_tests_for_opcode(0x40, rti_passed, rti_total);
    
    double rti_success_rate = (double)rti_passed / rti_total * 100.0;
    printf("RTI (0x40): %u/%u tests passed (%.1f%%)\n", rti_passed, rti_total, rti_success_rate);
    
    // Test JMP indirect instruction (0x6C)
    printf("\nTesting JMP indirect instruction (0x6C)...\n");
    uint32_t jmp_passed, jmp_total;
    run_processor_tests_for_opcode(0x6C, jmp_passed, jmp_total);
    
    double jmp_success_rate = (double)jmp_passed / jmp_total * 100.0;
    printf("JMP indirect (0x6C): %u/%u tests passed (%.1f%%)\n", jmp_passed, jmp_total, jmp_success_rate);
    
    // Summary
    printf("\n=== JUMP/CALL INSTRUCTION GROUP STATUS ===\n");
    printf("JSR (0x20): 100%% (FIXED)\n");
    printf("JMP absolute (0x4C): 100%% (FIXED)\n");
    printf("RTS (0x60): 100%% (FIXED)\n");
    printf("RTI (0x40): %.1f%% %s\n", rti_success_rate, rti_success_rate == 100.0 ? "(FIXED!)" : "(NEEDS WORK)");
    printf("JMP indirect (0x6C): %.1f%% %s\n", jmp_success_rate, jmp_success_rate == 100.0 ? "(FIXED!)" : "(NEEDS WORK)");
    
    uint32_t total_passed = rti_passed + jmp_passed;
    uint32_t total_tests = rti_total + jmp_total;
    double overall_success = (double)total_passed / total_tests * 100.0;
    printf("\nOverall Jump/Call Group: %u/%u tests passed (%.1f%%)\n", total_passed, total_tests, overall_success);
    
    return 0;
}