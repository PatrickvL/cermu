#ifndef MOS6510_CYCLE_STATE_H
#define MOS6510_CYCLE_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mos6510_registers.h"
#include "cpu_config.h"
#include "internal_bus.h"

// Forward declarations
typedef struct mos6510_state_s mos6510_state_t;

// Avoid symbol collisions with legacy mos6510 by prefixing cycle exports
#define mos6510_create mos6510_cycle_create
#define mos6510_destroy mos6510_cycle_destroy
#define mos6510_init    mos6510_cycle_init
#define mos6510_reset   mos6510_cycle_reset

// MOS6510 CPU State Structure
// Based on visual6502 internal structure (spec lines 62-76)
// All CPU state fits in a register array plus control state
typedef struct mos6510_state_s {
    // Core register array - all hardware registers and latches
    uint8_t registers[MOS6510_REGISTER_COUNT];  // 16-register array (spec line 63)
    
    // Timing and execution state
    uint8_t timing_state;           // Current timing state (T0, T2, etc.)
    uint8_t instruction_register;   // Current opcode in IR
    uint8_t predecode_register;     // Hardware predecode register
    uint8_t cycle_position;         // Position within current instruction
    
    // Interrupt handling state (4-stage recognition)
    uint8_t interrupt_state;        // 4-stage interrupt recognition state
    bool nmi_edge_detected;         // NMI edge detection latch
    
    // Deferred operation state (for external framework compatibility)
    uint8_t deferred_data_operation;    // Data operation to execute next tick
    uint8_t deferred_address_mode;      // Address mode for deferred operation
    uint8_t page_cross_detected;        // Page boundary crossing detection
    
    // Configuration and variant support
    cpu_config_t config;            // CPU configuration (variant, features, etc.)
    
    // Pipeline and timing control
    uint8_t pipeline_state;         // Pipeline overlap state tracking
    uint8_t branch_taken;           // Branch taken flag for optimization
    uint8_t rdy_halt_cycles;        // RDY line halt cycle counter
    
    // Internal bus state (visual6502 bus system)
    internal_bus_state_t internal_bus;  // Complete internal bus system state
    
    // Debug and inspection state
    uint64_t total_cycles;          // Total executed cycles counter
    uint16_t last_pc;               // Last program counter for debugging
    uint8_t last_opcode;            // Last executed opcode for debugging
    bool debug_enabled;             // Enable debug state tracking
} mos6510_state_t;

// CPU state initialization and management
mos6510_state_t* mos6510_create(const cpu_config_t *config);
void mos6510_destroy(mos6510_state_t *cpu);
bool mos6510_init(mos6510_state_t *cpu, const cpu_config_t *config);
void mos6510_reset(mos6510_state_t *cpu);

// Register array access validation
bool mos6510_validate_state(const mos6510_state_t *cpu);
void mos6510_state_dump(const mos6510_state_t *cpu, char *buffer, size_t buffer_size);

// State serialization for save/load
size_t mos6510_state_serialize(const mos6510_state_t *cpu, uint8_t *buffer, size_t buffer_size);
bool mos6510_state_deserialize(mos6510_state_t *cpu, const uint8_t *buffer, size_t buffer_size);

// Utility functions for register access
uint16_t mos6510_get_pc(const mos6510_state_t *cpu);
void mos6510_set_pc(mos6510_state_t *cpu, uint16_t pc);
uint8_t mos6510_get_a(const mos6510_state_t *cpu);
void mos6510_set_a(mos6510_state_t *cpu, uint8_t value);
uint8_t mos6510_get_status(const mos6510_state_t *cpu);
void mos6510_set_status(mos6510_state_t *cpu, uint8_t status);

// Debug and monitoring functions
bool mos6510_is_debug_enabled(const mos6510_state_t *cpu);
void mos6510_set_debug_enabled(mos6510_state_t *cpu, bool enabled);
uint64_t mos6510_get_cycle_count(const mos6510_state_t *cpu);
void mos6510_reset_cycle_count(mos6510_state_t *cpu);

#endif // MOS6510_CYCLE_STATE_H