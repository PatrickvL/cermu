#include <iostream>
#include <cassert>
#include "tests/fam65xx_cpp_test_harness.cpp"

int main() {
    std::cout << "=== 6502/6510 CPU Code Consolidation Validation ===" << std::endl;
    std::cout << "Testing key instructions after fast path removal..." << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // Test Transfer Operations
    std::cout << "\n--- Testing Transfer Operations ---" << std::endl;
    
    // TXA (0x8A)
    total++;
    try {
        auto cpu = create_cpu();
        cpu.get_registers()[CpuReg::X] = 0x42;
        run_test_case(cpu, {0x8A}); // TXA
        if (cpu.get_registers()[CpuReg::A] == 0x42) {
            std::cout << "✅ TXA - PASS" << std::endl;
            passed++;
        } else {
            std::cout << "❌ TXA - FAIL" << std::endl;
        }
    } catch (...) {
        std::cout << "❌ TXA - EXCEPTION" << std::endl;
    }
    
    // TAX (0xAA)
    total++;
    try {
        auto cpu = create_cpu();
        cpu.get_registers()[CpuReg::A] = 0x42;
        run_test_case(cpu, {0xAA}); // TAX
        if (cpu.get_registers()[CpuReg::X] == 0x42) {
            std::cout << "✅ TAX - PASS" << std::endl;
            passed++;
        } else {
            std::cout << "❌ TAX - FAIL" << std::endl;
        }
    } catch (...) {
        std::cout << "❌ TAX - EXCEPTION" << std::endl;
    }
    
    // Test Flag Operations
    std::cout << "\n--- Testing Flag Operations ---" << std::endl;
    
    // CLC (0x18)
    total++;
    try {
        auto cpu = create_cpu();
        cpu.get_registers()[CpuReg::P] |= P_CARRY;
        run_test_case(cpu, {0x18}); // CLC
        if (!(cpu.get_registers()[CpuReg::P] & P_CARRY)) {
            std::cout << "✅ CLC - PASS" << std::endl;
            passed++;
        } else {
            std::cout << "❌ CLC - FAIL" << std::endl;
        }
    } catch (...) {
        std::cout << "❌ CLC - EXCEPTION" << std::endl;
    }
    
    // SEC (0x38)
    total++;
    try {
        auto cpu = create_cpu();
        cpu.get_registers()[CpuReg::P] &= ~P_CARRY;
        run_test_case(cpu, {0x38}); // SEC
        if (cpu.get_registers()[CpuReg::P] & P_CARRY) {
            std::cout << "✅ SEC - PASS" << std::endl;
            passed++;
        } else {
            std::cout << "❌ SEC - FAIL" << std::endl;
        }
    } catch (...) {
        std::cout << "❌ SEC - EXCEPTION" << std::endl;
    }
    
    // Test ADC/SBC Operations
    std::cout << "\n--- Testing ADC/SBC Operations ---" << std::endl;
    
    // ADC immediate (0x69)
    total++;
    try {
        auto cpu = create_cpu();
        cpu.get_registers()[CpuReg::A] = 0x10;
        cpu.get_registers()[CpuReg::P] &= ~P_CARRY; // Clear carry
        run_test_case(cpu, {0x69, 0x01}); // ADC #$01
        if (cpu.get_registers()[CpuReg::A] == 0x11) {
            std::cout << "✅ ADC #$01 - PASS" << std::endl;
            passed++;
        } else {
            std::cout << "❌ ADC #$01 - FAIL (got 0x" << std::hex << (int)cpu.get_registers()[CpuReg::A] << ")" << std::endl;
        }
    } catch (...) {
        std::cout << "❌ ADC #$01 - EXCEPTION" << std::endl;
    }
    
    // SBC immediate (0xE9)
    total++;
    try {
        auto cpu = create_cpu();
        cpu.get_registers()[CpuReg::A] = 0x10;
        cpu.get_registers()[CpuReg::P] |= P_CARRY; // Set carry (no borrow)
        run_test_case(cpu, {0xE9, 0x01}); // SBC #$01
        if (cpu.get_registers()[CpuReg::A] == 0x0F) {
            std::cout << "✅ SBC #$01 - PASS" << std::endl;
            passed++;
        } else {
            std::cout << "❌ SBC #$01 - FAIL (got 0x" << std::hex << (int)cpu.get_registers()[CpuReg::A] << ")" << std::endl;
        }
    } catch (...) {
        std::cout << "❌ SBC #$01 - EXCEPTION" << std::endl;
    }
    
    // Test Accumulator Operations
    std::cout << "\n--- Testing Accumulator Operations ---" << std::endl;
    
    // ASL A (0x0A)
    total++;
    try {
        auto cpu = create_cpu();
        cpu.get_registers()[CpuReg::A] = 0x40;
        run_test_case(cpu, {0x0A}); // ASL A
        if (cpu.get_registers()[CpuReg::A] == 0x80) {
            std::cout << "✅ ASL A - PASS" << std::endl;
            passed++;
        } else {
            std::cout << "❌ ASL A - FAIL (got 0x" << std::hex << (int)cpu.get_registers()[CpuReg::A] << ")" << std::endl;
        }
    } catch (...) {
        std::cout << "❌ ASL A - EXCEPTION" << std::endl;
    }
    
    // LSR A (0x4A)
    total++;
    try {
        auto cpu = create_cpu();
        cpu.get_registers()[CpuReg::A] = 0x80;
        run_test_case(cpu, {0x4A}); // LSR A
        if (cpu.get_registers()[CpuReg::A] == 0x40) {
            std::cout << "✅ LSR A - PASS" << std::endl;
            passed++;
        } else {
            std::cout << "❌ LSR A - FAIL (got 0x" << std::hex << (int)cpu.get_registers()[CpuReg::A] << ")" << std::endl;
        }
    } catch (...) {
        std::cout << "❌ LSR A - EXCEPTION" << std::endl;
    }
    
    // Test Memory Operations
    std::cout << "\n--- Testing Memory Operations ---" << std::endl;
    
    // ASL zero page (0x06)
    total++;
    try {
        auto cpu = create_cpu();
        auto& bus = get_memory_bus(cpu);
        bus.write(0x80, 0x40);
        run_test_case(cpu, {0x06, 0x80}); // ASL $80
        if (bus.read(0x80) == 0x80) {
            std::cout << "✅ ASL $80 - PASS" << std::endl;
            passed++;
        } else {
            std::cout << "❌ ASL $80 - FAIL (got 0x" << std::hex << (int)bus.read(0x80) << ")" << std::endl;
        }
    } catch (...) {
        std::cout << "❌ ASL $80 - EXCEPTION" << std::endl;
    }
    
    // INC zero page (0xE6)
    total++;
    try {
        auto cpu = create_cpu();
        auto& bus = get_memory_bus(cpu);
        bus.write(0x80, 0x40);
        run_test_case(cpu, {0xE6, 0x80}); // INC $80
        if (bus.read(0x80) == 0x41) {
            std::cout << "✅ INC $80 - PASS" << std::endl;
            passed++;
        } else {
            std::cout << "❌ INC $80 - FAIL (got 0x" << std::hex << (int)bus.read(0x80) << ")" << std::endl;
        }
    } catch (...) {
        std::cout << "❌ INC $80 - EXCEPTION" << std::endl;
    }
    
    std::cout << "\n=== VALIDATION SUMMARY ===" << std::endl;
    std::cout << "Tests Passed: " << std::dec << passed << "/" << total << std::endl;
    std::cout << "Success Rate: " << (100.0 * passed / total) << "%" << std::endl;
    
    if (passed == total) {
        std::cout << "🎉 ALL TESTS PASSED - Code consolidation successful!" << std::endl;
        return 0;
    } else {
        std::cout << "⚠️  Some tests failed - need to investigate" << std::endl;
        return 1;
    }
}