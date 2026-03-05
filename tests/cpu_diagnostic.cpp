/**
 * Comprehensive CPU Diagnostic Tool for fam65xx
 * Analyzes and debugs CPU core implementation issues
 */
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <fstream>
#include <functional>

// Include fam65xx CPU implementation
#include "../src/chip/cpu/fam65xx/fam65xx.hpp"
#include "../src/chip/cpu/fam65xx/cpu_config.hpp"
#include "../src/core/system_lines.h"

// Simple memory interface for testing
class TestMemory {
private:
    std::vector<uint8_t> memory;
    
public:
    TestMemory() : memory(65536, 0x00) {}
    
    uint8_t read(uint16_t address) {
        return memory[address];
    }
    
    void write(uint16_t address, uint8_t value) {
        memory[address] = value;
    }
    
    void load_binary(const uint8_t* data, size_t size, uint16_t offset) {
        for (size_t i = 0; i < size && (offset + i) < memory.size(); i++) {
            memory[offset + i] = data[i];
        }
    }
    
    void dump(uint16_t start, uint16_t length) {
        std::cout << "Memory dump from $" << std::hex << std::setw(4) << std::setfill('0') 
                  << start << " to $" << std::setw(4) << (start + length - 1) << ":" << std::endl;
        
        for (uint16_t addr = start; addr < start + length; addr += 16) {
            std::cout << "$" << std::hex << std::setw(4) << std::setfill('0') << addr << ": ";
            
            for (int i = 0; i < 16 && (addr + i) < start + length; i++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)memory[addr + i] << " ";
            }
            std::cout << std::dec << std::endl;
        }
    }
};

class CPUDiagnostic {
private:
    using CPU = fam65xx::fam65xx<config_6502>;
    CPU* cpu;
    TestMemory* memory;
    
    void printCPUState() {
        std::cout << "CPU State:" << std::endl;
        std::cout << "  PC: $" << std::hex << std::setw(4) << std::setfill('0') << cpu->get_pc() << std::dec << std::endl;
        std::cout << "  A:  $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu->get_a() << std::dec << std::endl;
        std::cout << "  X:  $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu->get_x() << std::dec << std::endl;
        std::cout << "  Y:  $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu->get_y() << std::dec << std::endl;
        std::cout << "  SP: $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu->get_s() << std::dec << std::endl;
        
        uint8_t p = cpu->get_p();
        std::cout << "  P:  $" << std::hex << std::setw(2) << std::setfill('0') << (int)p << std::dec;
        std::cout << " [N:" << ((p & 0x80) ? "1" : "0");
        std::cout << " V:" << ((p & 0x40) ? "1" : "0");
        std::cout << " -:" << ((p & 0x20) ? "1" : "0");
        std::cout << " B:" << ((p & 0x10) ? "1" : "0");
        std::cout << " D:" << ((p & 0x08) ? "1" : "0");
        std::cout << " I:" << ((p & 0x04) ? "1" : "0");
        std::cout << " Z:" << ((p & 0x02) ? "1" : "0");
        std::cout << " C:" << ((p & 0x01) ? "1" : "0") << "]" << std::endl;
    }
    
    // Execute a single CPU cycle with proper bus interface
    void executeCycle() {
        bus_state_t bus_state = 0;
        
        // Set up default bus state
        BUS_SET_BIT(bus_state, BUS_RW_BIT);  // Default to read
        BUS_SET_BIT(bus_state, BUS_RDY_BIT); // CPU is ready
        
        // Get the address the CPU wants to access
        uint16_t addr = cpu->get_address();
        BUS_SET_ADDR(bus_state, addr);
        
        // Execute CPU cycle
        bus_state = cpu->cycle_tick(bus_state);
        
        // Handle memory operations
        if (BUS_GET_BIT(bus_state, BUS_RW_BIT)) {
            // Read cycle - provide data to CPU
            uint16_t read_addr = BUS_GET_ADDR(bus_state);
            uint8_t data = memory->read(read_addr);
            BUS_SET_DATA(bus_state, data);
            std::cout << "  READ  $" << std::hex << std::setw(4) << std::setfill('0') 
                      << read_addr << " = $" << std::setw(2) << (int)data << std::dec << std::endl;
        } else {
            // Write cycle - store data from CPU
            uint16_t write_addr = BUS_GET_ADDR(bus_state);
            uint8_t write_data = BUS_GET_DATA(bus_state);
            memory->write(write_addr, write_data);
            std::cout << "  WRITE $" << std::hex << std::setw(4) << std::setfill('0') 
                      << write_addr << " = $" << std::setw(2) << (int)write_data << std::dec << std::endl;
        }
    }
    
    // Execute CPU cycles until instruction completion or max cycles
    int executeInstruction(int max_cycles = 10) {
        uint16_t initial_pc = cpu->get_pc();
        int cycles = 0;
        
        for (cycles = 0; cycles < max_cycles; cycles++) {
            executeCycle();
            
            // Check if PC has advanced (simple instruction completion detection)
            if (cpu->get_pc() != initial_pc && cycles > 0) {
                break;
            }
        }
        
        return cycles + 1; // Add 1 for the final cycle
    }
    
public:
    CPUDiagnostic() {
        cpu = new CPU();
        memory = new TestMemory();
    }
    
    ~CPUDiagnostic() {
        delete cpu;
        delete memory;
    }
    
    void runResetTest() {
        std::cout << "\n=== RESET SEQUENCE TEST ===" << std::endl;
        
        // Set up reset vector to point to $0400
        memory->write(0xFFFC, 0x00);  // Low byte of reset vector
        memory->write(0xFFFD, 0x04);  // High byte of reset vector
        
        std::cout << "Memory setup complete. Reset vector points to $0400" << std::endl;
        std::cout << "Reset vector: $FFFC=" << std::hex << (int)memory->read(0xFFFC) 
                  << ", $FFFD=" << (int)memory->read(0xFFFD) << std::dec << std::endl;
        
        std::cout << "\nInitial CPU state:" << std::endl;
        printCPUState();
        
        std::cout << "\nExecuting RESET..." << std::endl;
        cpu->reset();
        
        // Execute reset sequence (typically 7 cycles)
        for (int i = 0; i < 10; i++) {
            executeCycle();
            if (cpu->get_pc() == 0x0400) {
                break;
            }
        }
        
        std::cout << "\nCPU state after RESET:" << std::endl;
        printCPUState();
        
        if (cpu->get_pc() == 0x0400) {
            std::cout << "✅ RESET: PC correctly set to $0400" << std::endl;
        } else {
            std::cout << "❌ RESET: PC should be $0400, got $" << std::hex << cpu->get_pc() << std::dec << std::endl;
        }
    }
    
    void runBasicInstructionTest() {
        std::cout << "\n=== BASIC INSTRUCTION TEST ===" << std::endl;
        
        // Set up reset vector to point to $0400
        memory->write(0xFFFC, 0x00);
        memory->write(0xFFFD, 0x04);
        
        // Test LDA #$42 instruction
        memory->write(0x0400, 0xA9);  // LDA immediate opcode
        memory->write(0x0401, 0x42);  // Immediate value
        memory->write(0x0402, 0xEA);  // NOP
        
        std::cout << "Loaded test program at $0400:" << std::endl;
        std::cout << "  $0400: $A9 (LDA #$42)" << std::endl;
        std::cout << "  $0401: $42 (immediate value)" << std::endl;
        std::cout << "  $0402: $EA (NOP)" << std::endl;
        
        cpu->reset();
        
        // Execute reset sequence
        for (int i = 0; i < 10; i++) {
            executeCycle();
            if (cpu->get_pc() == 0x0400) {
                break;
            }
        }
        
        std::cout << "\nExecuting LDA #$42..." << std::endl;
        printCPUState();
        
        int cycles = executeInstruction();
        std::cout << "Instruction took " << cycles << " cycles" << std::endl;
        
        std::cout << "\nCPU state after LDA #$42:" << std::endl;
        printCPUState();
        
        uint8_t a_reg = cpu->get_a();
        if (a_reg == 0x42) {
            std::cout << "✅ LDA #$42: A register correctly loaded with $42" << std::endl;
        } else {
            std::cout << "❌ LDA #$42: A register should be $42, got $" << std::hex << (int)a_reg << std::dec << std::endl;
        }
        
        if (cycles == 2) {
            std::cout << "✅ LDA #$42: Correct cycle count (2)" << std::endl;
        } else {
            std::cout << "❌ LDA #$42: Should take 2 cycles, took " << cycles << std::endl;
        }
    }
    
    void runZeroPageTest() {
        std::cout << "\n=== ZERO PAGE MEMORY TEST ===" << std::endl;
        
        // Set up reset vector to point to $0400
        memory->write(0xFFFC, 0x00);
        memory->write(0xFFFD, 0x04);
        
        // Test STA $80 followed by LDA $80
        memory->write(0x0400, 0xA9);  // LDA #$33
        memory->write(0x0401, 0x33);
        memory->write(0x0402, 0x85);  // STA $80
        memory->write(0x0403, 0x80);
        memory->write(0x0404, 0xA5);  // LDA $80
        memory->write(0x0405, 0x80);
        
        std::cout << "Loaded test program:" << std::endl;
        std::cout << "  LDA #$33" << std::endl;
        std::cout << "  STA $80" << std::endl;
        std::cout << "  LDA $80" << std::endl;
        
        cpu->reset();
        
        // Execute reset sequence
        for (int i = 0; i < 10; i++) {
            executeCycle();
            if (cpu->get_pc() == 0x0400) {
                break;
            }
        }
        
        // Execute LDA #$33
        std::cout << "\nExecuting LDA #$33..." << std::endl;
        executeInstruction();
        printCPUState();
        
        // Execute STA $80
        std::cout << "\nExecuting STA $80..." << std::endl;
        executeInstruction();
        printCPUState();
        
        std::cout << "Zero page $80 = $" << std::hex << (int)memory->read(0x80) << std::dec << std::endl;
        
        // Execute LDA $80
        std::cout << "\nExecuting LDA $80..." << std::endl;
        executeInstruction();
        printCPUState();
        
        if (memory->read(0x80) == 0x33 && cpu->get_a() == 0x33) {
            std::cout << "✅ Zero page memory: Read/write operations work correctly" << std::endl;
        } else {
            std::cout << "❌ Zero page memory: Operations failed" << std::endl;
            std::cout << "   Expected $80 = $33, A = $33" << std::endl;
            std::cout << "   Got $80 = $" << std::hex << (int)memory->read(0x80) 
                      << ", A = $" << (int)cpu->get_a() << std::dec << std::endl;
        }
    }
    
    void runStuckPCTest() {
        std::cout << "\n=== STUCK PC DIAGNOSIS ===" << std::endl;
        
        // Put a simple infinite loop at $0000 to see what happens
        memory->write(0x0000, 0x4C);  // JMP absolute
        memory->write(0x0001, 0x00);  // Low byte
        memory->write(0x0002, 0x00);  // High byte (JMP $0000)
        
        std::cout << "Loaded infinite loop at $0000: JMP $0000" << std::endl;
        
        // Set PC to $0000 manually
        cpu->init();
        cpu->set_pc(0x0000);
        
        std::cout << "\nForced PC to $0000" << std::endl;
        printCPUState();
        
        // Execute a few cycles to see the pattern
        for (int i = 0; i < 5; i++) {
            std::cout << "\nCycle " << (i + 1) << ":" << std::endl;
            executeCycle();
            printCPUState();
        }
    }
    
    void runComprehensiveDiagnostic() {
        std::cout << "========================================" << std::endl;
        std::cout << "   fam65xx CPU DIAGNOSTIC TOOL" << std::endl;
        std::cout << "   Comprehensive Implementation Analysis" << std::endl;
        std::cout << "========================================" << std::endl;
        
        runResetTest();
        runBasicInstructionTest();
        runZeroPageTest();
        runStuckPCTest();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "   DIAGNOSTIC COMPLETE" << std::endl;
        std::cout << "========================================" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    CPUDiagnostic diagnostic;
    
    if (argc > 1) {
        std::string test = argv[1];
        if (test == "reset") {
            diagnostic.runResetTest();
        } else if (test == "instruction") {
            diagnostic.runBasicInstructionTest();
        } else if (test == "zeropage") {
            diagnostic.runZeroPageTest();
        } else if (test == "stuck") {
            diagnostic.runStuckPCTest();
        } else {
            std::cout << "Usage: " << argv[0] << " [reset|instruction|zeropage|stuck]" << std::endl;
            return 1;
        }
    } else {
        diagnostic.runComprehensiveDiagnostic();
    }
    
    return 0;
}