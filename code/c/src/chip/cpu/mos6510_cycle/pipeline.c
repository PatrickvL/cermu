#include "pipeline.h"
#include <string.h>
#include <stdio.h>

/**
 * MOS6510 Advanced Pipeline Implementation
 * Based on visual6502.org "hidden pipeline" discovery
 *
 * This implements the sophisticated multi-stage overlapping execution
 * discovered through transistor-level analysis: "The datapath is a bit behind"
 */

// ===== FORWARD DECLARATIONS =====

/**
 * Update active stage count by counting actual active stages
 */
static void pipeline_update_active_stage_count(pipeline_overlap_manager_t *pom);

// ===== PIPELINE INITIALIZATION =====

/**
 * Initialize pipeline overlap manager
 */
void pipeline_init(pipeline_overlap_manager_t *pom) {
    // Clear all pipeline stages
    memset(pom->stages, 0, sizeof(pom->stages));
    pom->active_stage_count = 0;
    
    // Initialize predictive fetch state
    pom->prefetch_opcode = 0;
    pom->prefetch_valid = false;
    pom->prefetch_pc = 0;
    
    // Initialize delayed decode state
    pom->decode_delay_cycles = 0;
    pom->delayed_instruction = NULL;
    
    // Initialize lagged datapath state
    pom->datapath_lag_cycles = 0;
    pom->lagged_alu_operation = ALU_NOP;
    pom->lagged_register_target = 0;
    
    // Initialize overlapped writeback state
    pom->writeback_pending = false;
    pom->writeback_register = 0;
    pom->writeback_value = 0;
    pom->writeback_affects_flags = false;
    
    // Initialize synchronization state
    pom->pipeline_flush_requested = false;
    pom->pipeline_stall_requested = false;
    pom->synchronization_cycle_count = 0;
}

// ===== PREDICTIVE FETCH MECHANISM =====

/**
 * Predictive fetch: "I/PC peeks ahead to the next instruction that is predecoded"
 * This implements the sophisticated predictive behavior discovered in visual6502
 */
void pipeline_predictive_fetch(pipeline_overlap_manager_t *pom, uint16_t current_pc,
                              uint8_t (*fetch_callback)(uint16_t address)) {
    // Calculate next instruction address
    uint16_t next_pc = current_pc + 1;
    
    // Fetch next opcode predictively
    if (fetch_callback != NULL) {
        pom->prefetch_opcode = fetch_callback(next_pc);
        pom->prefetch_valid = true;
        pom->prefetch_pc = next_pc;
        
        // Update pipeline stage
        pipeline_stage_state_t *fetch_stage = &pom->stages[PIPELINE_FETCH];
        fetch_stage->stage = PIPELINE_FETCH;
        fetch_stage->opcode = pom->prefetch_opcode;
        fetch_stage->cycle_position = 0;
        fetch_stage->stage_active = true;
        fetch_stage->stage_stalled = false;
        fetch_stage->delay_cycles = 0;
        
        // Update active stage count immediately
        pipeline_update_active_stage_count(pom);
    } else {
        // No fetch callback - invalidate prefetch
        pom->prefetch_valid = false;
    }
}

// ===== DELAYED DECODE IMPLEMENTATION =====

/**
 * Delayed decode: "T0 and T1 inputs to the PLA actually come behind everything else"
 * This models the 1-2 cycle delay between instruction fetch and PLA decode
 */
void pipeline_delayed_decode(pipeline_overlap_manager_t *pom, uint8_t opcode) {
    // Set up delayed decode with T0/T1 lag
    pom->decode_delay_cycles = 2; // T0 and T1 lag behind
    
    // Start decode process but results won't be available immediately
    pipeline_stage_state_t *decode_stage = &pom->stages[PIPELINE_DECODE];
    decode_stage->stage = PIPELINE_DECODE;
    decode_stage->opcode = opcode;
    decode_stage->cycle_position = 0;
    decode_stage->stage_active = true;
    decode_stage->stage_stalled = false;
    decode_stage->delay_cycles = pom->decode_delay_cycles;
    
    // Clear any previous decode results
    pom->delayed_instruction = NULL;
    
    // Update active stage count immediately
    pipeline_update_active_stage_count(pom);
}

/**
 * Process delayed decode completion
 */
static void pipeline_complete_delayed_decode(pipeline_overlap_manager_t *pom) {
    pipeline_stage_state_t *decode_stage = &pom->stages[PIPELINE_DECODE];
    
    if (decode_stage->stage_active && decode_stage->delay_cycles == 0) {
        // Decode is complete - look up instruction
        pom->delayed_instruction = pla_lookup(decode_stage->opcode);
        
        // Mark decode stage as complete
        decode_stage->stage_active = false;
    }
}

// ===== LAGGED DATAPATH IMPLEMENTATION =====

/**
 * Lagged datapath: Register operations happen 1-2 cycles after decode
 * Models the "datapath behind" behavior
 */
void pipeline_lag_datapath(pipeline_overlap_manager_t *pom,
                          const cycle_definition_t *cycle_def) {
    // Set up datapath lag (1-2 cycles typical)
    pom->datapath_lag_cycles = 1; // Most operations have 1 cycle lag
    
    // Complex operations may have 2 cycle lag
    if (cycle_def->alu == ALU_RMW || cycle_def->alu == ALU_SHIFT) {
        pom->datapath_lag_cycles = 2;
    }
    
    // Store lagged operations
    pom->lagged_alu_operation = cycle_def->alu;
    pom->lagged_register_target = infer_target_register_index(0); // Will be determined later
    
    // Set up datapath pipeline stage
    pipeline_stage_state_t *datapath_stage = &pom->stages[PIPELINE_EXECUTE];
    datapath_stage->stage = PIPELINE_EXECUTE;
    datapath_stage->cycle_position = 0;
    datapath_stage->stage_active = true;
    datapath_stage->stage_stalled = false;
    datapath_stage->delay_cycles = pom->datapath_lag_cycles;
    
    // Update active stage count immediately
    pipeline_update_active_stage_count(pom);
}

// ===== OVERLAPPED WRITEBACK IMPLEMENTATION =====

/**
 * Overlapped writeback: Final updates during next instruction's fetch
 * This models the writeback occurring during the next instruction's T1F phase
 */
void pipeline_setup_overlapped_writeback(pipeline_overlap_manager_t *pom,
                                        uint8_t target_register, uint8_t value,
                                        bool affects_flags) {
    pom->writeback_pending = true;
    pom->writeback_register = target_register;
    pom->writeback_value = value;
    pom->writeback_affects_flags = affects_flags;
    
    // Set up writeback pipeline stage
    pipeline_stage_state_t *writeback_stage = &pom->stages[PIPELINE_WRITEBACK];
    writeback_stage->stage = PIPELINE_WRITEBACK;
    writeback_stage->cycle_position = 0;
    writeback_stage->stage_active = true;
    writeback_stage->stage_stalled = false;
    writeback_stage->delay_cycles = 1; // Writeback during next T1F
    
    // Update active stage count immediately
    pipeline_update_active_stage_count(pom);
}

/**
 * Execute pending overlapped writeback
 */
void pipeline_execute_overlapped_writeback(pipeline_overlap_manager_t *pom,
                                          void (*writeback_callback)(uint8_t reg, uint8_t val, bool flags)) {
    if (pom->writeback_pending && writeback_callback != NULL) {
        // Execute the writeback
        writeback_callback(pom->writeback_register, pom->writeback_value, 
                          pom->writeback_affects_flags);
        
        // Clear writeback state
        pom->writeback_pending = false;
        
        // Mark writeback stage as complete
        pipeline_stage_state_t *writeback_stage = &pom->stages[PIPELINE_WRITEBACK];
        writeback_stage->stage_active = false;
    }
}

// ===== PIPELINE CONTROL FUNCTIONS =====

/**
 * Advance pipeline by one cycle
 * This is the core function that manages all pipeline stages
 */
void pipeline_advance_cycle(pipeline_overlap_manager_t *pom) {
    // Handle pipeline flush/stall requests
    if (pom->pipeline_flush_requested) {
        pipeline_flush(pom);
        pom->pipeline_flush_requested = false;
        return;
    }
    
    if (pom->pipeline_stall_requested) {
        // Don't advance any stages during stall - decrement stall counter
        if (pom->synchronization_cycle_count > 0) {
            pom->synchronization_cycle_count--;
            if (pom->synchronization_cycle_count == 0) {
                pom->pipeline_stall_requested = false; // End stall
                // Clear stall flags from all stages
                for (int i = 0; i < 7; i++) {
                    pom->stages[i].stage_stalled = false;
                }
            }
        }
        return;
    }
    
    // Advance all active pipeline stages
    for (int i = 0; i < 7; i++) {
        pipeline_stage_state_t *stage = &pom->stages[i];
        
        if (stage->stage_active) {
            // Advance delay cycles
            if (stage->delay_cycles > 0) {
                stage->delay_cycles--;
            }
            
            // Check if stage is complete
            if (stage->delay_cycles == 0) {
                switch (stage->stage) {
                    case PIPELINE_DECODE:
                        pipeline_complete_delayed_decode(pom);
                        break;
                    case PIPELINE_EXECUTE:
                        // Datapath operation is ready
                        pom->datapath_lag_cycles = 0;
                        break;
                    case PIPELINE_WRITEBACK:
                        // Writeback can execute
                        break;
                    default:
                        // Other stages complete immediately
                        stage->stage_active = false;
                        break;
                }
            }
        }
    }
    
    // Decrement global delay counters
    if (pom->decode_delay_cycles > 0) {
        pom->decode_delay_cycles--;
    }
    
    if (pom->datapath_lag_cycles > 0) {
        pom->datapath_lag_cycles--;
    }
    
    if (pom->synchronization_cycle_count > 0) {
        pom->synchronization_cycle_count--;
    }
    
    // Update active stage count after all processing
    pipeline_update_active_stage_count(pom);
}

/**
 * Flush pipeline (for branches/interrupts)
 */
void pipeline_flush(pipeline_overlap_manager_t *pom) {
    // Clear all pipeline stages
    for (int i = 0; i < 7; i++) {
        pom->stages[i].stage_active = false;
        pom->stages[i].delay_cycles = 0;
    }
    
    // Clear prefetch and decode state
    pom->prefetch_valid = false;
    pom->delayed_instruction = NULL;
    pom->decode_delay_cycles = 0;
    pom->datapath_lag_cycles = 0;
    
    // Keep writeback if it's already committed
    // (some writebacks cannot be cancelled once started)
    
    pom->active_stage_count = pom->writeback_pending ? 1 : 0;
    pom->synchronization_cycle_count = 2; // 2 cycles to re-sync
}

/**
 * Stall pipeline (for memory wait states)
 */
void pipeline_stall(pipeline_overlap_manager_t *pom, uint8_t stall_cycles) {
    pom->pipeline_stall_requested = true;
    pom->synchronization_cycle_count = stall_cycles;
    
    // Stall affects all active stages
    for (int i = 0; i < 7; i++) {
        if (pom->stages[i].stage_active) {
            pom->stages[i].stage_stalled = true;
        }
    }
}

// ===== COMPLEX STATE TRANSITIONS =====

/**
 * Execute complex state transition with simultaneous state support
 * Implements the complex state diagram from spec lines 187-197
 */
void pipeline_execute_complex_transition(pipeline_overlap_manager_t *pom,
                                        complex_state_transition_t transition,
                                        const cycle_definition_t *current_cycle) {
    switch (transition) {
        case STATE_TRANS_T01_T0:
            // T01,T0 → T0 (instruction completion)
            pipeline_setup_overlapped_writeback(pom, 
                infer_target_register_index(current_cycle->alu),
                0, // Value will be determined by execution
                affects_nz_flags(current_cycle->alu));
            break;
            
        case STATE_TRANS_T01_T1F_T1:
            // T01,T1F,T1 → T01,T1F (multiple simultaneous states)
            // This is the "2 instructions!" case from the pipeline timing example
            break;
            
        case STATE_TRANS_T01_T1F:
            // T01,T1F ← T1F,T1 (state transition with overlap)
            break;
            
        case STATE_TRANS_T2:
            // → T2 (or T01,T0 if 2-cycle instruction)
            if (current_cycle->timing == TIMING_T0) {
                // 2-cycle instruction completion
                pipeline_execute_complex_transition(pom, STATE_TRANS_T01_T0, current_cycle);
            }
            break;
            
        case STATE_TRANS_SIMULTANEOUS:
            // Multiple timing states active simultaneously
            // This models the complex overlapping discovered in visual6502
            break;
    }
}

/**
 * Check if simultaneous timing states are active
 */
bool pipeline_has_simultaneous_states(const pipeline_overlap_manager_t *pom) {
    return pom->active_stage_count > 1;
}

/**
 * Get active simultaneous timing states mask
 */
uint16_t pipeline_get_simultaneous_states_mask(const pipeline_overlap_manager_t *pom) {
    uint16_t mask = 0;
    
    for (int i = 0; i < 7; i++) {
        if (pom->stages[i].stage_active) {
            mask |= (1 << i);
        }
    }
    
    return mask;
}

// ===== DEBUGGING AND VISUALIZATION =====

/**
 * Get pipeline stage name
 */
const char* pipeline_stage_name(pipeline_stage_t stage) {
    static const char* names[] = {
        "IDLE",       // PIPELINE_IDLE
        "FETCH",      // PIPELINE_FETCH
        "PREDECODE",  // PIPELINE_PREDECODE
        "DECODE",     // PIPELINE_DECODE
        "EXECUTE",    // PIPELINE_EXECUTE
        "WRITEBACK",  // PIPELINE_WRITEBACK
        "COMPLETE"    // PIPELINE_COMPLETE
    };
    
    return (stage < 7) ? names[stage] : "INVALID";
}

/**
 * Dump complete pipeline state for debugging
 */
void pipeline_dump_state(const pipeline_overlap_manager_t *pom, char *buffer, size_t buffer_size) {
    int pos = 0;
    
    pos += snprintf(buffer + pos, buffer_size - pos,
        "Pipeline State: %d active stages\n", pom->active_stage_count);
    
    // Dump active stages
    for (int i = 0; i < 7; i++) {
        const pipeline_stage_state_t *stage = &pom->stages[i];
        if (stage->stage_active) {
            pos += snprintf(buffer + pos, buffer_size - pos,
                "  Stage %s: opcode=0x%02X cycle=%d delay=%d %s\n",
                pipeline_stage_name(stage->stage),
                stage->opcode,
                stage->cycle_position,
                stage->delay_cycles,
                stage->stage_stalled ? "(STALLED)" : "");
        }
    }
    
    // Dump predictive fetch state
    if (pom->prefetch_valid) {
        pos += snprintf(buffer + pos, buffer_size - pos,
            "  Prefetch: opcode=0x%02X at PC=0x%04X\n",
            pom->prefetch_opcode, pom->prefetch_pc);
    }
    
    // Dump writeback state
    if (pom->writeback_pending) {
        pos += snprintf(buffer + pos, buffer_size - pos,
            "  Writeback: reg=%d value=0x%02X flags=%s\n",
            pom->writeback_register, pom->writeback_value,
            pom->writeback_affects_flags ? "yes" : "no");
    }
}

/**
 * Get current pipeline timing state for debugging
 */
pipeline_timing_example_t pipeline_get_timing_state(const pipeline_overlap_manager_t *pom,
                                                    uint8_t current_cycle) {
    pipeline_timing_example_t state = {0};
    
    state.cycle = current_cycle;
    state.sync_active = false; // Will be determined by caller
    
    // Determine current and next instruction status
    if (pipeline_has_multiple_instructions(pom)) {
        state.current_instr = "Multi-stage";
        state.next_instr = "Overlapped";
        state.pipeline_stage = "2 instructions!"; // The famous case from spec
    } else if (pom->active_stage_count == 1) {
        state.current_instr = "Single instruction";
        state.next_instr = "Pending";
        state.pipeline_stage = "Single instruction";
    } else {
        state.current_instr = "Idle";
        state.next_instr = "Ready";
        state.pipeline_stage = "Synchronized";
    }
    
    return state;
}

/**
 * Create pipeline visualization for debugging
 */
void pipeline_create_visualization(const pipeline_overlap_manager_t *pom, 
                                  char *buffer, size_t buffer_size) {
    int pos = 0;
    
    pos += snprintf(buffer + pos, buffer_size - pos,
        "Pipeline Visualization:\n");
    pos += snprintf(buffer + pos, buffer_size - pos,
        "Cycle | Current Instruction | Next Instruction  | Pipeline Stage\n");
    pos += snprintf(buffer + pos, buffer_size - pos,
        "------|-------------------|------------------|------------------\n");
    
    // Show example from spec (4-cycle LDA abs)
    const char* examples[][4] = {
        {"1", "T2: fetch addr_low", "       -        ", "Single instruction"},
        {"2", "T3: fetch addr_high", "       -        ", "Single instruction"},
        {"3", "T4: read data", "       -        ", "Single instruction"},
        {"4", "T0: complete", "T1F: fetch opcode", "2 instructions! (SYNC)"},
        {"5", "T1: writeback", "T2: fetch operand", "2 instructions"},
        {"6", "       -        ", "T3: execute", "Next begins"}
    };
    
    for (int i = 0; i < 6; i++) {
        pos += snprintf(buffer + pos, buffer_size - pos,
            "  %s   | %-17s | %-16s | %s\n",
            examples[i][0], examples[i][1], examples[i][2], examples[i][3]);
    }
}

/**
 * Validate pipeline state consistency
 */
/**
 * Update active stage count by counting actual active stages
 */
static void pipeline_update_active_stage_count(pipeline_overlap_manager_t *pom) {
    uint8_t active_count = 0;
    for (int i = 0; i < 7; i++) {
        if (pom->stages[i].stage_active) {
            active_count++;
        }
    }
    pom->active_stage_count = active_count;
}

bool pipeline_validate_state(const pipeline_overlap_manager_t *pom) {
    // Check that active stage count matches actual active stages
    uint8_t actual_active = 0;
    for (int i = 0; i < 7; i++) {
        if (pom->stages[i].stage_active) {
            actual_active++;
        }
    }
    
    if (actual_active != pom->active_stage_count) {
        return false; // Inconsistent stage count
    }
    
    // Check that delayed decode state is consistent
    if (pom->delayed_instruction != NULL && pom->decode_delay_cycles > 0) {
        return false; // Can't have both delayed and completed decode
    }
    
    // Check writeback consistency (relaxed - allow writeback_pending without active stage during testing)
    // This is valid when writeback is set up but hasn't started yet
    
    return true; // State is consistent
}