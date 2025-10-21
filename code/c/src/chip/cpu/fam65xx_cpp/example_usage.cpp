/*
 * example_usage.cpp - Example usage of the C++ 65xx CPU emulator
 *
 * This file demonstrates how to use both the C++ template interface
 * and the C wrapper API for different 65xx processor variants.
 */

#include "fam65xx.hpp"
#include "mos6502.h"
#include "mos6510.h"
#include <iostream>
#include <iomanip>

using namespace fam65xx_cpp;

// ============================================================================
// C++ TEMPLATE INTERFACE EXAMPLE
// ============================================================================

void cpp_template_example() {
    std::cout << "=== C++ Template Interface Example ===" << std::endl;
    
    // Create a MOS 6502 CPU using templates
    fam65xx_t<MOS6502Tag> cpu;
    
    // Initialize the CPU
    chip_descriptor_t desc = {};
    bus_state_t pins = cpu.init(&desc);
    
    // Reset the CPU
    pins = cpu.reset(pins);
    
    // Set some initial state using accessor macros
    CPU_A(&cpu) = 0x42;
    CPU_X(&cpu) = 0x10;
    CPU_Y(&cpu) = 0x20;
    CPU_PC(&cpu) = 0x8000;
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  A: $" << std::hex << std::setw(2) << std::setfill('0') << (int)CPU_A(&cpu) << std::endl;
    std::cout << "  X: $" << std::hex << std::setw(2) << std::setfill('0') << (int)CPU_X(&cpu) << std::endl;
    std::cout << "  Y: $" << std::hex << std::setw(2) << std::setfill('0') << (int)CPU_Y(&cpu) << std::endl;
    std::cout << "  PC: $" << std::hex << std::setw(4) << std::setfill('0') << (int)CPU_PC(&cpu) << std::endl;
    
    // Template feature detection at compile-time
    std::cout << "CPU Features:" << std::endl;
    std::cout << "  Has I/O Port: " << (has_io_port<MOS6502Tag>() ? "Yes" : "No") << std::endl;
    std::cout << "  Has BCD: " << (has_bcd<MOS6502Tag>() ? "Yes" : "No") << std::endl;
    std::cout << "  Has CMOS: " << (has_cmos_enhancements<MOS6502Tag>() ? "Yes" : "No") << std::endl;
    std::cout << "  Has Wide Regs: " << (has_wide_registers<MOS6502Tag>() ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
}

// ============================================================================
// MOS 6510 C++ TEMPLATE EXAMPLE
// ============================================================================

void cpp_6510_example() {
    std::cout << "=== MOS 6510 Template Example ===" << std::endl;
    
    // Create a MOS 6510 CPU (has I/O port)
    fam65xx_t<MOS6510Tag> cpu;
    
    // Initialize and reset
    chip_descriptor_t desc = {};
    bus_state_t pins = cpu.init(&desc);
    pins = cpu.reset(pins);
    
    // MOS 6510 has I/O port - initialize and use it
    cpu.init_io_port();
    cpu.write_io_ddr(0xFF);     // Set all pins as output
    cpu.write_io_data(0x07);    // Set data register
    cpu.set_io_input(0x00);     // Set input from external pins
    
    std::cout << "MOS 6510 I/O Port:" << std::endl;
    std::cout << "  DDR: $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.io_port.direction << std::endl;
    std::cout << "  Data: $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.io_port.data << std::endl;
    std::cout << "  Port: $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.read_io_port() << std::endl;
    
    // Feature detection shows I/O port is available
    std::cout << "CPU Features:" << std::endl;
    std::cout << "  Has I/O Port: " << (has_io_port<MOS6510Tag>() ? "Yes" : "No") << std::endl;
    std::cout << "  Has BCD: " << (has_bcd<ProcessorTag::MOS6510>() ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
}

// ============================================================================
// C WRAPPER INTERFACE EXAMPLE
// ============================================================================

void c_wrapper_example() {
    std::cout << "=== C Wrapper Interface Example ===" << std::endl;
    
    // Create MOS 6502 CPU using C wrapper
    mos6502_t* cpu = mos6502_create();
    
    // Initialize and reset
    chip_descriptor_t desc = {};
    bus_state_t pins = mos6502_init(cpu, &desc);
    pins = mos6502_reset(cpu, pins);
    
    // Set registers using C API
    mos6502_set_a(cpu, 0x42);
    mos6502_set_x(cpu, 0x10);
    mos6502_set_y(cpu, 0x20);
    mos6502_set_pc(cpu, 0x8000);
    
    // Read registers using C API
    std::cout << "C API State:" << std::endl;
    std::cout << "  A: $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)mos6502_get_a(cpu) << std::endl;
    std::cout << "  X: $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)mos6502_get_x(cpu) << std::endl;
    std::cout << "  Y: $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)mos6502_get_y(cpu) << std::endl;
    std::cout << "  PC: $" << std::hex << std::setw(4) << std::setfill('0') 
              << (int)mos6502_get_pc(cpu) << std::endl;
    
    // Clean up
    mos6502_destroy(cpu);
    std::cout << std::endl;
}

// ============================================================================
// MOS 6510 C WRAPPER EXAMPLE
// ============================================================================

void c_6510_wrapper_example() {
    std::cout << "=== MOS 6510 C Wrapper Example ===" << std::endl;
    
    // Create MOS 6510 CPU using C wrapper
    mos6510_t* cpu = mos6510_create();
    
    // Initialize with MOS 6510-specific descriptor
    mos6510_desc_t desc = {};
    // desc.base = {};  // Initialize base chip descriptor
    // desc.m6510_in_cb = nullptr;  // Set I/O callbacks if needed
    // desc.m6510_out_cb = nullptr;
    // desc.m6510_io_pullup = 0xFF;
    // desc.m6510_io_floating = 0x00;
    // desc.m6510_user_data = nullptr;
    
    bus_state_t pins = mos6510_init(cpu, &desc);
    pins = mos6510_reset(cpu, pins);
    
    // Set I/O port using C API (6510-specific)
    mos6510_set_io_input(cpu, 0x00);
    
    // Read I/O port state
    std::cout << "MOS 6510 I/O Port (C API):" << std::endl;
    std::cout << "  DDR: $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)mos6510_get_io_ddr(cpu) << std::endl;
    std::cout << "  Data: $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)mos6510_get_io_data(cpu) << std::endl;
    std::cout << "  Input: $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)mos6510_get_io_input(cpu) << std::endl;
    
    // Clean up
    mos6510_destroy(cpu);
    std::cout << std::endl;
}

// ============================================================================
// 65C816 EXAMPLE
// ============================================================================

void cpp_65c816_example() {
    std::cout << "=== WDC 65C816 Template Example ===" << std::endl;
    
    // Create a WDC 65C816 CPU (has wide registers)
    fam65xx_base_t<ProcessorTag::WDC65C816> cpu;
    
    // Initialize and reset
    fam65xx_desc_t desc = {};
    bus_state_t pins = cpu.init(&desc);
    pins = cpu.reset(pins);
    
    // 65C816 starts in emulation mode (6502 compatible)
    std::cout << "Initial mode: " << (cpu.emulation_mode ? "Emulation (6502)" : "Native (16-bit)") << std::endl;
    
    // In emulation mode, use 8-bit registers
    cpu.a = 0x42;
    cpu.x = 0x10;
    cpu.y = 0x20;
    
    std::cout << "8-bit registers:" << std::endl;
    std::cout << "  A: $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.a << std::endl;
    std::cout << "  X: $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.x << std::endl;
    std::cout << "  Y: $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.y << std::endl;
    
    // Switch to native mode and use 16-bit registers
    cpu.emulation_mode = false;
    cpu.a_full = 0x1234;
    cpu.x_full = 0x5678;
    cpu.y_full = 0x9ABC;
    cpu.d = 0x2000;  // Direct page register
    
    std::cout << "16-bit registers (native mode):" << std::endl;
    std::cout << "  A: $" << std::hex << std::setw(4) << std::setfill('0') << (int)cpu.a_full << std::endl;
    std::cout << "  X: $" << std::hex << std::setw(4) << std::setfill('0') << (int)cpu.x_full << std::endl;
    std::cout << "  Y: $" << std::hex << std::setw(4) << std::setfill('0') << (int)cpu.y_full << std::endl;
    std::cout << "  D: $" << std::hex << std::setw(4) << std::setfill('0') << (int)cpu.d << std::endl;
    
    // Feature detection
    std::cout << "CPU Features:" << std::endl;
    std::cout << "  Has Wide Regs: " << (has_wide_registers<ProcessorTag::WDC65C816>() ? "Yes" : "No") << std::endl;
    std::cout << "  Has CMOS: " << (has_cmos<ProcessorTag::WDC65C816>() ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main() {
    std::cout << "65xx Family CPU Emulator Examples" << std::endl;
    std::cout << "==================================" << std::endl << std::endl;
    
    // Run all examples
    cpp_template_example();
    cpp_6510_example();
    c_wrapper_example();
    c_6510_wrapper_example();
    cpp_65c816_example();
    
    std::cout << "All examples completed successfully!" << std::endl;
    return 0;
}