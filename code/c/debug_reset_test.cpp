#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

int main() {
    // Test direct RESET cycle function call
    std::cout << "=== DIRECT RESET CYCLE FUNCTION TEST ===" << std::endl;
    for (int cycle = 1; cycle <= 7; ++cycle) {
        auto desc = fam65xx_cpp::CycleInterrupts<config_6502>::get_reset_cycle(cycle);
        std::cout << "Direct RESET cycle " << cycle << ":" << std::endl;
        std::cout << "  mem_op: " << (int)desc.mem_op << std::endl;
        std::cout << "  data_op: " << (int)desc.data_op << std::endl;
        std::cout << "  alu_op: " << (int)desc.alu_op << std::endl;
        std::cout << "  sync: " << (desc.is_sync() ? "YES" : "NO") << std::endl;
    }
    std::cout << std::endl;

    // Test table indexing directly
    std::cout << "=== TABLE INDEX TEST ===" << std::endl;
    fam65xx_cpp::fam65xx<config_6502> test_cpu;
    for (int cycle = 1; cycle <= 7; ++cycle) {
        auto desc = test_cpu.GET_CYCLE(256, cycle);  // VIRTUAL_OPCODE_RESET = 256
        std::cout << "Table RESET cycle " << cycle << ":" << std::endl;
        std::cout << "  mem_op: " << (int)desc.mem_op << std::endl;
        std::cout << "  data_op: " << (int)desc.data_op << std::endl;
        std::cout << "  alu_op: " << (int)desc.alu_op << std::endl;
        std::cout << "  sync: " << (desc.is_sync() ? "YES" : "NO") << std::endl;
        
        // Show index calculation
        size_t index = 256 * 8 + (cycle - 1);
        std::cout << "  table index: " << index << std::endl;
    }
    std::cout << std::endl;

    fam65xx_cpp::fam65xx<config_6502> cpu;
    bus_state_t bus_state = BUS_BIT(BUS_RDY_BIT); // Set RDY high (ready)
    
    // Initialize CPU
    cpu.init();
    cpu.set_pc(0x1234);
    cpu.set_sp(0x55);
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: $" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: $" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  State flags: $" << std::hex << cpu.get_state_flags() << std::endl;
    std::cout << "  Opcode: $" << std::hex << cpu.get_opcode() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    
    // Trigger RESET
    cpu.reset();
    
    std::cout << std::endl << "After reset() call:" << std::endl;
    std::cout << "  PC: $" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: $" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  State flags: $" << std::hex << cpu.get_state_flags() << std::endl;
    std::cout << "  Opcode: $" << std::hex << cpu.get_opcode() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    
    // Execute cycles and debug the RESET sequence
    for (int cycle = 1; cycle <= 8; cycle++) {
        std::cout << std::endl << "Executing cycle " << cycle << ":" << std::endl;
        
        // Get state before cycle execution
        uint16_t addr_before = cpu.get_address();
        bool rw_before = cpu.get_rw();
        uint16_t opcode_before = cpu.get_opcode();
        uint8_t step_before = cpu.get_cycle_step();
        uint16_t state_flags_before = cpu.get_state_flags();
        
        std::cout << "  Before cycle:" << std::endl;
        std::cout << "    Address: $" << std::hex << std::setw(4) << std::setfill('0') << addr_before << std::endl;
        std::cout << "    R/W: " << (rw_before ? "R" : "W") << std::endl;
        std::cout << "    Opcode: $" << std::hex << opcode_before << std::endl;
        std::cout << "    Step: " << (int)step_before << std::endl;
        std::cout << "    State flags: $" << std::hex << state_flags_before << std::endl;
        
        // Check what cycle descriptor would be returned for this step
        if (opcode_before == 0x100 && step_before > 0) { // VIRTUAL_OPCODE_RESET
            fam65xx_cpp::cycle_desc_t cycle_desc = cpu.GET_CYCLE(opcode_before, step_before);
            std::cout << "    Cycle descriptor for step " << (int)step_before << ":" << std::endl;
            std::cout << "      mem_op: " << (int)cycle_desc.mem_op << std::endl;
            std::cout << "      data_op: " << (int)cycle_desc.data_op << std::endl;
            std::cout << "      alu_op: " << (int)cycle_desc.alu_op << std::endl;
            std::cout << "      sync: " << (cycle_desc.is_sync() ? "YES" : "NO") << std::endl;
            
            // Show what SHOULD be returned for this step according to direct function call
            auto expected_desc = fam65xx_cpp::CycleInterrupts<config_6502>::get_reset_cycle(step_before);
            std::cout << "    Expected descriptor for step " << (int)step_before << ":" << std::endl;
            std::cout << "      mem_op: " << (int)expected_desc.mem_op << std::endl;
            std::cout << "      data_op: " << (int)expected_desc.data_op << std::endl;
            std::cout << "      alu_op: " << (int)expected_desc.alu_op << std::endl;
            std::cout << "      sync: " << (expected_desc.is_sync() ? "YES" : "NO") << std::endl;
        }
        
        // Set up data for reads
        if (rw_before) {
            uint8_t read_data = 0x00;
            if (addr_before == 0xFFFC) read_data = 0x00; // RESET vector low
            if (addr_before == 0xFFFD) read_data = 0x80; // RESET vector high
            BUS_SET_DATA(bus_state, read_data);
        }
        
        // Test what the table returns for step 1 since that's where the issue might be
        if (cycle == 1 && opcode_before == 0x0) {
            std::cout << "    RESET step 1 table lookup:" << std::endl;
            auto step1_desc = cpu.GET_CYCLE(0x100, 1);
            std::cout << "      mem_op: " << (int)step1_desc.mem_op << std::endl;
            std::cout << "      data_op: " << (int)step1_desc.data_op << std::endl;
            std::cout << "      alu_op: " << (int)step1_desc.alu_op << std::endl;
            std::cout << "      sync: " << (step1_desc.is_sync() ? "YES" : "NO") << std::endl;
            std::cout << "      sync raw: " << (int)step1_desc.sync << std::endl;
            
            // Also check the raw bytes of the descriptor
            uint16_t* raw_desc = reinterpret_cast<uint16_t*>(&step1_desc);
            std::cout << "      raw descriptor: 0x" << std::hex << *raw_desc << std::dec << std::endl;
        }
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        // Get state after cycle execution
        uint16_t addr_after = cpu.get_address();
        bool rw_after = cpu.get_rw();
        uint16_t opcode_after = cpu.get_opcode();
        uint8_t step_after = cpu.get_cycle_step();
        uint16_t state_flags_after = cpu.get_state_flags();
        uint8_t bus_data = BUS_GET_DATA(bus_state);
        
        std::cout << "  After cycle:" << std::endl;
        std::cout << "    Address: $" << std::hex << std::setw(4) << std::setfill('0') << addr_after << std::endl;
        std::cout << "    R/W: " << (rw_after ? "R" : "W") << std::endl;
        std::cout << "    Opcode: $" << std::hex << opcode_after << std::endl;
        std::cout << "    Step: " << (int)step_after << std::endl;
        std::cout << "    State flags: $" << std::hex << state_flags_after << std::endl;
        std::cout << "    Bus data: $" << std::hex << std::setw(2) << std::setfill('0') << (int)bus_data << std::endl;
        
        // Check if we're still in interrupt sequence
        bool in_interrupt_seq = (state_flags_after & 0x800) != 0; // STATE_INTERRUPT_SEQUENCE
        std::cout << "    In interrupt sequence: " << (in_interrupt_seq ? "YES" : "NO") << std::endl;
        
        if (!in_interrupt_seq && cycle > 1) {
            std::cout << "  RESET sequence completed at cycle " << cycle << std::endl;
            break;
        }
    }
    
    std::cout << std::endl << "Final state:" << std::endl;
    std::cout << "  PC: $" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: $" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  P: $" << std::hex << (int)cpu.get_p() << std::endl;
    
    return 0;
}