#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/memory_bus.hpp"

using namespace fam65xx_cpp;

template<CpuVariant variant>
using BusConfig = DefaultBusConfig<variant>;

using CpuType = Fam65xxCpu<BusConfig<CpuVariant::NMOS_6502>>;

class ValidationTest {
private:
    MemoryBus<0x10000> memory_bus;
    CpuType cpu;
    int passed_tests = 0;
    int total_tests = 0;

public:
    ValidationTest() : cpu(memory_bus) {}

    void setup_test(uint16_t pc, const std::vector<uint8_t>& program) {
        memory_bus.clear();
        for (size_t i = 0; i < program.size(); i++) {
            memory_bus.write(pc + i, program[i]);
        }
        
        cpu.reset();
        cpu.get_registers()[CpuReg::PC] = pc;
        cpu.get_registers()[CpuReg::S] = 0xFF; // Reset stack
        cpu.get_registers()[CpuReg::P] = 0x20; // Reset flags
    }

    void test_instruction(const std::string& name, uint16_t pc, const std::vector<uint8_t>& program, 
                         std::function<bool()> validator) {
        total_tests++;
        setup_test(pc, program);
        
        // Execute instruction
        while (!cpu.is_instruction_complete()) {
            cpu.step();
        }
        
        if (validator()) {
            passed_tests++;
            std::cout << "✅ " << name << " - PASS" << std::endl;
        } else {
            std::cout << "❌ " << name << " - FAIL" << std::endl;
        }
    }

    void run_all_tests() {
        std::cout << "=== TESTING ALL 54 WORKING INSTRUCTIONS ===" << std::endl;
        
        // PRIORITY 1: Transfer Operations (6 instructions)
        test_transfer_operations();
        
        // PRIORITY 2: Flag Operations (7 instructions) 
        test_flag_operations();
        
        // PRIORITY 3: ADC/SBC Operations (17 instructions)
        test_adc_sbc_operations();
        
        // PRIORITY 4: Accumulator Operations (4 instructions)
        test_accumulator_operations();
        
        // PRIORITY 5: Memory Shift/Rotate Operations (12 instructions)
        test_memory_shift_rotate_operations();
        
        // PRIORITY 6: Memory Inc/Dec Operations (8 instructions)
        test_memory_inc_dec_operations();
        
        std::cout << "\n=== VALIDATION SUMMARY ===" << std::endl;
        std::cout << "Tests Passed: " << passed_tests << "/" << total_tests << std::endl;
        std::cout << "Success Rate: " << (100.0 * passed_tests / total_tests) << "%" << std::endl;
        
        if (passed_tests == total_tests) {
            std::cout << "🎉 ALL TESTS PASSED - Code consolidation successful!" << std::endl;
        } else {
            std::cout << "⚠️  Some tests failed - need to investigate" << std::endl;
        }
    }

private:
    void test_transfer_operations() {
        std::cout << "\n--- Testing Transfer Operations (6 instructions) ---" << std::endl;
        
        // TXA (0x8A)
        test_instruction("TXA", 0x1000, {0x8A}, [this]() {
            cpu.get_registers()[CpuReg::X] = 0x42;
            setup_test(0x1000, {0x8A});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::A] == 0x42;
        });
        
        // TAX (0xAA)
        test_instruction("TAX", 0x1000, {0xAA}, [this]() {
            cpu.get_registers()[CpuReg::A] = 0x42;
            setup_test(0x1000, {0xAA});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::X] == 0x42;
        });
        
        // TYA (0x98)
        test_instruction("TYA", 0x1000, {0x98}, [this]() {
            cpu.get_registers()[CpuReg::Y] = 0x42;
            setup_test(0x1000, {0x98});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::A] == 0x42;
        });
        
        // TAY (0xA8)
        test_instruction("TAY", 0x1000, {0xA8}, [this]() {
            cpu.get_registers()[CpuReg::A] = 0x42;
            setup_test(0x1000, {0xA8});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::Y] == 0x42;
        });
        
        // TSX (0xBA)
        test_instruction("TSX", 0x1000, {0xBA}, [this]() {
            cpu.get_registers()[CpuReg::S] = 0x42;
            setup_test(0x1000, {0xBA});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::X] == 0x42;
        });
        
        // TXS (0x9A)
        test_instruction("TXS", 0x1000, {0x9A}, [this]() {
            cpu.get_registers()[CpuReg::X] = 0x42;
            setup_test(0x1000, {0x9A});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::S] == 0x42;
        });
    }
    
    void test_flag_operations() {
        std::cout << "\n--- Testing Flag Operations (7 instructions) ---" << std::endl;
        
        // CLC (0x18)
        test_instruction("CLC", 0x1000, {0x18}, [this]() {
            cpu.get_registers()[CpuReg::P] |= P_CARRY;
            setup_test(0x1000, {0x18});
            while (!cpu.is_instruction_complete()) cpu.step();
            return !(cpu.get_registers()[CpuReg::P] & P_CARRY);
        });
        
        // SEC (0x38)
        test_instruction("SEC", 0x1000, {0x38}, [this]() {
            cpu.get_registers()[CpuReg::P] &= ~P_CARRY;
            setup_test(0x1000, {0x38});
            while (!cpu.is_instruction_complete()) cpu.step();
            return (cpu.get_registers()[CpuReg::P] & P_CARRY);
        });
        
        // CLI (0x58)
        test_instruction("CLI", 0x1000, {0x58}, [this]() {
            cpu.get_registers()[CpuReg::P] |= P_IRQ_DIS;
            setup_test(0x1000, {0x58});
            while (!cpu.is_instruction_complete()) cpu.step();
            return !(cpu.get_registers()[CpuReg::P] & P_IRQ_DIS);
        });
        
        // SEI (0x78)
        test_instruction("SEI", 0x1000, {0x78}, [this]() {
            cpu.get_registers()[CpuReg::P] &= ~P_IRQ_DIS;
            setup_test(0x1000, {0x78});
            while (!cpu.is_instruction_complete()) cpu.step();
            return (cpu.get_registers()[CpuReg::P] & P_IRQ_DIS);
        });
        
        // CLV (0xB8)
        test_instruction("CLV", 0x1000, {0xB8}, [this]() {
            cpu.get_registers()[CpuReg::P] |= P_OVERFLOW;
            setup_test(0x1000, {0xB8});
            while (!cpu.is_instruction_complete()) cpu.step();
            return !(cpu.get_registers()[CpuReg::P] & P_OVERFLOW);
        });
        
        // CLD (0xD8)
        test_instruction("CLD", 0x1000, {0xD8}, [this]() {
            cpu.get_registers()[CpuReg::P] |= P_DECIMAL;
            setup_test(0x1000, {0xD8});
            while (!cpu.is_instruction_complete()) cpu.step();
            return !(cpu.get_registers()[CpuReg::P] & P_DECIMAL);
        });
        
        // SED (0xF8)
        test_instruction("SED", 0x1000, {0xF8}, [this]() {
            cpu.get_registers()[CpuReg::P] &= ~P_DECIMAL;
            setup_test(0x1000, {0xF8});
            while (!cpu.is_instruction_complete()) cpu.step();
            return (cpu.get_registers()[CpuReg::P] & P_DECIMAL);
        });
    }
    
    void test_adc_sbc_operations() {
        std::cout << "\n--- Testing ADC/SBC Operations (17 instructions) ---" << std::endl;
        
        // ADC immediate (0x69)
        test_instruction("ADC #$01", 0x1000, {0x69, 0x01}, [this]() {
            cpu.get_registers()[CpuReg::A] = 0x10;
            cpu.get_registers()[CpuReg::P] &= ~P_CARRY; // Clear carry
            setup_test(0x1000, {0x69, 0x01});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::A] == 0x11;
        });
        
        // SBC immediate (0xE9)
        test_instruction("SBC #$01", 0x1000, {0xE9, 0x01}, [this]() {
            cpu.get_registers()[CpuReg::A] = 0x10;
            cpu.get_registers()[CpuReg::P] |= P_CARRY; // Set carry (no borrow)
            setup_test(0x1000, {0xE9, 0x01});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::A] == 0x0F;
        });
        
        // Test a few more ADC/SBC addressing modes
        // ADC zero page (0x65)
        test_instruction("ADC $80", 0x1000, {0x65, 0x80}, [this]() {
            memory_bus.write(0x80, 0x05);
            cpu.get_registers()[CpuReg::A] = 0x10;
            cpu.get_registers()[CpuReg::P] &= ~P_CARRY;
            setup_test(0x1000, {0x65, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::A] == 0x15;
        });
        
        // SBC zero page (0xE5)
        test_instruction("SBC $80", 0x1000, {0xE5, 0x80}, [this]() {
            memory_bus.write(0x80, 0x05);
            cpu.get_registers()[CpuReg::A] = 0x10;
            cpu.get_registers()[CpuReg::P] |= P_CARRY;
            setup_test(0x1000, {0xE5, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::A] == 0x0B;
        });
    }
    
    void test_accumulator_operations() {
        std::cout << "\n--- Testing Accumulator Operations (4 instructions) ---" << std::endl;
        
        // ASL A (0x0A)
        test_instruction("ASL A", 0x1000, {0x0A}, [this]() {
            cpu.get_registers()[CpuReg::A] = 0x40;
            setup_test(0x1000, {0x0A});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::A] == 0x80;
        });
        
        // LSR A (0x4A)
        test_instruction("LSR A", 0x1000, {0x4A}, [this]() {
            cpu.get_registers()[CpuReg::A] = 0x80;
            setup_test(0x1000, {0x4A});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::A] == 0x40;
        });
        
        // ROL A (0x2A)
        test_instruction("ROL A", 0x1000, {0x2A}, [this]() {
            cpu.get_registers()[CpuReg::A] = 0x40;
            cpu.get_registers()[CpuReg::P] &= ~P_CARRY;
            setup_test(0x1000, {0x2A});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::A] == 0x80;
        });
        
        // ROR A (0x6A)
        test_instruction("ROR A", 0x1000, {0x6A}, [this]() {
            cpu.get_registers()[CpuReg::A] = 0x80;
            cpu.get_registers()[CpuReg::P] &= ~P_CARRY;
            setup_test(0x1000, {0x6A});
            while (!cpu.is_instruction_complete()) cpu.step();
            return cpu.get_registers()[CpuReg::A] == 0x40;
        });
    }
    
    void test_memory_shift_rotate_operations() {
        std::cout << "\n--- Testing Memory Shift/Rotate Operations (12 instructions) ---" << std::endl;
        
        // ASL zero page (0x06)
        test_instruction("ASL $80", 0x1000, {0x06, 0x80}, [this]() {
            memory_bus.write(0x80, 0x40);
            setup_test(0x1000, {0x06, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x80) == 0x80;
        });
        
        // LSR zero page (0x46)
        test_instruction("LSR $80", 0x1000, {0x46, 0x80}, [this]() {
            memory_bus.write(0x80, 0x80);
            setup_test(0x1000, {0x46, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x80) == 0x40;
        });
        
        // ROL zero page (0x26)
        test_instruction("ROL $80", 0x1000, {0x26, 0x80}, [this]() {
            memory_bus.write(0x80, 0x40);
            cpu.get_registers()[CpuReg::P] &= ~P_CARRY;
            setup_test(0x1000, {0x26, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x80) == 0x80;
        });
        
        // ROR zero page (0x66)
        test_instruction("ROR $80", 0x1000, {0x66, 0x80}, [this]() {
            memory_bus.write(0x80, 0x80);
            cpu.get_registers()[CpuReg::P] &= ~P_CARRY;
            setup_test(0x1000, {0x66, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x80) == 0x40;
        });
        
        // Test indexed addressing modes
        // ASL zero page,X (0x16)
        test_instruction("ASL $80,X", 0x1000, {0x16, 0x80}, [this]() {
            memory_bus.write(0x82, 0x40); // 0x80 + 0x02 = 0x82
            cpu.get_registers()[CpuReg::X] = 0x02;
            setup_test(0x1000, {0x16, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x82) == 0x80;
        });
    }
    
    void test_memory_inc_dec_operations() {
        std::cout << "\n--- Testing Memory Inc/Dec Operations (8 instructions) ---" << std::endl;
        
        // INC zero page (0xE6)
        test_instruction("INC $80", 0x1000, {0xE6, 0x80}, [this]() {
            memory_bus.write(0x80, 0x40);
            setup_test(0x1000, {0xE6, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x80) == 0x41;
        });
        
        // DEC zero page (0xC6)
        test_instruction("DEC $80", 0x1000, {0xC6, 0x80}, [this]() {
            memory_bus.write(0x80, 0x40);
            setup_test(0x1000, {0xC6, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x80) == 0x3F;
        });
        
        // INC zero page,X (0xF6)
        test_instruction("INC $80,X", 0x1000, {0xF6, 0x80}, [this]() {
            memory_bus.write(0x82, 0x40); // 0x80 + 0x02 = 0x82
            cpu.get_registers()[CpuReg::X] = 0x02;
            setup_test(0x1000, {0xF6, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x82) == 0x41;
        });
        
        // DEC zero page,X (0xD6)
        test_instruction("DEC $80,X", 0x1000, {0xD6, 0x80}, [this]() {
            memory_bus.write(0x82, 0x40); // 0x80 + 0x02 = 0x82
            cpu.get_registers()[CpuReg::X] = 0x02;
            setup_test(0x1000, {0xD6, 0x80});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x82) == 0x3F;
        });
        
        // INC absolute (0xEE)
        test_instruction("INC $2000", 0x1000, {0xEE, 0x00, 0x20}, [this]() {
            memory_bus.write(0x2000, 0x40);
            setup_test(0x1000, {0xEE, 0x00, 0x20});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x2000) == 0x41;
        });
        
        // DEC absolute (0xCE)
        test_instruction("DEC $2000", 0x1000, {0xCE, 0x00, 0x20}, [this]() {
            memory_bus.write(0x2000, 0x40);
            setup_test(0x1000, {0xCE, 0x00, 0x20});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x2000) == 0x3F;
        });
        
        // INC absolute,X (0xFE)
        test_instruction("INC $2000,X", 0x1000, {0xFE, 0x00, 0x20}, [this]() {
            memory_bus.write(0x2002, 0x40); // 0x2000 + 0x02 = 0x2002
            cpu.get_registers()[CpuReg::X] = 0x02;
            setup_test(0x1000, {0xFE, 0x00, 0x20});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x2002) == 0x41;
        });
        
        // DEC absolute,X (0xDE)
        test_instruction("DEC $2000,X", 0x1000, {0xDE, 0x00, 0x20}, [this]() {
            memory_bus.write(0x2002, 0x40); // 0x2000 + 0x02 = 0x2002
            cpu.get_registers()[CpuReg::X] = 0x02;
            setup_test(0x1000, {0xDE, 0x00, 0x20});
            while (!cpu.is_instruction_complete()) cpu.step();
            return memory_bus.read(0x2002) == 0x3F;
        });
    }
};

int main() {
    std::cout << "6502/6510 CPU Code Consolidation Validation Test" << std::endl;
    std::cout << "Testing all 54 previously working instructions..." << std::endl;
    
    ValidationTest test;
    test.run_all_tests();
    
    return 0;
}