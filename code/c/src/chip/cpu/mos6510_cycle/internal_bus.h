#ifndef MOS6510_CYCLE_INTERNAL_BUS_H
#define MOS6510_CYCLE_INTERNAL_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Visual6502 Internal Bus System
// Based on visual6502.org transistor-level analysis
// Implements hardware-accurate bus routing and transfer control

// ===== BUS TRANSFER CONTROL BITS =====
// These control bits specify which bus transfers occur during a cycle
// Each bit controls a specific routing path through the internal buses

#define BUS_DL_ADL             0x01  // DL → ADL (operand to address low)
#define BUS_DL_ADH             0x02  // DL → ADH (operand to address high)
#define BUS_PCL_ADL            0x04  // PCL → ADL (PC low to address)
#define BUS_PCH_ADH            0x08  // PCH → ADH (PC high to address)
#define BUS_S_ADL              0x10  // S → ADL (stack pointer to address)
#define BUS_ZERO_ADH           0x20  // 0 → ADH (zero page/stack addressing)
#define BUS_SB_ROUTE           0x40  // SB routing (complex internal routing)
#define BUS_ADDR_LATCH         0x80  // ADL/ADH → ABL/ABH (latch to output)

// Convenience macros for common bus operations
#define BUS_PC_TO_ADDR         (BUS_PCL_ADL | BUS_PCH_ADH)
#define BUS_ZERO_PAGE          (BUS_DL_ADL | BUS_ZERO_ADH)
#define BUS_STACK_ADDR         (BUS_S_ADL | BUS_ZERO_ADH)

// ===== BUS STATE STRUCTURE =====
// Represents the current state of all internal buses
// Models the actual hardware bus values and control state

typedef struct {
    // Internal bus values (match register array indices)
    uint8_t sb_value;     // Special Bus current value
    uint8_t adl_value;    // Address Low internal bus value
    uint8_t adh_value;    // Address High internal bus value
    uint8_t abl_value;    // Address Bus Low latch value
    uint8_t abh_value;    // Address Bus High latch value
    
    // Bus control state
    uint8_t transfer_control;   // Active bus transfer control bits
    bool phi1_phase;           // Current clock phase (true = φ1, false = φ2)
    bool precharge_active;     // φ2 precharge state
    
    // Driver conflict detection
    uint8_t sb_drivers;        // Number of active SB drivers
    uint8_t adl_drivers;       // Number of active ADL drivers
    uint8_t adh_drivers;       // Number of active ADH drivers
    
    // Bus transfer queue (for φ1 execution)
    uint8_t pending_transfers; // Transfers queued for next φ1
} internal_bus_state_t;

// ===== FORWARD DECLARATIONS =====
struct mos6510_state_s;  // Forward declaration to avoid circular includes

// ===== BUS INITIALIZATION AND MANAGEMENT =====

// Initialize internal bus system to hardware reset state
void internal_bus_init(internal_bus_state_t *bus);

// Reset all buses to precharge state (φ2 behavior)
void internal_bus_reset(internal_bus_state_t *bus);

// Validate bus state for conflicts and consistency
bool internal_bus_validate(const internal_bus_state_t *bus);

// ===== CLOCK PHASE OPERATIONS =====

// Set current clock phase and update bus behavior accordingly
void internal_bus_set_phase(internal_bus_state_t *bus, bool phi1_phase);

// Execute φ2 phase: precharge all buses, latch transfer control
void internal_bus_phi2_precharge(internal_bus_state_t *bus, uint8_t transfer_control);

// Execute φ1 phase: execute pending transfers, update bus values
void internal_bus_phi1_execute(internal_bus_state_t *bus, struct mos6510_state_s *cpu);

// ===== BUS TRANSFER OPERATIONS =====

// Queue bus transfers for execution during next φ1 phase
void internal_bus_queue_transfers(internal_bus_state_t *bus, uint8_t transfer_control);

// Execute a specific bus transfer (called during φ1)
void internal_bus_execute_transfer(internal_bus_state_t *bus, struct mos6510_state_s *cpu, uint8_t control_bit);

// Update address bus latches (ABL/ABH) from internal address buses (ADL/ADH)
void internal_bus_latch_address(internal_bus_state_t *bus);

// ===== BUS ROUTING FUNCTIONS =====

// Route data through Special Bus (SB) - complex internal routing
void internal_bus_route_sb(internal_bus_state_t *bus, struct mos6510_state_s *cpu, uint8_t data);

// Set up address calculation on ADL/ADH buses
void internal_bus_setup_address(internal_bus_state_t *bus, uint8_t adl_value, uint8_t adh_value);

// Zero the address high bus (for zero page and stack operations)
void internal_bus_zero_adh(internal_bus_state_t *bus);

// ===== DRIVER CONFLICT PREVENTION =====

// Check for electrical conflicts ("at most one driver" rule)
bool internal_bus_check_conflicts(const internal_bus_state_t *bus);

// Count active drivers for a specific bus
uint8_t internal_bus_count_drivers(uint8_t transfer_control, uint8_t bus_mask);

// Resolve bus conflicts (log warning, use first driver)
void internal_bus_resolve_conflicts(internal_bus_state_t *bus);

// ===== BUS STATE ACCESS =====

// Get current 16-bit address from internal buses (ADL/ADH)
static inline uint16_t internal_bus_get_address(const internal_bus_state_t *bus) {
    return (uint16_t)(bus->adl_value | (bus->adh_value << 8));
}

// Get current 16-bit address from external latches (ABL/ABH)
static inline uint16_t internal_bus_get_external_address(const internal_bus_state_t *bus) {
    return (uint16_t)(bus->abl_value | (bus->abh_value << 8));
}

// Set internal address buses directly (for special cases)
static inline void internal_bus_set_address(internal_bus_state_t *bus, uint16_t address) {
    bus->adl_value = address & 0xFF;
    bus->adh_value = (address >> 8) & 0xFF;
}

// ===== DEBUGGING AND INSPECTION =====

// Generate human-readable bus state dump
void internal_bus_dump(const internal_bus_state_t *bus, char *buffer, size_t buffer_size);

// Get transfer control bit names for debugging
const char* internal_bus_get_control_name(uint8_t control_bit);

// Check if specific transfer is active
static inline bool internal_bus_transfer_active(const internal_bus_state_t *bus, uint8_t control_bit) {
    return (bus->transfer_control & control_bit) != 0;
}

// Get bus routing description for debugging
void internal_bus_get_routing_description(uint8_t transfer_control, char *buffer, size_t buffer_size);

#endif // MOS6510_CYCLE_INTERNAL_BUS_H