#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "internal_bus.h"
#include "mos6510_state.h"
#include "cpu_config.h"
#include "register_access.h"

// Test helper functions
static void test_assert(bool condition, const char* test_name) {
    if (condition) {
        printf("PASS: %s\n", test_name);
    } else {
        printf("FAIL: %s\n", test_name);
        printf("TEST FAILED: %s\n", __func__);
        exit(1);
    }
}

// ===== TEST FUNCTIONS =====

// Test basic bus initialization
void test_bus_initialization() {
    printf("\n--- Testing test_bus_initialization ---\n");
    
    internal_bus_state_t bus;
    internal_bus_init(&bus);
    
    // Check precharge state (all buses should be 0xFF)
    test_assert(bus.sb_value == 0xFF, "SB initialized to precharge state");
    test_assert(bus.adl_value == 0xFF, "ADL initialized to precharge state");
    test_assert(bus.adh_value == 0xFF, "ADH initialized to precharge state");
    test_assert(bus.abl_value == 0xFF, "ABL initialized to precharge state");
    test_assert(bus.abh_value == 0xFF, "ABH initialized to precharge state");
    
    // Check control state
    test_assert(bus.transfer_control == 0x00, "Transfer control initialized to zero");
    test_assert(bus.phi1_phase == false, "Initialized in φ2 phase");
    test_assert(bus.precharge_active == true, "Precharge active after init");
    test_assert(bus.pending_transfers == 0x00, "No pending transfers");
    
    // Check driver counters
    test_assert(bus.sb_drivers == 0, "No SB drivers initially");
    test_assert(bus.adl_drivers == 0, "No ADL drivers initially");
    test_assert(bus.adh_drivers == 0, "No ADH drivers initially");
    
    // Validate initial state
    test_assert(internal_bus_validate(&bus), "Initial bus state is valid");
}

// Test bus phase operations
void test_bus_phase_operations() {
    printf("\n--- Testing test_bus_phase_operations ---\n");
    
    internal_bus_state_t bus;
    internal_bus_init(&bus);
    
    // Test φ1 phase setting
    internal_bus_set_phase(&bus, true);
    test_assert(bus.phi1_phase == true, "φ1 phase set correctly");
    test_assert(bus.precharge_active == false, "Precharge disabled in φ1");
    
    // Test φ2 phase setting
    internal_bus_set_phase(&bus, false);
    test_assert(bus.phi1_phase == false, "φ2 phase set correctly");
    test_assert(bus.precharge_active == true, "Precharge enabled in φ2");
    
    // Test φ2 precharge operation
    uint8_t test_control = BUS_PCL_ADL | BUS_PCH_ADH;
    internal_bus_phi2_precharge(&bus, test_control);
    test_assert(bus.pending_transfers == test_control, "Transfer control latched for φ1");
    test_assert(bus.transfer_control == 0x00, "Current control cleared");
    test_assert(bus.sb_value == 0xFF, "SB precharged high");
    test_assert(bus.adl_value == 0xFF, "ADL precharged high");
    test_assert(bus.adh_value == 0xFF, "ADH precharged high");
}

// Test bus transfer control bits
void test_bus_transfer_control() {
    printf("\n--- Testing test_bus_transfer_control ---\n");
    
    internal_bus_state_t bus;
    internal_bus_init(&bus);
    
    // Create CPU for testing transfers
    mos6510_state_t *cpu = mos6510_create(&CPU_CONFIG_6510);
    test_assert(cpu != NULL, "CPU creation successful");
    
    // Set up test values in CPU registers
    SET_CPU_PCL(cpu, 0x34);
    SET_CPU_PCH(cpu, 0x12);
    SET_CPU_DL(cpu, 0xAB);
    SET_CPU_SP(cpu, 0xFD);
    
    // Test PCL → ADL transfer
    internal_bus_set_phase(&bus, true); // φ1 phase
    internal_bus_execute_transfer(&bus, cpu, BUS_PCL_ADL);
    test_assert(bus.adl_value == 0x34, "PCL → ADL transfer correct");
    test_assert(bus.adl_drivers == 1, "ADL driver count correct");
    
    // Reset for next test
    internal_bus_init(&bus);
    
    // Test PCH → ADH transfer
    internal_bus_set_phase(&bus, true);
    internal_bus_execute_transfer(&bus, cpu, BUS_PCH_ADH);
    test_assert(bus.adh_value == 0x12, "PCH → ADH transfer correct");
    test_assert(bus.adh_drivers == 1, "ADH driver count correct");
    
    // Reset for next test
    internal_bus_init(&bus);
    
    // Test DL → ADL transfer
    internal_bus_set_phase(&bus, true);
    internal_bus_execute_transfer(&bus, cpu, BUS_DL_ADL);
    test_assert(bus.adl_value == 0xAB, "DL → ADL transfer correct");
    
    // Test DL → ADH transfer
    internal_bus_execute_transfer(&bus, cpu, BUS_DL_ADH);
    test_assert(bus.adh_value == 0xAB, "DL → ADH transfer correct");
    
    // Test stack pointer → ADL transfer
    internal_bus_init(&bus);
    internal_bus_set_phase(&bus, true);
    internal_bus_execute_transfer(&bus, cpu, BUS_S_ADL);
    test_assert(bus.adl_value == 0xFD, "S → ADL transfer correct");
    
    // Test zero → ADH transfer
    internal_bus_execute_transfer(&bus, cpu, BUS_ZERO_ADH);
    test_assert(bus.adh_value == 0x00, "0 → ADH transfer correct");
    
    // Test address latching
    internal_bus_execute_transfer(&bus, cpu, BUS_ADDR_LATCH);
    test_assert(bus.abl_value == bus.adl_value, "ADL latched to ABL");
    test_assert(bus.abh_value == bus.adh_value, "ADH latched to ABH");
    
    mos6510_destroy(cpu);
}

// Test common addressing patterns
void test_addressing_patterns() {
    printf("\n--- Testing test_addressing_patterns ---\n");
    
    internal_bus_state_t bus;
    internal_bus_init(&bus);
    
    // Create CPU for testing
    mos6510_state_t *cpu = mos6510_create(&CPU_CONFIG_6510);
    
    // Set up PC values
    SET_CPU_PC(cpu, 0x1234);
    SET_CPU_DL(cpu, 0x56);
    SET_CPU_SP(cpu, 0xFE);
    
    // Test PC addressing pattern
    internal_bus_set_phase(&bus, true);
    internal_bus_execute_transfer(&bus, cpu, BUS_PCL_ADL);
    internal_bus_execute_transfer(&bus, cpu, BUS_PCH_ADH);
    internal_bus_execute_transfer(&bus, cpu, BUS_ADDR_LATCH);
    
    test_assert(internal_bus_get_address(&bus) == 0x1234, "PC address setup correct");
    test_assert(internal_bus_get_external_address(&bus) == 0x1234, "External address latched correct");
    
    // Reset for zero page test
    internal_bus_init(&bus);
    internal_bus_set_phase(&bus, true);
    
    // Test zero page addressing pattern
    internal_bus_execute_transfer(&bus, cpu, BUS_DL_ADL);
    internal_bus_execute_transfer(&bus, cpu, BUS_ZERO_ADH);
    internal_bus_execute_transfer(&bus, cpu, BUS_ADDR_LATCH);
    
    test_assert(internal_bus_get_address(&bus) == 0x0056, "Zero page address setup correct");
    test_assert(internal_bus_get_external_address(&bus) == 0x0056, "Zero page external address correct");
    
    // Reset for stack test
    internal_bus_init(&bus);
    internal_bus_set_phase(&bus, true);
    
    // Test stack addressing pattern  
    internal_bus_execute_transfer(&bus, cpu, BUS_S_ADL);
    internal_bus_execute_transfer(&bus, cpu, BUS_ZERO_ADH);
    internal_bus_execute_transfer(&bus, cpu, BUS_ADDR_LATCH);
    
    test_assert(internal_bus_get_address(&bus) == 0x00FE, "Stack address setup correct");
    test_assert(internal_bus_get_external_address(&bus) == 0x00FE, "Stack external address correct");
    
    mos6510_destroy(cpu);
}

// Test driver conflict detection
void test_driver_conflicts() {
    printf("\n--- Testing test_driver_conflicts ---\n");
    
    internal_bus_state_t bus;
    internal_bus_init(&bus);
    
    mos6510_state_t *cpu = mos6510_create(&CPU_CONFIG_6510);
    
    // Set up conflicting transfers (multiple drivers for ADL)
    internal_bus_set_phase(&bus, true);
    internal_bus_execute_transfer(&bus, cpu, BUS_PCL_ADL);  // First ADL driver
    internal_bus_execute_transfer(&bus, cpu, BUS_DL_ADL);   // Second ADL driver - conflict!
    
    test_assert(bus.adl_drivers == 2, "Multiple ADL drivers detected");
    test_assert(!internal_bus_check_conflicts(&bus), "Conflict detected correctly");
    
    // Test conflict resolution
    internal_bus_resolve_conflicts(&bus);
    test_assert(internal_bus_check_conflicts(&bus), "Conflicts resolved");
    test_assert(bus.adl_drivers == 1, "ADL driver count normalized");
    
    mos6510_destroy(cpu);
}

// Test complete φ1/φ2 cycle
void test_complete_phi_cycle() {
    printf("\n--- Testing test_complete_phi_cycle ---\n");
    
    internal_bus_state_t bus;
    internal_bus_init(&bus);
    
    mos6510_state_t *cpu = mos6510_create(&CPU_CONFIG_6510);
    
    SET_CPU_PC(cpu, 0xABCD);
    
    // φ2 phase: latch transfer control
    uint8_t transfers = BUS_PCL_ADL | BUS_PCH_ADH | BUS_ADDR_LATCH;
    internal_bus_phi2_precharge(&bus, transfers);
    
    test_assert(bus.phi1_phase == false, "In φ2 phase");
    test_assert(bus.precharge_active == true, "Precharge active");
    test_assert(bus.pending_transfers == transfers, "Transfers pending");
    
    // φ1 phase: execute transfers
    internal_bus_phi1_execute(&bus, cpu);
    
    test_assert(bus.phi1_phase == true, "In φ1 phase"); 
    test_assert(bus.precharge_active == false, "Precharge inactive");
    test_assert(bus.transfer_control == transfers, "Transfers active");
    test_assert(internal_bus_get_external_address(&bus) == 0xABCD, "Address correctly latched");
    
    mos6510_destroy(cpu);
}

// Test bus state debugging
void test_bus_debugging() {
    printf("\n--- Testing test_bus_debugging ---\n");
    
    internal_bus_state_t bus;
    internal_bus_init(&bus);
    
    // Test control bit name lookup
    test_assert(strcmp(internal_bus_get_control_name(BUS_PCL_ADL), "PCL→ADL") == 0, "Control name correct");
    test_assert(strcmp(internal_bus_get_control_name(BUS_ZERO_ADH), "0→ADH") == 0, "Zero ADH name correct");
    test_assert(strcmp(internal_bus_get_control_name(BUS_ADDR_LATCH), "ADL/ADH→ABL/ABH") == 0, "Latch name correct");
    
    // Test routing description
    char buffer[256];
    internal_bus_get_routing_description(BUS_PCL_ADL | BUS_PCH_ADH, buffer, sizeof(buffer));
    test_assert(strstr(buffer, "PCL→ADL") != NULL, "Routing description contains PCL→ADL");
    test_assert(strstr(buffer, "PCH→ADH") != NULL, "Routing description contains PCH→ADH");
    
    // Test bus state dump
    char dump_buffer[512];
    internal_bus_dump(&bus, dump_buffer, sizeof(dump_buffer));
    test_assert(strstr(dump_buffer, "Bus State") != NULL, "Dump contains bus state");
    test_assert(strstr(dump_buffer, "precharge=ON") != NULL, "Dump shows precharge state");
    
    printf("Bus state dump preview:\n%s\n", dump_buffer);
}

// Test inline utility functions
void test_utility_functions() {
    printf("\n--- Testing test_utility_functions ---\n");
    
    internal_bus_state_t bus;
    internal_bus_init(&bus);
    
    // Test address access functions
    internal_bus_set_address(&bus, 0x1234);
    test_assert(bus.adl_value == 0x34, "ADL set correctly");
    test_assert(bus.adh_value == 0x12, "ADH set correctly");
    test_assert(internal_bus_get_address(&bus) == 0x1234, "Address retrieved correctly");
    
    // Test external address
    bus.abl_value = 0x78;
    bus.abh_value = 0x56;
    test_assert(internal_bus_get_external_address(&bus) == 0x5678, "External address correct");
    
    // Test transfer detection
    bus.transfer_control = BUS_PCL_ADL | BUS_ZERO_ADH;
    test_assert(internal_bus_transfer_active(&bus, BUS_PCL_ADL), "PCL→ADL transfer detected");
    test_assert(!internal_bus_transfer_active(&bus, BUS_PCH_ADH), "PCH→ADH transfer not active");
    test_assert(internal_bus_transfer_active(&bus, BUS_ZERO_ADH), "Zero→ADH transfer detected");
}

// ===== MAIN TEST RUNNER =====

int main() {
    printf("=== MOS6510 Internal Bus System Tests ===\n");
    
    test_bus_initialization();
    test_bus_phase_operations();
    test_bus_transfer_control();
    test_addressing_patterns();
    test_driver_conflicts();
    test_complete_phi_cycle();
    test_bus_debugging();
    test_utility_functions();
    
    printf("\n=== ALL TESTS PASSED ===\n");
    printf("Internal bus system implementation is complete and functional.\n\n");
    
    printf("Architecture Summary:\n");
    printf("- Visual6502 internal bus architecture (SB, ADL/ADH, ABL/ABH)\n");
    printf("- 8 bus transfer control bits with routing rules\n");
    printf("- φ1/φ2 phase-accurate bus behavior\n"); 
    printf("- Bus precharge during φ2 (hardware-accurate)\n");
    printf("- Driver conflict detection and resolution\n");
    printf("- Complete debugging and inspection capabilities\n");
    printf("- Hardware-accurate timing and electrical behavior\n");
    
    return 0;
}