#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/chip/cpu/mos6510/mos6510.h"
#include "src/core/control_lines_interface.h"

// Simple memory interface for testing
static uint8_t test_memory[65536];

uint8_t test_read_cycle(void* context, uint16_t address) {
    (void)context;  // Unused
    return test_memory[address];
}

void test_write_cycle(void* context, uint16_t address, uint8_t value) {
    (void)context;  // Unused
    test_memory[address] = value;
}

uint8_t test_io_read(void* context, uint8_t port_value, uint8_t ddr) {
    (void)context;    // Unused
    (void)port_value; // Unused
    (void)ddr;        // Unused
    return 0xFF;  // Return default value for external pins
}

void test_io_write(void* context, uint8_t port_value, uint8_t ddr) {
    (void)context;    // Unused
    (void)port_value; // Unused
    (void)ddr;        // Unused
    // Do nothing for this test
}

// Dummy control lines interface - returns no active interrupts
static uint32_t dummy_get_control_lines(void *context) {
    (void)context;  // Unused
    // Return RDY active (bit 29) to allow CPU to proceed, no IRQ/NMI active
    return (1U << 29);  // SYS_MASK_RDY = (1U << SYS_LINE_RDY) = (1U << 29)
}

static void dummy_set_control_lines(void *context, uint32_t lines) {
    (void)context;  // Unused
    (void)lines;    // Unused - dummy implementation does nothing
}

static const control_lines_interface_t control_interface = {
    .get_lines = dummy_get_control_lines,
    .set_lines = dummy_set_control_lines,
    .context = NULL
};

int main() {
    printf("Testing MOS6510 basic functionality after null check removal...\n");
    
    // Initialize memory
    memset(test_memory, 0, sizeof(test_memory));
    
    // Create CPU using device descriptor
    mos6510_t* cpu = (mos6510_t*)mos6510_descriptor.create(&mos6510_descriptor);
    if (!cpu) {
        printf("FAIL: Could not create CPU\n");
        return 1;
    }
    printf("PASS: CPU created successfully\n");
    
    // Create bus cycle operations
    bus_cycle_ops_t bus_ops = {
        .context = NULL,
        .bus_read_cycle = test_read_cycle,
        .bus_write_cycle = test_write_cycle
    };
    
    // Attach bus interface
    mos6510_attach_bus_interface(cpu, &bus_ops);
    printf("PASS: Bus interface attached successfully\n");
    
    // Create I/O interface  
    mos6510_io_port_interface_t io_interface = {
        .context = NULL,
        .read_external_pins = test_io_read,
        .output_pins_changed = test_io_write
    };
    
    // Attach I/O interface
    mos6510_attach_io_interface(cpu, &io_interface);
    printf("PASS: I/O interface attached successfully\n");
    
    // Attach control lines interface
    mos6510_attach_control_lines_interface(cpu, &control_interface);
    printf("PASS: Control lines interface attached successfully\n");
    
    // Set up reset vector to point to our test program
    test_memory[0xFFFC] = 0x00;  // Reset vector low byte (0x1000)
    test_memory[0xFFFD] = 0x10;  // Reset vector high byte (0x1000)
    
    // Test basic CPU reset
    mos6510_reset(cpu);
    printf("PASS: CPU reset completed\n");
    printf("DEBUG: PC after reset = 0x%04X\n", cpu->base.pc);
    
    // Test a simple program: NOP instruction at address 0x1000
    test_memory[0x1000] = 0xEA;  // NOP opcode
    printf("DEBUG: About to execute NOP instruction\n");
    
    // Execute one instruction
    bool step_result = mos6510_step(cpu);
    printf("DEBUG: Step completed, result = %s\n", step_result ? "true" : "false");
    if (!step_result) {
        printf("FAIL: Instruction execution failed\n");
        mos6510_descriptor.destroy(cpu);
        return 1;
    }
    printf("PASS: NOP instruction executed successfully\n");
    
    // Verify PC advanced
    if (cpu->base.pc != 0x1001) {
        printf("FAIL: PC did not advance correctly. Expected 0x1001, got 0x%04X\n", cpu->base.pc);
        mos6510_descriptor.destroy(cpu);
        return 1;
    }
    printf("PASS: PC advanced correctly to 0x%04X\n", cpu->base.pc);
    
    // Test zero page read/write (this will exercise our modified functions)
    test_memory[0x00] = 0x42;  // Set zero page value
    test_memory[0x1001] = 0xA5;  // LDA $00 (zero page)
    test_memory[0x1002] = 0x00;
    
    cpu->base.pc = 0x1001;
    step_result = mos6510_step(cpu);
    if (!step_result) {
        printf("FAIL: LDA zero page instruction failed\n");
        mos6510_descriptor.destroy(cpu);
        return 1;
    }
    printf("PASS: LDA zero page executed\n");
    
    // Check if accumulator was loaded correctly
    if (cpu->base.a != 0x42) {
        printf("FAIL: LDA zero page did not load correct value. Expected 0x42, got 0x%02X\n", cpu->base.a);
        mos6510_descriptor.destroy(cpu);
        return 1;
    }
    printf("PASS: LDA zero page loaded correct value (0x%02X)\n", cpu->base.a);
    
    // Test zero page write
    test_memory[0x1003] = 0x85;  // STA $01 (zero page)
    test_memory[0x1004] = 0x01;
    
    cpu->base.pc = 0x1003;
    step_result = mos6510_step(cpu);
    if (!step_result) {
        printf("FAIL: STA zero page instruction failed\n");
        mos6510_descriptor.destroy(cpu);
        return 1;
    }
    printf("PASS: STA zero page executed\n");
    
    // Check if value was stored correctly
    if (test_memory[0x01] != 0x42) {
        printf("FAIL: STA zero page did not store correct value. Expected 0x42, got 0x%02X\n", test_memory[0x01]);
        mos6510_descriptor.destroy(cpu);
        return 1;
    }
    printf("PASS: STA zero page stored correct value (0x%02X)\n", test_memory[0x01]);
    
    // Clean up
    mos6510_descriptor.destroy(cpu);
    printf("PASS: CPU destroyed successfully\n");
    
    printf("\n=== ALL TESTS PASSED ===\n");
    printf("The null check removal from runtime callbacks is working correctly!\n");
    
    return 0;
}
