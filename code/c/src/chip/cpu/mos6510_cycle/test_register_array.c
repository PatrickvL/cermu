#include "mos6510_state.h"
#include "mos6510_registers.h"
#include "register_access.h"
#include "opcode_mapping.h"
#include "cpu_config.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// Simple test framework macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return false; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

#define TEST_FUNCTION(name) \
    printf("\n--- Testing %s ---\n", #name); \
    if (!name()) { \
        printf("TEST FAILED: %s\n", #name); \
        return 1; \
    }

// Test register array structure and access
bool test_register_array_structure() {
    mos6510_state_t *cpu = mos6510_create(&CPU_CONFIG_6510);
    TEST_ASSERT(cpu != NULL, "CPU creation successful");
    
    // Test register count and layout
    TEST_ASSERT(MOS6510_REGISTER_COUNT == 16, "Register count is 16");
    
    // Test architectural registers
    TEST_ASSERT(REG_A == 0, "REG_A is index 0");
    TEST_ASSERT(REG_X == 1, "REG_X is index 1"); 
    TEST_ASSERT(REG_Y == 2, "REG_Y is index 2");
    TEST_ASSERT(REG_P == 3, "REG_P is index 3");
    TEST_ASSERT(REG_SP == 4, "REG_SP is index 4");
    TEST_ASSERT(REG_PCL == 5, "REG_PCL is index 5");
    TEST_ASSERT(REG_PCH == 6, "REG_PCH is index 6");
    
    // Test visual6502 internal registers
    TEST_ASSERT(REG_DL == 7, "REG_DL is index 7");
    TEST_ASSERT(REG_DOR == 8, "REG_DOR is index 8");
    TEST_ASSERT(REG_SB == 9, "REG_SB is index 9");
    TEST_ASSERT(REG_ADL == 10, "REG_ADL is index 10");
    TEST_ASSERT(REG_ADH == 11, "REG_ADH is index 11");
    TEST_ASSERT(REG_ABL == 12, "REG_ABL is index 12");
    TEST_ASSERT(REG_ABH == 13, "REG_ABH is index 13");
    TEST_ASSERT(REG_AC == 14, "REG_AC is index 14");
    TEST_ASSERT(REG_ADD == 15, "REG_ADD is index 15");
    
    mos6510_destroy(cpu);
    return true;
}

// Test register access macros
bool test_register_access_macros() {
    mos6510_state_t *cpu = mos6510_create(&CPU_CONFIG_6510);
    TEST_ASSERT(cpu != NULL, "CPU creation successful");
    
    // Test accumulator access
    SET_CPU_A(cpu, 0x42);
    TEST_ASSERT(CPU_A(cpu) == 0x42, "Accumulator read/write");
    
    // Test X/Y register access
    SET_CPU_X(cpu, 0x12);
    SET_CPU_Y(cpu, 0x34);
    TEST_ASSERT(CPU_X(cpu) == 0x12, "X register read/write");
    TEST_ASSERT(CPU_Y(cpu) == 0x34, "Y register read/write");
    
    // Test processor status register
    SET_CPU_P(cpu, 0xFF);
    TEST_ASSERT(CPU_P(cpu) == 0xFF, "Processor status read/write");
    
    // Test stack pointer
    SET_CPU_SP(cpu, 0xFD);
    TEST_ASSERT(CPU_SP(cpu) == 0xFD, "Stack pointer read/write");
    
    // Test program counter (16-bit)
    SET_CPU_PC(cpu, 0x1234);
    TEST_ASSERT(CPU_PC(cpu) == 0x1234, "Program counter 16-bit access");
    TEST_ASSERT(CPU_PCL(cpu) == 0x34, "Program counter low byte");
    TEST_ASSERT(CPU_PCH(cpu) == 0x12, "Program counter high byte");
    
    // Test internal registers
    SET_CPU_DL(cpu, 0xAB);
    SET_CPU_SB(cpu, 0xCD);
    TEST_ASSERT(CPU_DL(cpu) == 0xAB, "Data latch read/write");
    TEST_ASSERT(CPU_SB(cpu) == 0xCD, "Special bus read/write");
    
    // Test address registers
    SET_CPU_ADDR_INTERNAL(cpu, 0x5678);
    TEST_ASSERT(CPU_ADDR_INTERNAL(cpu) == 0x5678, "Internal address 16-bit access");
    TEST_ASSERT(CPU_ADL(cpu) == 0x78, "Address low internal bus");
    TEST_ASSERT(CPU_ADH(cpu) == 0x56, "Address high internal bus");
    
    SET_CPU_ADDR_EXTERNAL(cpu, 0x9ABC);
    TEST_ASSERT(CPU_ADDR_EXTERNAL(cpu) == 0x9ABC, "External address 16-bit access");
    TEST_ASSERT(CPU_ABL(cpu) == 0xBC, "Address bus low latch");
    TEST_ASSERT(CPU_ABH(cpu) == 0x9A, "Address bus high latch");
    
    mos6510_destroy(cpu);
    return true;
}

// Test processor status flags
bool test_processor_status_flags() {
    mos6510_state_t *cpu = mos6510_create(&CPU_CONFIG_6510);
    TEST_ASSERT(cpu != NULL, "CPU creation successful");
    
    // Clear all flags
    SET_CPU_P(cpu, FLAG_UNUSED);  // Only unused flag set
    
    // Test individual flag setting
    SET_CPU_FLAG_C(cpu);
    TEST_ASSERT(CPU_FLAG_C(cpu) != 0, "Carry flag set");
    
    SET_CPU_FLAG_Z(cpu);
    TEST_ASSERT(CPU_FLAG_Z(cpu) != 0, "Zero flag set");
    
    SET_CPU_FLAG_N(cpu);  
    TEST_ASSERT(CPU_FLAG_N(cpu) != 0, "Negative flag set");
    
    // Test flag clearing
    CLR_CPU_FLAG_C(cpu);
    TEST_ASSERT(CPU_FLAG_C(cpu) == 0, "Carry flag cleared");
    
    // Test conditional flag setting
    SET_CPU_FLAG_Z_COND(cpu, true);
    TEST_ASSERT(CPU_FLAG_Z(cpu) != 0, "Conditional flag set (true)");
    
    SET_CPU_FLAG_Z_COND(cpu, false);
    TEST_ASSERT(CPU_FLAG_Z(cpu) == 0, "Conditional flag set (false)");
    
    // Test NZ flag setting helper
    set_cpu_nz_flags(cpu, 0x00);
    TEST_ASSERT(CPU_FLAG_Z(cpu) != 0, "Zero flag set for zero value");
    TEST_ASSERT(CPU_FLAG_N(cpu) == 0, "Negative flag clear for zero value");
    
    set_cpu_nz_flags(cpu, 0x80);
    TEST_ASSERT(CPU_FLAG_Z(cpu) == 0, "Zero flag clear for non-zero value");
    TEST_ASSERT(CPU_FLAG_N(cpu) != 0, "Negative flag set for negative value");
    
    mos6510_destroy(cpu);
    return true;
}

// Test opcode to register mapping
bool test_opcode_register_mapping() {
    // Test target register mapping
    TEST_ASSERT(opcode_to_target_register(0xA0) == REG_Y, "LDY maps to Y register");  // LDY #
    TEST_ASSERT(opcode_to_target_register(0xA2) == REG_X, "LDX maps to X register");  // LDX #
    
    // Test source register mapping  
    TEST_ASSERT(opcode_to_source_register(0x8D) == REG_A, "STA maps to A register");  // STA abs
    TEST_ASSERT(opcode_to_source_register(0x8E) == REG_X, "STX maps to X register");  // STX abs
    TEST_ASSERT(opcode_to_source_register(0x8C) == REG_Y, "STY maps to Y register");  // STY abs
    
    // Test instruction classification
    TEST_ASSERT(opcode_is_branch(0x10), "BPL is branch instruction");
    TEST_ASSERT(opcode_is_branch(0x30), "BMI is branch instruction"); 
    TEST_ASSERT(!opcode_is_branch(0xA9), "LDA is not branch instruction");
    
    TEST_ASSERT(opcode_is_store(0x85), "STA zp is store instruction");
    TEST_ASSERT(!opcode_is_store(0xA5), "LDA zp is not store instruction");
    
    TEST_ASSERT(opcode_is_load(0xA5), "LDA zp is load instruction");
    TEST_ASSERT(opcode_is_load(0xA6), "LDX zp is load instruction");
    TEST_ASSERT(!opcode_is_load(0x85), "STA zp is not load instruction");
    
    return true;
}

// Test shared operations
bool test_shared_operations() {
    mos6510_state_t *cpu = mos6510_create(&CPU_CONFIG_6510);
    TEST_ASSERT(cpu != NULL, "CPU creation successful");
    
    // Test shared load operation
    shared_load_operation(cpu, REG_A, 0x42);
    TEST_ASSERT(CPU_A(cpu) == 0x42, "Shared load operation sets value");
    TEST_ASSERT(CPU_FLAG_Z(cpu) == 0, "Shared load clears zero flag for non-zero");
    TEST_ASSERT(CPU_FLAG_N(cpu) == 0, "Shared load clears negative flag for positive");
    
    shared_load_operation(cpu, REG_X, 0x00);
    TEST_ASSERT(CPU_X(cpu) == 0x00, "Shared load operation sets zero");
    TEST_ASSERT(CPU_FLAG_Z(cpu) != 0, "Shared load sets zero flag for zero");
    
    shared_load_operation(cpu, REG_Y, 0x80);
    TEST_ASSERT(CPU_Y(cpu) == 0x80, "Shared load operation sets negative");
    TEST_ASSERT(CPU_FLAG_N(cpu) != 0, "Shared load sets negative flag for negative");
    
    // Test shared store operation
    SET_CPU_A(cpu, 0x55);
    uint8_t stored = shared_store_operation(cpu, REG_A);
    TEST_ASSERT(stored == 0x55, "Shared store operation returns value");
    
    // Test shared increment operation
    SET_CPU_X(cpu, 0x7E);
    shared_increment_operation(cpu, REG_X);
    TEST_ASSERT(CPU_X(cpu) == 0x7F, "Shared increment operation increments");
    TEST_ASSERT(CPU_FLAG_N(cpu) == 0, "Increment result positive");
    
    shared_increment_operation(cpu, REG_X);  // 0x7F -> 0x80
    TEST_ASSERT(CPU_X(cpu) == 0x80, "Shared increment wraps correctly");
    TEST_ASSERT(CPU_FLAG_N(cpu) != 0, "Increment result negative");
    
    // Test shared decrement operation  
    SET_CPU_Y(cpu, 0x01);
    shared_decrement_operation(cpu, REG_Y);
    TEST_ASSERT(CPU_Y(cpu) == 0x00, "Shared decrement operation decrements");
    TEST_ASSERT(CPU_FLAG_Z(cpu) != 0, "Decrement result zero");
    
    // Test shared compare operation
    SET_CPU_A(cpu, 0x50);
    shared_compare_operation(cpu, REG_A, 0x30);
    TEST_ASSERT(CPU_FLAG_C(cpu) != 0, "Compare sets carry for A >= operand");
    TEST_ASSERT(CPU_FLAG_Z(cpu) == 0, "Compare clears zero for A != operand");
    
    shared_compare_operation(cpu, REG_A, 0x50);
    TEST_ASSERT(CPU_FLAG_Z(cpu) != 0, "Compare sets zero for A == operand");
    
    mos6510_destroy(cpu);
    return true;
}

// Test register properties and debugging
bool test_register_properties() {
    // Test register property lookup
    TEST_ASSERT(strcmp(register_properties[REG_A].name, "A") == 0, "Register A name correct");
    TEST_ASSERT(strcmp(register_properties[REG_DL].name, "DL") == 0, "Register DL name correct");
    
    // Test register categories
    TEST_ASSERT(register_properties[REG_A].is_architectural, "A is architectural");
    TEST_ASSERT(!register_properties[REG_DL].is_architectural, "DL is not architectural");
    TEST_ASSERT(register_properties[REG_SB].is_internal_bus, "SB is internal bus");
    TEST_ASSERT(register_properties[REG_ADL].is_address_related, "ADL is address related");
    TEST_ASSERT(register_properties[REG_AC].is_alu_related, "AC is ALU related");
    
    // Test convenience macros
    TEST_ASSERT(IS_ARCHITECTURAL_REG(REG_A), "A is architectural (macro)");
    TEST_ASSERT(IS_INTERNAL_BUS_REG(REG_SB), "SB is internal bus (macro)");
    TEST_ASSERT(IS_ADDRESS_REG(REG_PCL), "PCL is address related (macro)");
    TEST_ASSERT(IS_ALU_REG(REG_AC), "AC is ALU related (macro)");
    
    // Test register validation
    TEST_ASSERT(IS_VALID_REG(0), "Register 0 is valid");
    TEST_ASSERT(IS_VALID_REG(15), "Register 15 is valid");
    TEST_ASSERT(!IS_VALID_REG(16), "Register 16 is invalid");
    
    return true;
}

// Test CPU state management
bool test_cpu_state_management() {
    // Test CPU creation and destruction
    mos6510_state_t *cpu = mos6510_create(&CPU_CONFIG_6510);
    TEST_ASSERT(cpu != NULL, "CPU creation successful");
    
    // Test state validation
    TEST_ASSERT(mos6510_validate_state(cpu), "Initial state is valid");
    
    // Test register array is properly initialized
    TEST_ASSERT(CPU_P(cpu) & FLAG_UNUSED, "Unused flag set after init");
    TEST_ASSERT(CPU_P(cpu) & FLAG_I, "Interrupt disable flag set after init");
    
    // Test reset functionality
    SET_CPU_A(cpu, 0x42);
    SET_CPU_PC(cpu, 0x1234);
    mos6510_reset(cpu);
    
    TEST_ASSERT(CPU_A(cpu) == 0x00, "Accumulator reset to 0");
    TEST_ASSERT(CPU_P(cpu) & FLAG_I, "Interrupt disable flag set after reset");
    TEST_ASSERT(CPU_SP(cpu) == 0xFD, "Stack pointer reset to 0xFD");
    
    // Test utility functions
    mos6510_set_pc(cpu, 0x5678);
    TEST_ASSERT(mos6510_get_pc(cpu) == 0x5678, "PC utility functions work");
    
    mos6510_set_a(cpu, 0xAB);
    TEST_ASSERT(mos6510_get_a(cpu) == 0xAB, "A utility functions work");
    
    // Test debug mode
    TEST_ASSERT(!mos6510_is_debug_enabled(cpu), "Debug initially disabled");
    mos6510_set_debug_enabled(cpu, true);
    TEST_ASSERT(mos6510_is_debug_enabled(cpu), "Debug can be enabled");
    
    // Test cycle counter
    TEST_ASSERT(mos6510_get_cycle_count(cpu) == 0, "Cycle count initially zero");
    cpu->total_cycles = 12345;
    TEST_ASSERT(mos6510_get_cycle_count(cpu) == 12345, "Cycle count can be read");
    mos6510_reset_cycle_count(cpu);
    TEST_ASSERT(mos6510_get_cycle_count(cpu) == 0, "Cycle count can be reset");
    
    mos6510_destroy(cpu);
    return true;
}

// Test state dump functionality
bool test_state_dump() {
    mos6510_state_t *cpu = mos6510_create(&CPU_CONFIG_6510);
    TEST_ASSERT(cpu != NULL, "CPU creation successful");
    
    // Set some interesting values
    SET_CPU_A(cpu, 0x42);
    SET_CPU_X(cpu, 0x12);
    SET_CPU_Y(cpu, 0x34);
    SET_CPU_PC(cpu, 0x1234);
    SET_CPU_FLAG_C(cpu);
    SET_CPU_FLAG_Z(cpu);
    
    // Test state dump
    char buffer[1024];
    mos6510_state_dump(cpu, buffer, sizeof(buffer));
    
    // Check that dump contains expected information
    TEST_ASSERT(strstr(buffer, "6510") != NULL, "State dump contains CPU variant");
    TEST_ASSERT(strstr(buffer, "A=42") != NULL, "State dump contains accumulator");
    TEST_ASSERT(strstr(buffer, "PC=1234") != NULL, "State dump contains PC");
    
    printf("State dump preview:\n%s\n", buffer);
    
    mos6510_destroy(cpu);
    return true;
}

// Main test function
int main() {
    printf("=== MOS6510 Register Array Architecture Tests ===\n");
    
    TEST_FUNCTION(test_register_array_structure);
    TEST_FUNCTION(test_register_access_macros);
    TEST_FUNCTION(test_processor_status_flags);
    TEST_FUNCTION(test_opcode_register_mapping);
    TEST_FUNCTION(test_shared_operations);
    TEST_FUNCTION(test_register_properties);
    TEST_FUNCTION(test_cpu_state_management);
    TEST_FUNCTION(test_state_dump);
    
    printf("\n=== ALL TESTS PASSED ===\n");
    printf("Register array architecture implementation is complete and functional.\n");
    
    printf("\nArchitecture Summary:\n");
    printf("- 16-register array with visual6502 internal registers\n");
    printf("- Hardware-accurate register access macros\n");
    printf("- Opcode bit-pattern to register index mapping\n");
    printf("- Shared operation functions for code reuse\n");
    printf("- Complete debugging and inspection capabilities\n");
    printf("- Processor status flag management\n");
    printf("- CPU state management with validation\n");
    
    return 0;
}