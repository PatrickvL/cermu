#ifndef DEFERRED_OPS_H
#define DEFERRED_OPS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * MOS6510 Deferred Operation Architecture
 * 
 * Implements hardware-accurate φ1/φ2 phase execution timing according to 
 * visual6502.org analysis (spec lines 353, 534-549, 552-554).
 * 
 * Key Requirements:
 * - Address setup must be last operation (spec lines 353, 534-536)
 * - Deferred data operations until next tick (spec lines 541-549)
 * - φ1/φ2 phase-accurate execution flow (spec lines 358-386)
 * - RDY line handling (read cycles only, spec lines 552-554)
 */

// Forward declarations
struct mos6510_state_s;

// ===== DEFERRED OPERATION TYPES =====

/**
 * Types of operations that can be deferred to φ1 phase
 */
typedef enum {
    DEFERRED_OP_NONE = 0,
    DEFERRED_OP_REGISTER_LOAD,     // Load data into register
    DEFERRED_OP_REGISTER_STORE,    // Store register to data latch
    DEFERRED_OP_ALU_OPERATION,     // Perform ALU operation
    DEFERRED_OP_FLAG_UPDATE,       // Update processor status flags
    DEFERRED_OP_BUS_TRANSFER,      // Internal bus transfer
    DEFERRED_OP_MEMORY_READ,       // Memory read operation
    DEFERRED_OP_MEMORY_WRITE,      // Memory write operation
    DEFERRED_OP_PC_INCREMENT,      // Program counter increment
    DEFERRED_OP_STACK_PUSH,        // Stack push operation
    DEFERRED_OP_STACK_PULL,        // Stack pull operation
    DEFERRED_OP_ADDRESS_CALC,      // Address calculation (except final setup)
    DEFERRED_OP_COUNT              // Number of operation types
} deferred_op_type_t;

/**
 * Deferred operation priority levels
 * Higher values = higher priority (executed first)
 */
typedef enum {
    DEFERRED_PRIORITY_LOW = 0,     // General operations
    DEFERRED_PRIORITY_NORMAL = 1,  // Most operations
    DEFERRED_PRIORITY_HIGH = 2,    // Critical operations
    DEFERRED_PRIORITY_URGENT = 3   // Must execute immediately
} deferred_op_priority_t;

// ===== DEFERRED OPERATION STRUCTURE =====

/**
 * Individual deferred operation
 */
typedef struct {
    deferred_op_type_t type;       // Operation type
    deferred_op_priority_t priority; // Execution priority
    
    // Operation parameters
    uint8_t source_reg;            // Source register index (0-15)
    uint8_t dest_reg;              // Destination register index (0-15)
    uint8_t data;                  // Immediate data value
    uint16_t address;              // Memory address
    
    // Operation flags
    bool is_memory_op;             // Involves memory access
    bool is_read_cycle;            // Read operation (affects RDY handling)
    bool is_write_cycle;           // Write operation
    bool updates_flags;            // Updates processor status flags
    bool affects_pc;               // Affects program counter
    
    // Timing information
    uint8_t defer_cycles;          // Number of cycles to defer
    uint8_t remaining_cycles;      // Cycles remaining before execution
} deferred_operation_t;

// ===== DEFERRED OPERATION QUEUE =====

#define DEFERRED_OP_QUEUE_SIZE 8   // Maximum deferred operations

/**
 * Deferred operation queue state
 */
typedef struct {
    deferred_operation_t operations[DEFERRED_OP_QUEUE_SIZE];
    uint8_t head;                  // Queue head index
    uint8_t tail;                  // Queue tail index
    uint8_t count;                 // Current number of operations
    bool queue_full;               // Queue overflow flag
    
    // Queue statistics
    uint32_t total_operations;     // Total operations processed
    uint32_t peak_queue_depth;     // Maximum queue depth reached
    uint32_t queue_overflows;      // Number of queue overflows
} deferred_op_queue_t;

// ===== DEFERRED OPERATION MANAGER STATE =====

/**
 * Complete deferred operation management state
 */
typedef struct {
    // Operation queue
    deferred_op_queue_t queue;
    
    // Phase coordination
    bool phi1_phase_active;        // Currently in φ1 phase
    bool phi2_phase_active;        // Currently in φ2 phase
    bool address_setup_deferred;   // Address setup is waiting
    
    // RDY line handling (spec lines 552-554)
    bool rdy_line_state;           // Current RDY line state
    bool rdy_affects_cycle;        // RDY affects current cycle
    bool waiting_for_rdy;          // CPU stalled waiting for RDY
    
    // Address setup (must be last - spec lines 534-536)
    uint16_t deferred_address;     // Address to set up in φ2
    bool address_setup_pending;    // Address setup is pending
    bool address_setup_enabled;   // Address setup is allowed
    
    // Execution coordination
    uint32_t current_cycle;        // Current execution cycle
    uint32_t deferred_until_cycle; // Defer operations until this cycle
    bool execution_stalled;        // Execution is stalled
    
    // Debug and validation
    uint32_t phi1_operations_executed; // Operations executed in φ1
    uint32_t phi2_address_setups;      // Address setups in φ2
    uint32_t rdy_stall_cycles;         // Cycles stalled by RDY
} deferred_ops_state_t;

// ===== CORE DEFERRED OPERATION FUNCTIONS =====

/**
 * Initialize deferred operation system
 */
void deferred_ops_init(deferred_ops_state_t* deferred_state);

/**
 * Reset deferred operation system
 */
void deferred_ops_reset(deferred_ops_state_t* deferred_state);

/**
 * Add operation to deferred queue
 */
bool deferred_ops_enqueue(deferred_ops_state_t* deferred_state, 
                         const deferred_operation_t* op);

/**
 * Execute all φ1 phase deferred operations
 * Called during φ1 phase of tick function
 */
void deferred_ops_execute_phi1(deferred_ops_state_t* deferred_state, 
                              struct mos6510_state_s* cpu);

/**
 * Perform final address setup during φ2 phase
 * Called at end of tick function - MUST be last operation
 */
void deferred_ops_address_setup_phi2(deferred_ops_state_t* deferred_state, 
                                     struct mos6510_state_s* cpu);

/**
 * Update φ1/φ2 phase state
 */
void deferred_ops_set_phase(deferred_ops_state_t* deferred_state, 
                           bool phi1_active, bool phi2_active);

// ===== OPERATION QUEUE MANAGEMENT =====

/**
 * Create deferred operation with parameters
 */
deferred_operation_t deferred_ops_create(deferred_op_type_t type,
                                        deferred_op_priority_t priority,
                                        uint8_t source_reg, uint8_t dest_reg,
                                        uint8_t data, uint16_t address);

/**
 * Check if queue has operations ready for execution
 */
bool deferred_ops_has_ready_operations(const deferred_ops_state_t* deferred_state);

/**
 * Get next operation for execution (priority-based)
 */
bool deferred_ops_dequeue(deferred_ops_state_t* deferred_state, 
                         deferred_operation_t* op);

/**
 * Clear all operations from queue
 */
void deferred_ops_clear_queue(deferred_ops_state_t* deferred_state);

// ===== RDY LINE HANDLING (SPEC LINES 552-554) =====

/**
 * Update RDY line state
 * RDY only affects read cycles, not write cycles
 */
void deferred_ops_set_rdy_line(deferred_ops_state_t* deferred_state, bool rdy_active);

/**
 * Check if CPU should stall due to RDY line
 * Only applies to read cycles
 */
bool deferred_ops_should_stall_for_rdy(const deferred_ops_state_t* deferred_state);

/**
 * Handle RDY line during operation execution
 */
bool deferred_ops_process_rdy_stall(deferred_ops_state_t* deferred_state, 
                                   const deferred_operation_t* op);

// ===== ADDRESS SETUP COORDINATION =====

/**
 * Defer address setup until φ2 phase (must be last operation)
 */
void deferred_ops_defer_address_setup(deferred_ops_state_t* deferred_state, 
                                      uint16_t address);

/**
 * Check if address setup is pending
 */
bool deferred_ops_address_setup_pending(const deferred_ops_state_t* deferred_state);

/**
 * Execute pending address setup (φ2 phase only)
 */
void deferred_ops_execute_address_setup(deferred_ops_state_t* deferred_state, 
                                       struct mos6510_state_s* cpu);

// ===== OPERATION HELPERS =====

/**
 * Create common deferred operations
 */
deferred_operation_t deferred_ops_register_load(uint8_t dest_reg, uint8_t data);
deferred_operation_t deferred_ops_register_store(uint8_t source_reg, uint16_t address);
deferred_operation_t deferred_ops_memory_read(uint16_t address, uint8_t dest_reg);
deferred_operation_t deferred_ops_memory_write(uint16_t address, uint8_t source_reg);
deferred_operation_t deferred_ops_alu_operation(uint8_t source_reg, uint8_t dest_reg);
deferred_operation_t deferred_ops_flag_update(uint8_t flag_mask, bool set_flags);

// ===== VALIDATION AND DEBUGGING =====

/**
 * Validate deferred operation state consistency
 */
bool deferred_ops_validate(const deferred_ops_state_t* deferred_state);

/**
 * Get operation queue status
 */
void deferred_ops_get_queue_status(const deferred_ops_state_t* deferred_state,
                                  uint8_t* current_count, uint8_t* peak_depth,
                                  uint32_t* total_ops, uint32_t* overflows);

/**
 * Get execution statistics
 */
void deferred_ops_get_statistics(const deferred_ops_state_t* deferred_state,
                                uint32_t* phi1_ops, uint32_t* phi2_setups,
                                uint32_t* rdy_stalls, uint32_t* total_cycles);

/**
 * Dump deferred operation state for debugging
 */
void deferred_ops_dump(const deferred_ops_state_t* deferred_state, 
                      char* buffer, size_t buffer_size);

/**
 * Get operation type name for debugging
 */
const char* deferred_ops_type_name(deferred_op_type_t type);

/**
 * Get operation priority name for debugging  
 */
const char* deferred_ops_priority_name(deferred_op_priority_t priority);

#endif // DEFERRED_OPS_H