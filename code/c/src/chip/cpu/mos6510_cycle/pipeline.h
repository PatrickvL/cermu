#ifndef MOS6510_CYCLE_PIPELINE_H
#define MOS6510_CYCLE_PIPELINE_H

#include "timing_states.h"
#include "instruction_table.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * MOS6510 Advanced Pipeline Implementation
 * Based on visual6502.org "hidden pipeline" discovery
 * 
 * This implements the sophisticated multi-stage overlapping execution
 * discovered through transistor-level analysis: "The datapath is a bit behind"
 */

// ===== PIPELINE STAGE DEFINITIONS =====

/**
 * Pipeline stages discovered in visual6502 analysis
 */
typedef enum {
    PIPELINE_IDLE       = 0,    // No active pipeline stage
    PIPELINE_FETCH      = 1,    // Predictive fetch stage
    PIPELINE_PREDECODE  = 2,    // Hardware predecode stage
    PIPELINE_DECODE     = 3,    // Delayed decode stage (T0/T1 lag)
    PIPELINE_EXECUTE    = 4,    // Execution stage
    PIPELINE_WRITEBACK  = 5,    // Overlapped writeback stage
    PIPELINE_COMPLETE   = 6     // Instruction completion
} pipeline_stage_t;

/**
 * Pipeline stage properties
 */
typedef struct {
    pipeline_stage_t stage;         // Current pipeline stage
    uint8_t opcode;                 // Opcode in this stage
    uint8_t cycle_position;         // Cycle within instruction
    bool stage_active;              // Stage is actively processing
    bool stage_stalled;             // Stage is stalled waiting
    uint8_t delay_cycles;           // Remaining delay cycles
} pipeline_stage_state_t;

// ===== PIPELINE OVERLAP MANAGER =====

/**
 * Multi-stage pipeline with overlapping execution
 * Models the "datapath behind" behavior discovered in visual6502
 */
typedef struct {
    pipeline_stage_state_t stages[7];      // All pipeline stages
    uint8_t active_stage_count;            // Number of active stages
    
    // Predictive fetch state
    uint8_t prefetch_opcode;               // Next instruction prefetch
    bool prefetch_valid;                   // Prefetch data valid
    uint16_t prefetch_pc;                  // Prefetch PC value
    
    // Delayed decode state  
    uint8_t decode_delay_cycles;           // T0/T1 decode delay
    const instruction_definition_ultra_t *delayed_instruction; // Delayed decode result
    
    // Lagged datapath state
    uint8_t datapath_lag_cycles;           // 1-2 cycle datapath delay
    uint8_t lagged_alu_operation;          // Delayed ALU operation
    uint8_t lagged_register_target;        // Delayed register target
    
    // Overlapped writeback state
    bool writeback_pending;                // Writeback operation pending
    uint8_t writeback_register;            // Target register for writeback
    uint8_t writeback_value;               // Value to write back
    bool writeback_affects_flags;          // Writeback affects processor flags
    
    // Pipeline synchronization
    bool pipeline_flush_requested;        // Flush pipeline (interrupts/branches)
    bool pipeline_stall_requested;        // Stall pipeline (memory wait)
    uint8_t synchronization_cycle_count;   // Cycles to synchronize pipeline
} pipeline_overlap_manager_t;

// ===== PREDICTIVE FETCH MECHANISM =====

/**
 * Initialize pipeline overlap manager
 */
void pipeline_init(pipeline_overlap_manager_t *pom);

/**
 * Predictive fetch: "I/PC peeks ahead to the next instruction that is predecoded"
 */
void pipeline_predictive_fetch(pipeline_overlap_manager_t *pom, uint16_t current_pc,
                              uint8_t (*fetch_callback)(uint16_t address));

/**
 * Check if predictive fetch is valid and ready
 */
static inline bool pipeline_prefetch_ready(const pipeline_overlap_manager_t *pom) {
    return pom->prefetch_valid;
}

/**
 * Get predictively fetched opcode
 */
static inline uint8_t pipeline_get_prefetch_opcode(const pipeline_overlap_manager_t *pom) {
    return pom->prefetch_opcode;
}

// ===== DELAYED DECODE IMPLEMENTATION =====

/**
 * Delayed decode: "T0 and T1 inputs to the PLA actually come behind everything else"
 */
void pipeline_delayed_decode(pipeline_overlap_manager_t *pom, uint8_t opcode);

/**
 * Check if delayed decode is complete
 */
static inline bool pipeline_decode_ready(const pipeline_overlap_manager_t *pom) {
    return pom->decode_delay_cycles == 0 && pom->delayed_instruction != NULL;
}

/**
 * Get delayed decode instruction definition
 */
static inline const instruction_definition_ultra_t* pipeline_get_decoded_instruction(
    const pipeline_overlap_manager_t *pom) {
    return pom->delayed_instruction;
}

// ===== LAGGED DATAPATH IMPLEMENTATION =====

/**
 * Lagged datapath: Register operations happen 1-2 cycles after decode
 */
void pipeline_lag_datapath(pipeline_overlap_manager_t *pom, 
                          const cycle_definition_ultra_t *cycle_def);

/**
 * Check if datapath operation is ready
 */
static inline bool pipeline_datapath_ready(const pipeline_overlap_manager_t *pom) {
    return pom->datapath_lag_cycles == 0;
}

/**
 * Get lagged ALU operation
 */
static inline uint8_t pipeline_get_lagged_alu_op(const pipeline_overlap_manager_t *pom) {
    return pom->lagged_alu_operation;
}

/**
 * Get lagged register target
 */
static inline uint8_t pipeline_get_lagged_register(const pipeline_overlap_manager_t *pom) {
    return pom->lagged_register_target;
}

// ===== OVERLAPPED WRITEBACK IMPLEMENTATION =====

/**
 * Overlapped writeback: Final updates during next instruction's fetch
 */
void pipeline_setup_overlapped_writeback(pipeline_overlap_manager_t *pom,
                                        uint8_t target_register, uint8_t value,
                                        bool affects_flags);

/**
 * Execute pending overlapped writeback
 */
void pipeline_execute_overlapped_writeback(pipeline_overlap_manager_t *pom,
                                          void (*writeback_callback)(uint8_t reg, uint8_t val, bool flags));

/**
 * Check if overlapped writeback is pending
 */
static inline bool pipeline_writeback_pending(const pipeline_overlap_manager_t *pom) {
    return pom->writeback_pending;
}

// ===== PIPELINE CONTROL FUNCTIONS =====

/**
 * Advance pipeline by one cycle
 * This is the core function that manages all pipeline stages
 */
void pipeline_advance_cycle(pipeline_overlap_manager_t *pom);

/**
 * Flush pipeline (for branches/interrupts)
 */
void pipeline_flush(pipeline_overlap_manager_t *pom);

/**
 * Stall pipeline (for memory wait states)
 */
void pipeline_stall(pipeline_overlap_manager_t *pom, uint8_t stall_cycles);

/**
 * Check if pipeline is synchronized (all stages complete)
 */
static inline bool pipeline_is_synchronized(const pipeline_overlap_manager_t *pom) {
    return pom->active_stage_count == 0 && !pom->writeback_pending;
}

/**
 * Get number of active pipeline stages
 */
static inline uint8_t pipeline_get_active_stages(const pipeline_overlap_manager_t *pom) {
    return pom->active_stage_count;
}

// ===== PIPELINE STATE TRACKING =====

/**
 * Pipeline timing example from spec (4-cycle LDA abs)
 */
typedef struct {
    uint8_t cycle;                  // Current cycle number
    const char* current_instr;      // Current instruction status  
    const char* next_instr;         // Next instruction status
    const char* pipeline_stage;    // Active pipeline stage
    bool sync_active;               // SYNC pin active
} pipeline_timing_example_t;

/**
 * Get current pipeline timing state for debugging
 */
pipeline_timing_example_t pipeline_get_timing_state(const pipeline_overlap_manager_t *pom,
                                                    uint8_t current_cycle);

/**
 * Check if multiple instructions are active simultaneously
 */
static inline bool pipeline_has_multiple_instructions(const pipeline_overlap_manager_t *pom) {
    uint8_t active_instructions = 0;
    for (int i = 0; i < 7; i++) {
        if (pom->stages[i].stage_active) active_instructions++;
    }
    return active_instructions > 1;
}

// ===== COMPLEX STATE TRANSITIONS =====

/**
 * Complex state diagram transitions from spec lines 187-197
 */
typedef enum {
    STATE_TRANS_T01_T0      = 0,    // T01,T0 → T0
    STATE_TRANS_T01_T1F_T1  = 1,    // T01,T1F,T1 → T01,T1F  
    STATE_TRANS_T01_T1F     = 2,    // T01,T1F ← T1F,T1
    STATE_TRANS_T2          = 3,    // → T2 (or T01,T0 if 2-cycle)
    STATE_TRANS_SIMULTANEOUS = 4    // Multiple states active
} complex_state_transition_t;

/**
 * Execute complex state transition with simultaneous state support
 */
void pipeline_execute_complex_transition(pipeline_overlap_manager_t *pom,
                                        complex_state_transition_t transition,
                                        const cycle_definition_ultra_t *current_cycle);

/**
 * Check if simultaneous timing states are active
 */
bool pipeline_has_simultaneous_states(const pipeline_overlap_manager_t *pom);

/**
 * Get active simultaneous timing states mask
 */
uint16_t pipeline_get_simultaneous_states_mask(const pipeline_overlap_manager_t *pom);

// ===== DEBUGGING AND VISUALIZATION =====

/**
 * Get pipeline stage name
 */
const char* pipeline_stage_name(pipeline_stage_t stage);

/**
 * Dump complete pipeline state for debugging
 */
void pipeline_dump_state(const pipeline_overlap_manager_t *pom, char *buffer, size_t buffer_size);

/**
 * Create pipeline visualization for debugging
 */
void pipeline_create_visualization(const pipeline_overlap_manager_t *pom, 
                                  char *buffer, size_t buffer_size);

/**
 * Validate pipeline state consistency
 */
bool pipeline_validate_state(const pipeline_overlap_manager_t *pom);

#endif // MOS6510_CYCLE_PIPELINE_H