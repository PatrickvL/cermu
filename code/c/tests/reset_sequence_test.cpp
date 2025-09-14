#include <iostream>
#include <iomanip>
#include <cassert>
#include "fam65xx_cpp_test_harness.h"

// Simple RESET sequence test using the fam65xx_cpp direct API
class ResetSequenceTest {
private:
    fam65xx_cpp::fam65xx<config_6502> cpu;
    TestMemory memory;
    bus_state_t bus_state;
    
public:
    ResetSequenceTest() : bus_state(BUS_BIT(BUS_RDY_BIT)) {}
    
    void setup() {
        // Initialize CPU and memory
        cpu.init();
        
        // Set RESET vector to $8000
        memory.write(0xFFFC, 0x00); // Low byte
        memory.write(0xFFFD, 0x80); // High byte
        
        // Put some code at $8000 for after RESET
        memory.write(0x8000, 0xEA); // NOP
        
        // Initialize CPU to known state
        cpu.set_pc(0x1234);
        cpu.set_sp(0x55);
        cpu.set_p(0x20);
        
        std::cout << "Initial state:" << std::endl;
        std::cout << "  PC: $" << std::hex << std::setw(4) << std::setfill('0') 
                  << cpu.get_pc() << std::endl;
        std::cout << "  SP: $" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)cpu.get_sp() << std::endl;
        std::cout << "  P:  $" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)cpu.get_p() << std::endl;
    }
    
    void test_reset_sequence() {
        std::cout << std::endl << "=== Testing RESET Sequence ===" << std::endl;
        
        // Trigger RESET
        cpu.reset();
        
        // Execute RESET sequence (7 cycles)
        for (int cycle = 1; cycle <= 7; cycle++) {
            // Get expected address before cycle execution
            uint16_t addr = cpu.get_address();
            bool rw = cpu.get_rw();
            
            // Provide data for memory reads
            if (rw) {
                uint8_t data = memory.read(addr);
                BUS_SET_DATA(bus_state, data);
            }
            
            // Execute one cycle
            bus_state = cpu.cycle_tick(bus_state);
            
            // Get actual data after cycle
            uint8_t bus_data = BUS_GET_DATA(bus_state);
            
            std::cout << "  Cycle " << cycle << ": addr=$" << std::hex << std::setw(4) << std::setfill('0') 
                      << addr << ", " << (rw ? "R" : "W") << ", data=$" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)bus_data << std::endl;
            
            // Handle memory writes
            if (!rw) {
                memory.write(addr, bus_data);
            }
            
            // Verify cycle behavior
            switch (cycle) {
                case 1:
                case 2:
                    // Dummy PC reads
                    assert(rw == true);  // Read operation
                    break;
                case 3:
                case 4:
                case 5:
                    // Dummy stack reads (NO writes during RESET)
                    assert(rw == true);  // Read operation (not write!)
                    assert((addr & 0xFF00) == 0x0100); // Stack page
                    break;
                case 6:
                    // Read RESET vector low byte
                    assert(rw == true);  // Read operation
                    assert(addr == 0xFFFC); // RESET vector low
                    break;
                case 7:
                    // Read RESET vector high byte
                    assert(rw == true);  // Read operation
                    assert(addr == 0xFFFD); // RESET vector high
                    break;
            }
        }
        
        std::cout << std::endl << "After RESET execution:" << std::endl;
        std::cout << "  PC: $" << std::hex << std::setw(4) << std::setfill('0') 
                  << cpu.get_pc() << std::endl;
        std::cout << "  SP: $" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)cpu.get_sp() << std::endl;
        std::cout << "  P:  $" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)cpu.get_p() << std::endl;
        
        // Verify final state
        assert(cpu.get_pc() == 0x8000); // PC should jump to RESET vector
        assert(cpu.get_sp() == 0xFF);   // SP should be reset to $FF
        assert((cpu.get_p() & 0x04) != 0); // I flag should be set
        
        std::cout << "✓ RESET sequence test PASSED" << std::endl;
        std::cout << "  - Correct cycle count (7 cycles)" << std::endl;
        std::cout << "  - No stack writes during RESET" << std::endl;
        std::cout << "  - Correct RESET vector read ($FFFC/$FFFD)" << std::endl;
        std::cout << "  - Proper register initialization" << std::endl;
        std::cout << "  - I flag correctly set" << std::endl;
    }
    
    void test_reset_vs_brk() {
        std::cout << std::endl << "=== Testing RESET vs BRK Differences ===" << std::endl;
        
        // Test 1: Verify RESET doesn't write to stack
        std::cout << "Test 1: RESET stack behavior" << std::endl;
        
        // Initialize stack with known pattern
        for (int i = 0; i < 256; i++) {
            memory.write(0x0100 + i, 0xAA);
        }
        
        // Reset CPU and execute sequence
        cpu.reset();
        cpu.set_sp(0xFF); // Ensure SP is at expected value
        
        for (int cycle = 1; cycle <= 7; cycle++) {
            uint16_t addr = cpu.get_address();
            bool rw = cpu.get_rw();
            
            if (rw) {
                uint8_t data = memory.read(addr);
                BUS_SET_DATA(bus_state, data);
            }
            
            bus_state = cpu.cycle_tick(bus_state);
            
            if (!rw) {
                uint8_t bus_data = BUS_GET_DATA(bus_state);
                memory.write(addr, bus_data);
            }
        }
        
        // Verify stack wasn't modified
        assert(cpu.get_sp() == 0xFF); // SP should still be $FF
        for (int i = 0xFD; i <= 0xFF; i++) {
            uint8_t stack_data = memory.read(0x0100 + i);
            assert(stack_data == 0xAA); // Stack should be unchanged
        }
        
        std::cout << "  ✓ RESET doesn't modify stack" << std::endl;
        
        // Test 2: Compare with BRK (which does write to stack)
        std::cout << "Test 2: BRK vs RESET stack comparison" << std::endl;
        
        // Reset CPU for BRK test
        cpu.init();
        cpu.set_pc(0x0200);
        cpu.set_sp(0xFF);
        
        // Set up memory for BRK test
        memory.write(0x0200, 0x00); // BRK instruction
        memory.write(0xFFFE, 0x00); // IRQ vector low
        memory.write(0xFFFF, 0x90); // IRQ vector high
        
        // Initialize stack with known pattern
        for (int i = 0; i < 256; i++) {
            memory.write(0x0100 + i, 0xBB);
        }
        
        // Execute BRK instruction
        for (int cycle = 0; cycle < 8; cycle++) { // BRK takes 7 cycles + opcode fetch
            uint16_t addr = cpu.get_address();
            bool rw = cpu.get_rw();
            
            if (rw) {
                uint8_t data = memory.read(addr);
                BUS_SET_DATA(bus_state, data);
            }
            
            bus_state = cpu.cycle_tick(bus_state);
            
            if (!rw) {
                uint8_t bus_data = BUS_GET_DATA(bus_state);
                memory.write(addr, bus_data);
            }
        }
        
        // Verify BRK did write to stack
        assert(cpu.get_sp() == 0xFC); // SP should be decremented
        
        // Check stack contents
        uint8_t stack_pch = memory.read(0x01FF);
        uint8_t stack_pcl = memory.read(0x01FE);
        uint8_t stack_p = memory.read(0x01FD);
        
        assert(stack_pch == 0x02);  // PCH should be pushed
        assert(stack_pcl == 0x02);  // PCL should be pushed (PC+2)
        assert((stack_p & 0x10) != 0); // B flag should be set in pushed status
        
        std::cout << "  ✓ BRK correctly writes to stack (SP: $" << std::hex 
                  << (int)cpu.get_sp() << ")" << std::endl;
    }
    
    void test_reset_vector_handling() {
        std::cout << std::endl << "=== Testing RESET Vector Handling ===" << std::endl;
        
        struct {
            uint8_t low, high;
            uint16_t expected;
        } test_vectors[] = {
            {0x00, 0x80, 0x8000},
            {0x34, 0x12, 0x1234},
            {0xFF, 0x7F, 0x7FFF},
            {0x00, 0x00, 0x0000},
            {0xFF, 0xFF, 0xFFFF}
        };
        
        for (const auto& test : test_vectors) {
            std::cout << "Testing vector $" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)test.high << std::setw(2) << (int)test.low 
                      << " -> $" << std::setw(4) << test.expected << std::endl;
            
            // Set up RESET vector
            memory.write(0xFFFC, test.low);
            memory.write(0xFFFD, test.high);
            
            // Reset and execute sequence
            cpu.reset();
            
            for (int cycle = 1; cycle <= 7; cycle++) {
                uint16_t addr = cpu.get_address();
                bool rw = cpu.get_rw();
                
                if (rw) {
                    uint8_t data = memory.read(addr);
                    BUS_SET_DATA(bus_state, data);
                }
                
                bus_state = cpu.cycle_tick(bus_state);
                
                if (!rw) {
                    uint8_t bus_data = BUS_GET_DATA(bus_state);
                    memory.write(addr, bus_data);
                }
            }
            
            // Verify PC jumped to correct address
            assert(cpu.get_pc() == test.expected);
        }
        
        std::cout << "✓ RESET vector handling test PASSED" << std::endl;
        std::cout << "  - All vector values correctly loaded" << std::endl;
        std::cout << "  - PC correctly set to vector address" << std::endl;
    }
};

int main() {
    std::cout << "6502/65C02 RESET Sequence Test Suite" << std::endl;
    std::cout << "====================================" << std::endl;
    
    try {
        ResetSequenceTest test;
        
        test.setup();
        test.test_reset_sequence();
        test.test_reset_vs_brk();
        test.test_reset_vector_handling();
        
        std::cout << std::endl << "🎉 All RESET sequence tests completed successfully!" << std::endl;
        std::cout << std::endl << "RESET implementation verified:" << std::endl;
        std::cout << "✓ Hardware-accurate 7-cycle sequence" << std::endl;
        std::cout << "✓ No stack modifications (unlike other interrupts)" << std::endl;
        std::cout << "✓ Proper RESET vector reading ($FFFC/$FFFD)" << std::endl;
        std::cout << "✓ Correct register initialization (SP=$FF, I=1)" << std::endl;
        std::cout << "✓ Accurate cycle-by-cycle bus behavior" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}