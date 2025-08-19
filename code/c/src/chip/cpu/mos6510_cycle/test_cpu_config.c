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

// Test predefined configurations
bool test_predefined_configurations() {
    // Test 6502 configuration
    TEST_ASSERT(CPU_CONFIG_6502.cpu_variant == CPU_6502, "6502 variant correct");
    TEST_ASSERT(!CPU_CONFIG_6502.has_io_port, "6502 no I/O port");
    TEST_ASSERT(!CPU_CONFIG_6502.has_aec_pin, "6502 no AEC pin");
    TEST_ASSERT(CPU_CONFIG_6502.address_lines == 16, "6502 16 address lines");
    TEST_ASSERT(CPU_CONFIG_6502.has_irq_pin, "6502 has IRQ pin");
    TEST_ASSERT(CPU_CONFIG_6502.has_nmi_pin, "6502 has NMI pin");
    
    // Test 6507 configuration
    TEST_ASSERT(CPU_CONFIG_6507.cpu_variant == CPU_6507, "6507 variant correct");
    TEST_ASSERT(CPU_CONFIG_6507.address_lines == 13, "6507 13 address lines");
    TEST_ASSERT(!CPU_CONFIG_6507.has_irq_pin, "6507 no IRQ pin");
    TEST_ASSERT(!CPU_CONFIG_6507.has_nmi_pin, "6507 no NMI pin");
    
    // Test 6510 configuration
    TEST_ASSERT(CPU_CONFIG_6510.cpu_variant == CPU_6510, "6510 variant correct");
    TEST_ASSERT(CPU_CONFIG_6510.has_io_port, "6510 has I/O port");
    TEST_ASSERT(CPU_CONFIG_6510.has_aec_pin, "6510 has AEC pin");
    TEST_ASSERT(CPU_CONFIG_6510.enhanced_rdy, "6510 enhanced RDY");
    
    // Test 8502 configuration
    TEST_ASSERT(CPU_CONFIG_8502.cpu_variant == CPU_8502, "8502 variant correct");
    TEST_ASSERT(CPU_CONFIG_8502.variable_clock, "8502 variable clock");
    TEST_ASSERT(CPU_CONFIG_8502.base_frequency == 2000000, "8502 2MHz frequency");
    
    return true;
}

// Test configuration validation
bool test_configuration_validation() {
    cpu_config_t test_config;
    
    // Test valid configurations
    test_config = CPU_CONFIG_6502;
    TEST_ASSERT(cpu_config_validate(&test_config), "Valid 6502 config validates");
    
    test_config = CPU_CONFIG_6507;
    TEST_ASSERT(cpu_config_validate(&test_config), "Valid 6507 config validates");
    
    test_config = CPU_CONFIG_6510;
    TEST_ASSERT(cpu_config_validate(&test_config), "Valid 6510 config validates");
    
    test_config = CPU_CONFIG_8502;
    TEST_ASSERT(cpu_config_validate(&test_config), "Valid 8502 config validates");
    
    // Test invalid configurations
    TEST_ASSERT(!cpu_config_validate(NULL), "NULL config fails validation");
    
    // Test invalid variant
    test_config = CPU_CONFIG_6502;
    test_config.cpu_variant = 99;  // Invalid variant
    TEST_ASSERT(!cpu_config_validate(&test_config), "Invalid variant fails validation");
    
    // Test invalid address lines
    test_config = CPU_CONFIG_6502;
    test_config.address_lines = 12;  // Too few
    TEST_ASSERT(!cpu_config_validate(&test_config), "Too few address lines fails validation");
    
    test_config.address_lines = 20;  // Too many
    TEST_ASSERT(!cpu_config_validate(&test_config), "Too many address lines fails validation");
    
    // Test invalid frequency
    test_config = CPU_CONFIG_6502;
    test_config.base_frequency = 0;  // Zero frequency
    TEST_ASSERT(!cpu_config_validate(&test_config), "Zero frequency fails validation");
    
    test_config.base_frequency = 100000000;  // Too high
    TEST_ASSERT(!cpu_config_validate(&test_config), "Too high frequency fails validation");
    
    // Test variant-specific validation
    test_config = CPU_CONFIG_6502;
    test_config.has_io_port = true;  // 6502 shouldn't have I/O port
    TEST_ASSERT(!cpu_config_validate(&test_config), "6502 with I/O port fails validation");
    
    test_config = CPU_CONFIG_6507;
    test_config.has_irq_pin = true;  // 6507 shouldn't have IRQ pin
    TEST_ASSERT(!cpu_config_validate(&test_config), "6507 with IRQ pin fails validation");
    
    return true;
}

// Test compatibility checking
bool test_compatibility_checking() {
    TEST_ASSERT(cpu_config_is_compatible(&CPU_CONFIG_6502, CPU_6502), "6502 config compatible with 6502");
    TEST_ASSERT(!cpu_config_is_compatible(&CPU_CONFIG_6502, CPU_6510), "6502 config not compatible with 6510");
    TEST_ASSERT(!cpu_config_is_compatible(NULL, CPU_6502), "NULL config not compatible");
    
    return true;
}

// Test variant defaults
bool test_variant_defaults() {
    cpu_config_t test_config;
    
    // Test applying defaults
    cpu_config_apply_variant_defaults(&test_config, CPU_6502);
    TEST_ASSERT(test_config.cpu_variant == CPU_6502, "6502 defaults applied correctly");
    TEST_ASSERT(cpu_config_validate(&test_config), "6502 defaults are valid");
    
    cpu_config_apply_variant_defaults(&test_config, CPU_6507);
    TEST_ASSERT(test_config.cpu_variant == CPU_6507, "6507 defaults applied correctly");
    TEST_ASSERT(cpu_config_validate(&test_config), "6507 defaults are valid");
    
    cpu_config_apply_variant_defaults(&test_config, CPU_6510);
    TEST_ASSERT(test_config.cpu_variant == CPU_6510, "6510 defaults applied correctly");
    TEST_ASSERT(cpu_config_validate(&test_config), "6510 defaults are valid");
    
    cpu_config_apply_variant_defaults(&test_config, CPU_8502);
    TEST_ASSERT(test_config.cpu_variant == CPU_8502, "8502 defaults applied correctly");
    TEST_ASSERT(cpu_config_validate(&test_config), "8502 defaults are valid");
    
    return true;
}

// Test utility functions
bool test_utility_functions() {
    // Test variant names
    TEST_ASSERT(strcmp(cpu_config_get_variant_name(CPU_6502), "MOS 6502") == 0, "6502 name correct");
    TEST_ASSERT(strcmp(cpu_config_get_variant_name(CPU_6507), "MOS 6507") == 0, "6507 name correct");
    TEST_ASSERT(strcmp(cpu_config_get_variant_name(CPU_6510), "MOS 6510") == 0, "6510 name correct");
    TEST_ASSERT(strcmp(cpu_config_get_variant_name(CPU_8502), "MOS 8502") == 0, "8502 name correct");
    
    // Test address masks
    TEST_ASSERT(cpu_config_get_address_mask(&CPU_CONFIG_6502) == 0xFFFF, "6502 address mask correct");
    TEST_ASSERT(cpu_config_get_address_mask(&CPU_CONFIG_6507) == 0x1FFF, "6507 address mask correct");
    TEST_ASSERT(cpu_config_get_address_mask(&CPU_CONFIG_6510) == 0xFFFF, "6510 address mask correct");
    TEST_ASSERT(cpu_config_get_address_mask(&CPU_CONFIG_8502) == 0xFFFF, "8502 address mask correct");
    
    // Test pin support
    TEST_ASSERT(cpu_config_supports_pin(&CPU_CONFIG_6502, "IRQ"), "6502 supports IRQ");
    TEST_ASSERT(!cpu_config_supports_pin(&CPU_CONFIG_6507, "IRQ"), "6507 doesn't support IRQ");
    TEST_ASSERT(cpu_config_supports_pin(&CPU_CONFIG_6510, "AEC"), "6510 supports AEC");
    TEST_ASSERT(!cpu_config_supports_pin(&CPU_CONFIG_6502, "AEC"), "6502 doesn't support AEC");
    TEST_ASSERT(cpu_config_supports_pin(&CPU_CONFIG_6502, "RDY"), "All variants support RDY");
    
    return true;
}

// Main test function
int main() {
    printf("=== MOS6510 Cycle-Accurate CPU Configuration System Tests ===\n");
    
    TEST_FUNCTION(test_predefined_configurations);
    TEST_FUNCTION(test_configuration_validation);
    TEST_FUNCTION(test_compatibility_checking);
    TEST_FUNCTION(test_variant_defaults);
    TEST_FUNCTION(test_utility_functions);
    
    printf("\n=== ALL TESTS PASSED ===\n");
    printf("CPU Configuration System implementation is complete and functional.\n");
    
    printf("\nConfiguration Summary:\n");
    printf("- MOS 6502: %s, %d address lines, %s illegal ops\n", 
           cpu_config_get_variant_name(CPU_6502), 
           CPU_CONFIG_6502.address_lines,
           CPU_CONFIG_6502.supports_illegal_ops ? "supports" : "no");
    printf("- MOS 6507: %s, %d address lines, no interrupts\n",
           cpu_config_get_variant_name(CPU_6507),
           CPU_CONFIG_6507.address_lines);
    printf("- MOS 6510: %s, %s I/O port, %s AEC pin\n",
           cpu_config_get_variant_name(CPU_6510),
           CPU_CONFIG_6510.has_io_port ? "has" : "no",
           CPU_CONFIG_6510.has_aec_pin ? "has" : "no");
    printf("- MOS 8502: %s, %.2f MHz, %s clock\n",
           cpu_config_get_variant_name(CPU_8502),
           CPU_CONFIG_8502.base_frequency / 1000000.0,
           CPU_CONFIG_8502.variable_clock ? "variable" : "fixed");
    
    return 0;
}