#ifndef MOS6510_CYCLE_BUS_ROUTING_H
#define MOS6510_CYCLE_BUS_ROUTING_H

#include <stdint.h>
#include <stdbool.h>
#include "internal_bus.h"

// High-Level Bus Routing Operations
// Provides convenient functions for common bus routing patterns
// Based on visual6502 analysis of typical instruction sequences

// ===== FORWARD DECLARATIONS =====
struct mos6510_state_s;

// ===== COMMON ADDRESSING PATTERNS =====

// Set up Program Counter addressing (PCL→ADL, PCH→ADH)
static inline uint8_t bus_route_pc_address(void) {
    return BUS_PCL_ADL | BUS_PCH_ADH;
}

// Set up zero page addressing (DL→ADL, 0→ADH)
static inline uint8_t bus_route_zero_page_address(void) {
    return BUS_DL_ADL | BUS_ZERO_ADH;
}

// Set up stack addressing (S→ADL, 0→ADH)
static inline uint8_t bus_route_stack_address(void) {
    return BUS_S_ADL | BUS_ZERO_ADH;
}

// Set up absolute addressing (DL→ADL for low, DL→ADH for high - called separately)
static inline uint8_t bus_route_absolute_low(void) {
    return BUS_DL_ADL;
}

static inline uint8_t bus_route_absolute_high(void) {
    return BUS_DL_ADH;
}

// Complete absolute addressing (both bytes set up)
static inline uint8_t bus_route_absolute_address(void) {
    return BUS_DL_ADL | BUS_DL_ADH;  // Note: typically done in two cycles
}

// ===== BUS ROUTING EXECUTION FUNCTIONS =====

// Execute PC-relative addressing setup
void bus_route_execute_pc_addressing(internal_bus_state_t *bus, struct mos6510_state_s *cpu);

// Execute zero page addressing setup
void bus_route_execute_zero_page_addressing(internal_bus_state_t *bus, struct mos6510_state_s *cpu, uint8_t zero_page_addr);

// Execute stack addressing setup
void bus_route_execute_stack_addressing(internal_bus_state_t *bus, struct mos6510_state_s *cpu);

// Execute absolute addressing setup (two-phase: low byte, then high byte)
void bus_route_execute_absolute_low(internal_bus_state_t *bus, struct mos6510_state_s *cpu, uint8_t addr_low);
void bus_route_execute_absolute_high(internal_bus_state_t *bus, struct mos6510_state_s *cpu, uint8_t addr_high);

// ===== SPECIAL BUS OPERATIONS =====

// Route data through Special Bus for ALU operations
void bus_route_alu_operation(internal_bus_state_t *bus, struct mos6510_state_s *cpu, uint8_t data);

// Set up address increment for multi-byte operations
void bus_route_increment_address(internal_bus_state_t *bus, struct mos6510_state_s *cpu);

// Handle page boundary crossing detection
bool bus_route_check_page_crossing(const internal_bus_state_t *bus, uint16_t base_addr, uint8_t offset);

// ===== INSTRUCTION-SPECIFIC ROUTING =====

// Load/Store instruction bus routing
void bus_route_load_instruction(internal_bus_state_t *bus, struct mos6510_state_s *cpu, uint8_t addressing_mode);
void bus_route_store_instruction(internal_bus_state_t *bus, struct mos6510_state_s *cpu, uint8_t addressing_mode);

// Branch instruction bus routing
void bus_route_branch_instruction(internal_bus_state_t *bus, struct mos6510_state_s *cpu, bool branch_taken, int8_t offset);

// Interrupt vector bus routing
void bus_route_interrupt_vector(internal_bus_state_t *bus, struct mos6510_state_s *cpu, uint16_t vector_address);

// RMW instruction bus routing (Read-Modify-Write)
void bus_route_rmw_instruction(internal_bus_state_t *bus, struct mos6510_state_s *cpu, uint8_t addressing_mode);

// ===== ADDRESSING MODE CONSTANTS =====

// Addressing mode identifiers for routing functions
typedef enum {
    ADDR_MODE_IMPLIED     = 0,
    ADDR_MODE_IMMEDIATE   = 1,
    ADDR_MODE_ZERO_PAGE   = 2,
    ADDR_MODE_ZERO_PAGE_X = 3,
    ADDR_MODE_ZERO_PAGE_Y = 4,
    ADDR_MODE_ABSOLUTE    = 5,
    ADDR_MODE_ABSOLUTE_X  = 6,
    ADDR_MODE_ABSOLUTE_Y  = 7,
    ADDR_MODE_INDIRECT    = 8,
    ADDR_MODE_INDIRECT_X  = 9,
    ADDR_MODE_INDIRECT_Y  = 10,
    ADDR_MODE_RELATIVE    = 11,
    ADDR_MODE_STACK       = 12
} addressing_mode_t;

// ===== BUS ROUTING VALIDATION =====

// Validate that bus routing is appropriate for the current instruction
bool bus_route_validate_addressing(addressing_mode_t mode, uint8_t transfer_control);

// Check if bus routing will cause page crossing
bool bus_route_will_cross_page(addressing_mode_t mode, uint16_t base_addr, uint8_t index);

// Get expected bus routing for a given addressing mode
uint8_t bus_route_get_expected_routing(addressing_mode_t mode, uint8_t cycle_number);

// ===== DEBUGGING SUPPORT =====

// Get addressing mode name for debugging
const char* bus_route_get_addressing_mode_name(addressing_mode_t mode);

// Analyze current bus routing and provide description
void bus_route_analyze_current_routing(const internal_bus_state_t *bus, char *buffer, size_t buffer_size);

// Validate bus routing against expected pattern
bool bus_route_validate_pattern(const internal_bus_state_t *bus, addressing_mode_t expected_mode, uint8_t cycle);

// ===== CONVENIENCE MACROS =====

// Common routing patterns as macros for performance
#define BUS_ROUTE_PC()           (BUS_PCL_ADL | BUS_PCH_ADH | BUS_ADDR_LATCH)
#define BUS_ROUTE_ZERO_PAGE()    (BUS_DL_ADL | BUS_ZERO_ADH | BUS_ADDR_LATCH)
#define BUS_ROUTE_STACK()        (BUS_S_ADL | BUS_ZERO_ADH | BUS_ADDR_LATCH)
#define BUS_ROUTE_LATCH_ONLY()   (BUS_ADDR_LATCH)

// Check if routing includes address latching
#define BUS_ROUTING_LATCHES_ADDR(control) ((control) & BUS_ADDR_LATCH)

// Check if routing affects specific buses
#define BUS_ROUTING_AFFECTS_ADL(control) ((control) & (BUS_DL_ADL | BUS_PCL_ADL | BUS_S_ADL))
#define BUS_ROUTING_AFFECTS_ADH(control) ((control) & (BUS_DL_ADH | BUS_PCH_ADH | BUS_ZERO_ADH))
#define BUS_ROUTING_AFFECTS_SB(control)  ((control) & BUS_SB_ROUTE)

#endif // MOS6510_CYCLE_BUS_ROUTING_H