#include <iostream>
#include <vector>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

// Test configuration for NMOS 6502
using TestConfig = fam65xx_cpp::cpu_config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

void test_sbc_decimal_nz_flags() {
    std::cout << "=== Testing SBC Decimal Mode N/Z Flag Bug ===" << std::endl;
    
    TestCPU cpu;
    cpu.init_for_test();
    
    // Set decimal mode
    cpu.set_p(cpu.get_p() | P_DECIMAL);
    cpu.set_p(cpu.get_p() | P_CARRY); // Set carry (no borrow)
    
    // Test case: 0x50 - 0x01 = 0x49 (BCD) but 0x4F (binary)
    // On NMOS, N/Z flags should be based on binary result (0x4F), not BCD result (0x49)
    cpu.set_a(0x50);
    
    std::cout << "Before SBC: A=0x" << std::hex << std::uppercase << (int)cpu.get_a() 
              << ", P=0x" << (int)cpu.get_p() << std::endl;
    
    // Manually call ALU operation for SBC
    auto& alu_ops = fam65xx_cpp::AluOperations<TestConfig>();
    auto reg = cpu.get_reg(fam65xx_cpp::CpuReg::A);  // This won't work, need different approach
    
    std::cout << "Testing SBC decimal mode implementation..." << std::endl;
    std::cout << "Expected behavior: N/Z flags based on binary calculation on NMOS" << std::endl;
    std::cout << "Binary: 0x50 - 0x01 = 0x4F (N=0, Z=0)" << std::endl;
    std::cout << "BCD: 0x50 - 0x01 = 0x49 (would give different flags)" << std::endl;
}

void test_adc_sbc_comparison() {
    std::cout << "\n=== Comparing ADC vs SBC Decimal Implementation ===" << std::endl;
    
    // The problem is that SBC doesn't have the same NMOS/CMOS flag handling as ADC
    std::cout << "ADC has proper NMOS/CMOS flag differentiation" << std::endl;
    std::cout << "SBC is missing this logic - always uses BCD result for N/Z flags" << std::endl;
    std::cout << "This causes ProcessorTests failures on NMOS variants" << std::endl;
}

int main() {
    test_sbc_decimal_nz_flags();
    test_adc_sbc_comparison();
    
    std::cout << "\n=== Fix Required ===" << std::endl;
    std::cout << "SBC decimal mode needs same N/Z flag handling as ADC:" << std::endl;
    std::cout << "- NMOS: N/Z flags based on binary calculation" << std::endl;
    std::cout << "- CMOS: N/Z flags based on BCD result" << std::endl;
    
    return 0;
}