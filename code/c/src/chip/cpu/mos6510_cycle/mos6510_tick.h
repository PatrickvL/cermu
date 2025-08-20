#ifndef MOS6510_CYCLE_TICK_H
#define MOS6510_CYCLE_TICK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * MOS6510 Core Tick Function Implementation
 * 
 * Implements the main CPU tick function as specified in the visual6502 analysis
 * (spec lines 358-386). This function coordinates all CPU subsystems for
 * cycle-accurate execution with hardware-perfect timing.
 * 
 * Core Requirements:
 * - φ1 phase deferred operation execution (spec lines 359-365)
 * - φ2 phase interrupt recognition update (spec lines 366-372) 
 * - Timing state machine advancement (spec lines 373-378)
 * - PLA decode and cycle execution setup (spec lines 379-382)
 * - Address bus setup as final operation (spec lines 383-386)
 * - Complete pipeline coordination and overlap handling
 * - Hardware-accurate bus timing and latching
 */

// Forward declarations
struct mos6510_state_s;
struct deferred_ops_state_s;
struct interrupt_recognition_state_s;
struct pipeline_state_s;

// ===== TICK EXECUTION PHASES =====

/**
 * CPU tick execution phases
 * Matches the hardware φ1/φ2 clock phases with proper timing coordination
 */
typedef enum {
    TICK_PHASE_PHI1 = 0,    // φ1 phase - execute deferred operations
    TICK_PHASE_PHI2 = 1,    // φ2 phase - interrupt recognition, address setup
    TICK_PHASE_IDLE = 2,    // CPU idle (for debugging/testing)
    TICK_PHASE_ERROR = 3    // Error state
} tick_phase_t;

/**
 * Tick execution statistics for performance monitoring
 */
typedef struct {
    uint64_t total_ticks;           // Total CPU ticks executed
    uint64_t phi1_operations;       // Operations executed in φ1 phase
    uint64_t phi2_interrupts;       // Interrupt checks in φ2 phase
    uint64_t timing_advances;       // Timing state advances
    uint64_t pla_lookups;          // PLA decode operations
    uint64_t address_setups;       // Address bus setups
    uint64_t pipeline_stalls;      // Pipeline stall cycles
    uint64_t rdy_stall_cycles;     // RDY line stall cycles
    
    // Performance metrics
    uint64_t cycle_start_time;     // Tick start timestamp
    uint64_t cycle_end_time;       // Tick end timestamp
    uint32_t longest_tick_cycles;  // Longest single tick duration
    uint32_t shortest_tick_cycles; // Shortest single tick duration
} tick_statistics_t;

/**
 * Tick execution context
 * Maintains state across all tick execution phases
 */
typedef struct {
    // Current execution context
    tick_phase_t current_phase;        // Current tick phase
    tick_phase_t next_phase;          // Next tick phase
    bool phase_transition_pending;     // Phase transition is pending
    
    // Cycle coordination
    uint64_t current_cycle;           // Current cycle number
    bool cycle_complete;              // Current cycle is complete
    bool next_cycle_ready;            // Next cycle is ready to start
    
    // Operation coordination
    bool deferred_ops_executed;      // φ1 deferred operations completed
    bool interrupts_processed;       // φ2 interrupt recognition completed
    bool timing_advanced;            // Timing state machine advanced
    bool pla_decoded;                // PLA decode completed
    bool address_setup_completed;    // Address bus setup completed
    
    // Error handling
    uint8_t error_count;             // Number of errors in current tick
    bool error_recovery_active;      // Error recovery is active
    
    // Debug and validation
    tick_statistics_t statistics;    // Execution statistics
    bool debug_enabled;              // Enable debug tracing
    bool validation_enabled;         // Enable state validation
} tick_context_t;

// ===== MAIN TICK FUNCTION =====

/**
 * Execute one complete CPU tick cycle
 * 
 * This is the main CPU execution function that coordinates all subsystems
 * for cycle-accurate execution. Implements the exact algorithm specified
 * in the visual6502 analysis (spec lines 358-386).
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @return true if tick executed successfully, false on error
 */
bool mos6510_tick(struct mos6510_state_s* cpu, tick_context_t* context);

/**
 * Initialize tick execution context
 * 
 * @param context Tick context to initialize
 * @param enable_debug Enable debug tracing
 * @param enable_validation Enable state validation
 */
void mos6510_tick_context_init(tick_context_t* context, bool enable_debug, bool enable_validation);

/**
 * Reset tick execution context
 * 
 * @param context Tick context to reset
 */
void mos6510_tick_context_reset(tick_context_t* context);

// ===== TICK PHASE EXECUTION FUNCTIONS =====

/**
 * Execute φ1 phase - deferred operation execution
 * 
 * Executes all deferred operations that were queued during the previous
 * tick. This includes register transfers, ALU operations, and internal
 * bus routing operations.
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @return true if φ1 phase completed successfully
 */
bool mos6510_tick_phi1_phase(struct mos6510_state_s* cpu, tick_context_t* context);

/**
 * Execute φ2 phase - interrupt recognition and address setup
 * 
 * Updates interrupt recognition state, advances timing state machine,
 * performs PLA decode for next cycle, and sets up address bus as the
 * final operation.
 * 
 * @param cpu CPU state structure  
 * @param context Tick execution context
 * @return true if φ2 phase completed successfully
 */
bool mos6510_tick_phi2_phase(struct mos6510_state_s* cpu, tick_context_t* context);

// ===== TICK COORDINATION FUNCTIONS =====

/**
 * Advance timing state machine
 * 
 * Advances the CPU timing state based on the current instruction cycle
 * and handles timing state transitions including pipeline overlaps.
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @return true if timing advance completed successfully
 */
bool mos6510_tick_advance_timing(struct mos6510_state_s* cpu, tick_context_t* context);

/**
 * Perform PLA decode for current cycle
 * 
 * Decodes the current instruction and timing state to determine the
 * operations to be performed in the current cycle.
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @return true if PLA decode completed successfully
 */
bool mos6510_tick_pla_decode(struct mos6510_state_s* cpu, tick_context_t* context);

/**
 * Update interrupt recognition state
 * 
 * Processes interrupt signals through the 4-stage interrupt recognition
 * system and handles NMI skipping conditions.
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @return true if interrupt processing completed successfully
 */
bool mos6510_tick_process_interrupts(struct mos6510_state_s* cpu, tick_context_t* context);

/**
 * Set up address bus (final operation)
 * 
 * Sets up the address bus as the final operation of the tick cycle.
 * This is critical for external framework compatibility.
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @return true if address setup completed successfully
 */
bool mos6510_tick_setup_address_bus(struct mos6510_state_s* cpu, tick_context_t* context);

// ===== PIPELINE COORDINATION =====

/**
 * Update pipeline state
 * 
 * Advances the instruction pipeline state including fetch, decode,
 * execute, and writeback stages with proper overlap handling.
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @return true if pipeline update completed successfully
 */
bool mos6510_tick_update_pipeline(struct mos6510_state_s* cpu, tick_context_t* context);

/**
 * Handle pipeline stalls and flushes
 * 
 * Manages pipeline stalls due to RDY line, interrupt processing,
 * or branch mispredictions.
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @return true if pipeline handling completed successfully
 */
bool mos6510_tick_handle_pipeline_stalls(struct mos6510_state_s* cpu, tick_context_t* context);

// ===== VALIDATION AND DEBUGGING =====

/**
 * Validate tick execution state
 * 
 * Performs comprehensive validation of CPU state after tick execution
 * to ensure correctness and detect errors early.
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @return true if validation passed, false if errors detected
 */
bool mos6510_tick_validate_state(const struct mos6510_state_s* cpu, const tick_context_t* context);

/**
 * Generate tick execution trace
 * 
 * Creates a human-readable trace of tick execution for debugging
 * and analysis purposes.
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @param buffer Output buffer for trace
 * @param buffer_size Size of output buffer
 */
void mos6510_tick_generate_trace(const struct mos6510_state_s* cpu, 
                                const tick_context_t* context,
                                char* buffer, size_t buffer_size);

/**
 * Get tick execution statistics
 * 
 * @param context Tick execution context
 * @return Pointer to statistics structure
 */
const tick_statistics_t* mos6510_tick_get_statistics(const tick_context_t* context);

/**
 * Reset tick execution statistics
 * 
 * @param context Tick execution context
 */
void mos6510_tick_reset_statistics(tick_context_t* context);

// ===== ERROR HANDLING =====

/**
 * Handle tick execution errors
 * 
 * Manages error conditions during tick execution and attempts
 * recovery when possible.
 * 
 * @param cpu CPU state structure
 * @param context Tick execution context
 * @param error_phase Phase where error occurred
 * @return true if error was handled successfully
 */
bool mos6510_tick_handle_error(struct mos6510_state_s* cpu, 
                              tick_context_t* context, 
                              tick_phase_t error_phase);

/**
 * Get tick phase name for debugging
 * 
 * @param phase Tick phase
 * @return Human-readable phase name
 */
const char* mos6510_tick_phase_name(tick_phase_t phase);

#endif // MOS6510_CYCLE_TICK_H