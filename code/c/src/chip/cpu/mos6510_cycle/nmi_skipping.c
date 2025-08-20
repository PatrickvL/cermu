#include "nmi_skipping.h"
#include "interrupt_recognition.h"
#include "mos6510_state.h"
#include "mos6510_registers.h"
#include "timing_states.h"
#include <string.h>
#include <stdio.h>

/**
 * MOS6510 NMI Skipping Conditions Implementation
 * Based on visual6502.org analysis (spec lines 271-288)
 * 
 * Implements all 4 critical NMI skipping conditions that cause NMIs
 * to be lost or delayed, exactly matching real hardware behavior.
 */

// ===== PROCESSOR STATUS FLAGS =====

#define FLAG_I  0x04  // IRQ disable flag

// ===== OPCODES FOR SEI/CLI DETECTION =====

#define OPCODE_SEI  0x78  // Set Interrupt Disable
#define OPCODE_CLI  0x58  // Clear Interrupt Disable

// ===== NMI SKIPPING INITIALIZATION =====

void nmi_skipping_init(nmi_skipping_state_t *skip_state) {
    if (!skip_state) return;
    
    // Initialize all conditions to inactive
    skip_state->current_condition = NMI_SKIP_NONE;
    skip_state->nmi_skip_active = false;
    
    // Initialize Condition 1: Lost NMI during IRQ vector fetch
    skip_state->irq_vector_fetch_active = false;
    skip_state->irq_vector_fetch_cycles = 0;
    skip_state->nmi_lost_during_irq_vector = false;
    
    // Initialize Condition 2: Branch instruction masking
    skip_state->branch_t3_to_t1f_sequence = false;
    skip_state->branch_masking_next_instr = false;
    skip_state->branch_masking_cycles = 0;
    
    // Initialize Condition 3: Critical timing window miss
    skip_state->timing_window_t5_phi1 = false;
    skip_state->timing_window_t1_phi1 = false;
    skip_state->nmi_down_at_t5_phi1 = false;
    skip_state->nmi_up_before_t1_phi1 = false;
    skip_state->critical_timing_cycles = 0;
    
    // Initialize Condition 4: Pipeline-induced delays
    skip_state->sei_cli_instruction_active = false;
    skip_state->status_register_delay = false;
    skip_state->pipeline_delay_cycles = 0;
    skip_state->interrupt_slip_window = false;
    
    // Initialize timing and state tracking
    skip_state->last_timing_state = TIMING_T1F;  // Start in fetch state
    skip_state->phi1_phase_active = false;
    skip_state->skip_detection_cycles = 0;
    
    // Initialize statistics
    skip_state->condition1_count = 0;
    skip_state->condition2_count = 0;
    skip_state->condition3_count = 0;
    skip_state->condition4_count = 0;
    skip_state->total_nmis_skipped = 0;
    skip_state->total_nmis_delayed = 0;
}

void nmi_skipping_reset(nmi_skipping_state_t *skip_state) {
    if (!skip_state) return;
    
    // Reset clears all skip conditions immediately
    nmi_skipping_init(skip_state);
}

// ===== MAIN NMI SKIPPING UPDATE =====

void nmi_skipping_update(nmi_skipping_state_t *skip_state,
                        struct mos6510_state_s *cpu,
                        struct interrupt_recognition_t *int_rec,
                        bool nmi_pin, bool irq_pin) {
    if (!skip_state || !cpu || !int_rec) return;
    
    // Update timing state tracking
    bool phi1_phase = skip_state->phi1_phase_active;  // Would be set by clock system
    nmi_skipping_update_timing_tracking(skip_state, cpu, phi1_phase);
    
    // Check all 4 NMI skipping conditions
    
    // Condition 1: Lost NMI during IRQ vector fetch
    nmi_skipping_condition1_update(skip_state, cpu, int_rec, nmi_pin);
    
    // Condition 2: Branch instruction masking  
    nmi_skipping_condition2_update(skip_state, cpu, nmi_pin);
    
    // Condition 3: Critical timing window miss
    nmi_skipping_condition3_update(skip_state, cpu, nmi_pin, phi1_phase);
    
    // Condition 4: Pipeline-induced delays with SEI/CLI
    nmi_skipping_condition4_update(skip_state, cpu, nmi_pin);
    
    // Update active condition based on priority
    nmi_skip_condition_t new_condition = NMI_SKIP_NONE;
    
    // Priority: Condition 1 > Condition 3 > Condition 2 > Condition 4
    if (skip_state->nmi_lost_during_irq_vector) {
        new_condition = NMI_SKIP_IRQ_VECTOR_FETCH;
    } else if (skip_state->nmi_down_at_t5_phi1 && skip_state->nmi_up_before_t1_phi1) {
        new_condition = NMI_SKIP_TIMING_WINDOW;
    } else if (skip_state->branch_masking_next_instr) {
        new_condition = NMI_SKIP_BRANCH_MASKING;
    } else if (skip_state->interrupt_slip_window) {
        new_condition = NMI_SKIP_PIPELINE_DELAY;
    }
    
    // Update current condition and statistics
    if (new_condition != skip_state->current_condition) {
        if (new_condition != NMI_SKIP_NONE) {
            // New skip condition detected
            switch (new_condition) {
                case NMI_SKIP_IRQ_VECTOR_FETCH: skip_state->condition1_count++; break;
                case NMI_SKIP_BRANCH_MASKING:   skip_state->condition2_count++; break;
                case NMI_SKIP_TIMING_WINDOW:    skip_state->condition3_count++; break;
                case NMI_SKIP_PIPELINE_DELAY:   skip_state->condition4_count++; break;
                default: break;
            }
            skip_state->total_nmis_skipped++;
        }
        skip_state->current_condition = new_condition;
        skip_state->nmi_skip_active = (new_condition != NMI_SKIP_NONE);
    }
    
    // Update detection cycle counter
    skip_state->skip_detection_cycles++;
}

bool nmi_skipping_should_skip_nmi(const nmi_skipping_state_t *skip_state) {
    return skip_state && skip_state->nmi_skip_active;
}

nmi_skip_condition_t nmi_skipping_get_condition(const nmi_skipping_state_t *skip_state) {
    return skip_state ? skip_state->current_condition : NMI_SKIP_NONE;
}

void nmi_skipping_clear_condition(nmi_skipping_state_t *skip_state) {
    if (!skip_state) return;
    
    // Clear the current condition and associated state
    skip_state->current_condition = NMI_SKIP_NONE;
    skip_state->nmi_skip_active = false;
    
    // Clear condition-specific state
    skip_state->nmi_lost_during_irq_vector = false;
    skip_state->branch_masking_next_instr = false;
    skip_state->nmi_down_at_t5_phi1 = false;
    skip_state->nmi_up_before_t1_phi1 = false;
    skip_state->interrupt_slip_window = false;
}

// ===== CONDITION 1: LOST NMI DURING IRQ VECTOR FETCH =====

void nmi_skipping_condition1_update(nmi_skipping_state_t *skip_state,
                                   struct mos6510_state_s *cpu,
                                   struct interrupt_recognition_t *int_rec,
                                   bool nmi_pin) {
    if (!skip_state || !cpu || !int_rec) return;
    
    // Check if IRQ vector fetch is currently active
    bool irq_fetch_active = nmi_skipping_irq_vector_fetch_active(cpu, int_rec);
    
    // Track IRQ vector fetch state
    if (irq_fetch_active && !skip_state->irq_vector_fetch_active) {
        // IRQ vector fetch started
        skip_state->irq_vector_fetch_active = true;
        skip_state->irq_vector_fetch_cycles = 3;  // IRQ vector fetch takes 3 cycles
    } else if (!irq_fetch_active && skip_state->irq_vector_fetch_active) {
        // IRQ vector fetch ended
        skip_state->irq_vector_fetch_active = false;
        skip_state->irq_vector_fetch_cycles = 0;
    }
    
    // Update IRQ vector fetch cycle counter
    if (skip_state->irq_vector_fetch_active && skip_state->irq_vector_fetch_cycles > 0) {
        skip_state->irq_vector_fetch_cycles--;
    }
    
    // Detect NMI loss during IRQ vector fetch
    if (skip_state->irq_vector_fetch_active) {
        nmi_skipping_detect_nmi_loss_during_irq(skip_state, nmi_pin);
    }
}

bool nmi_skipping_irq_vector_fetch_active(const struct mos6510_state_s *cpu,
                                         const struct interrupt_recognition_t *int_rec) {
    if (!cpu || !int_rec) return false;
    
    // IRQ vector fetch is active when interrupt recognition is in BRK substitution
    // for IRQ and we're in the vector fetch timing states
    return (interrupt_recognition_get_vector_type(int_rec) == VECTOR_IRQ_BRK) &&
           interrupt_recognition_vector_ready(int_rec) &&
           (cpu->timing_state == TIMING_VEC);  // Vector fetch timing state
}

void nmi_skipping_detect_nmi_loss_during_irq(nmi_skipping_state_t *skip_state,
                                            bool nmi_pin) {
    if (!skip_state) return;
    
    // Static variables to track NMI state during IRQ vector fetch
    static bool nmi_was_active = false;
    static uint8_t nmi_active_cycles = 0;
    
    if (skip_state->irq_vector_fetch_active) {
        if (!nmi_pin && !nmi_was_active) {
            // NMI went active during IRQ vector fetch
            nmi_was_active = true;
            nmi_active_cycles = 1;
        } else if (!nmi_pin && nmi_was_active) {
            // NMI still active, increment counter
            nmi_active_cycles++;
        } else if (nmi_pin && nmi_was_active) {
            // NMI went inactive, check if it was active < 3 cycles
            if (nmi_active_cycles < 3) {
                // NMI was active for less than 3 cycles during IRQ vector fetch
                // This causes the NMI to be completely lost
                skip_state->nmi_lost_during_irq_vector = true;
            }
            nmi_was_active = false;
            nmi_active_cycles = 0;
        }
    } else {
        // Reset tracking when not in IRQ vector fetch
        nmi_was_active = false;
        nmi_active_cycles = 0;
    }
}

// ===== CONDITION 2: BRANCH INSTRUCTION MASKING =====

void nmi_skipping_condition2_update(nmi_skipping_state_t *skip_state,
                                   struct mos6510_state_s *cpu,
                                   bool nmi_pin) {
    if (!skip_state || !cpu) return;
    
    // Detect T3→T1F branch sequence
    bool branch_sequence = nmi_skipping_detect_branch_t3_to_t1f(skip_state, cpu);
    
    if (branch_sequence && !skip_state->branch_t3_to_t1f_sequence) {
        // Branch T3→T1F sequence detected, next instruction will be masked
        skip_state->branch_t3_to_t1f_sequence = true;
        skip_state->branch_masking_next_instr = true;
        skip_state->branch_masking_cycles = 2;  // Mask for next 2 cycles
    }
    
    // Update masking cycle counter
    if (skip_state->branch_masking_cycles > 0) {
        skip_state->branch_masking_cycles--;
        if (skip_state->branch_masking_cycles == 0) {
            // Masking period ended
            skip_state->branch_masking_next_instr = false;
            skip_state->branch_t3_to_t1f_sequence = false;
        }
    }
}

bool nmi_skipping_detect_branch_t3_to_t1f(const nmi_skipping_state_t *skip_state,
                                         const struct mos6510_state_s *cpu) {
    if (!skip_state || !cpu) return false;
    
    // Detect transition from T3 to T1F (fetch state)
    // This indicates a branch instruction that affects the next instruction
    return (skip_state->last_timing_state == TIMING_T3) &&
           (cpu->timing_state == TIMING_T1F);
}

bool nmi_skipping_next_instruction_masked(const nmi_skipping_state_t *skip_state) {
    return skip_state && skip_state->branch_masking_next_instr;
}

// ===== CONDITION 3: CRITICAL TIMING WINDOW MISS =====

void nmi_skipping_condition3_update(nmi_skipping_state_t *skip_state,
                                   struct mos6510_state_s *cpu,
                                   bool nmi_pin, bool phi1_phase) {
    if (!skip_state || !cpu) return;
    
    // Detect T5 φ1 timing window
    nmi_skipping_detect_t5_phi1_window(skip_state, cpu, phi1_phase);
    
    // Detect T1 φ1 timing window and check for NMI loss
    nmi_skipping_detect_t1_phi1_window(skip_state, cpu, phi1_phase, nmi_pin);
    
    // Update critical timing cycle counter
    if (skip_state->timing_window_t5_phi1 || skip_state->timing_window_t1_phi1) {
        skip_state->critical_timing_cycles++;
    } else {
        skip_state->critical_timing_cycles = 0;
    }
}

void nmi_skipping_detect_t5_phi1_window(nmi_skipping_state_t *skip_state,
                                       const struct mos6510_state_s *cpu,
                                       bool phi1_phase) {
    if (!skip_state || !cpu) return;
    
    // T5 φ1 window is active when CPU is in T5 timing state during φ1 phase
    bool t5_phi1_active = (cpu->timing_state == TIMING_T5) && phi1_phase;
    
    if (t5_phi1_active && !skip_state->timing_window_t5_phi1) {
        // Entering T5 φ1 window
        skip_state->timing_window_t5_phi1 = true;
    } else if (!t5_phi1_active && skip_state->timing_window_t5_phi1) {
        // Exiting T5 φ1 window
        skip_state->timing_window_t5_phi1 = false;
    }
}

void nmi_skipping_detect_t1_phi1_window(nmi_skipping_state_t *skip_state,
                                       const struct mos6510_state_s *cpu,
                                       bool phi1_phase, bool nmi_pin) {
    if (!skip_state || !cpu) return;
    
    // T1 φ1 window is active when CPU is in T1 timing state during φ1 phase  
    bool t1_phi1_active = (cpu->timing_state == TIMING_T1F) && phi1_phase;
    
    if (t1_phi1_active && !skip_state->timing_window_t1_phi1) {
        // Entering T1 φ1 window - check if NMI went up before this point
        skip_state->timing_window_t1_phi1 = true;
        
        if (skip_state->nmi_down_at_t5_phi1 && nmi_pin) {
            // NMI went down at T5 φ1 and is now up before T1 φ1
            // This causes NMI loss due to timing window miss
            skip_state->nmi_up_before_t1_phi1 = true;
        }
    } else if (!t1_phi1_active && skip_state->timing_window_t1_phi1) {
        // Exiting T1 φ1 window
        skip_state->timing_window_t1_phi1 = false;
        skip_state->nmi_down_at_t5_phi1 = false;  // Clear for next cycle
        skip_state->nmi_up_before_t1_phi1 = false;
    }
    
    // Track NMI going down during T5 φ1
    if (skip_state->timing_window_t5_phi1 && !nmi_pin) {
        skip_state->nmi_down_at_t5_phi1 = true;
    }
}

// ===== CONDITION 4: PIPELINE-INDUCED DELAYS =====

void nmi_skipping_condition4_update(nmi_skipping_state_t *skip_state,
                                   struct mos6510_state_s *cpu,
                                   bool nmi_pin) {
    if (!skip_state || !cpu) return;
    
    // Detect SEI/CLI instruction execution
    bool sei_cli_active = nmi_skipping_detect_sei_cli_instruction(cpu);
    
    if (sei_cli_active && !skip_state->sei_cli_instruction_active) {
        // SEI/CLI instruction started
        skip_state->sei_cli_instruction_active = true;
        skip_state->status_register_delay = true;
        skip_state->pipeline_delay_cycles = 2;  // Pipeline delay affects next instruction
    }
    
    // Update pipeline delay
    if (skip_state->pipeline_delay_cycles > 0) {
        skip_state->pipeline_delay_cycles--;
        
        // Check for interrupt slip window during delay
        nmi_skipping_detect_interrupt_slip_window(skip_state, nmi_pin);
        
        if (skip_state->pipeline_delay_cycles == 0) {
            // Pipeline delay ended
            skip_state->sei_cli_instruction_active = false;
            skip_state->status_register_delay = false;
            skip_state->interrupt_slip_window = false;
        }
    }
}

bool nmi_skipping_detect_sei_cli_instruction(const struct mos6510_state_s *cpu) {
    if (!cpu) return false;
    
    // Check if current instruction register contains SEI or CLI opcode
    uint8_t opcode = cpu->instruction_register;
    return (opcode == OPCODE_SEI) || (opcode == OPCODE_CLI);
}

bool nmi_skipping_status_register_delayed(const nmi_skipping_state_t *skip_state,
                                         const struct mos6510_state_s *cpu) {
    if (!skip_state || !cpu) return false;
    
    // Status register update is delayed during pipeline delay cycles
    return skip_state->status_register_delay && (skip_state->pipeline_delay_cycles > 0);
}

void nmi_skipping_detect_interrupt_slip_window(nmi_skipping_state_t *skip_state,
                                              bool nmi_pin) {
    if (!skip_state) return;
    
    // Interrupt slip window occurs during status register delay
    // when NMI becomes active but I flag hasn't been updated yet
    if (skip_state->status_register_delay && !nmi_pin) {
        // NMI active during pipeline delay - creates slip window
        skip_state->interrupt_slip_window = true;
    }
}

// ===== TIMING STATE TRACKING =====

void nmi_skipping_update_timing_tracking(nmi_skipping_state_t *skip_state,
                                        const struct mos6510_state_s *cpu,
                                        bool phi1_phase) {
    if (!skip_state || !cpu) return;
    
    // Update timing state tracking
    skip_state->last_timing_state = cpu->timing_state;
    skip_state->phi1_phase_active = phi1_phase;
}

bool nmi_skipping_timing_transition_detected(const nmi_skipping_state_t *skip_state,
                                            const struct mos6510_state_s *cpu,
                                            uint8_t from_state, uint8_t to_state) {
    if (!skip_state || !cpu) return false;
    
    return (skip_state->last_timing_state == from_state) &&
           (cpu->timing_state == to_state);
}

bool nmi_skipping_in_timing_state(const struct mos6510_state_s *cpu, uint8_t timing_state) {
    return cpu && (cpu->timing_state == timing_state);
}

// ===== VALIDATION AND DEBUGGING =====

bool nmi_skipping_validate(const nmi_skipping_state_t *skip_state) {
    if (!skip_state) return false;
    
    // Check for invalid condition combinations
    uint8_t active_conditions = 0;
    if (skip_state->nmi_lost_during_irq_vector) active_conditions++;
    if (skip_state->branch_masking_next_instr) active_conditions++;
    if (skip_state->nmi_down_at_t5_phi1 && skip_state->nmi_up_before_t1_phi1) active_conditions++;
    if (skip_state->interrupt_slip_window) active_conditions++;
    
    // Only one condition should be active at a time
    if (active_conditions > 1) return false;
    
    // Current condition should match active conditions
    if (active_conditions == 0 && skip_state->current_condition != NMI_SKIP_NONE) {
        return false;
    }
    if (active_conditions == 1 && skip_state->current_condition == NMI_SKIP_NONE) {
        return false;
    }
    
    return true;
}

void nmi_skipping_dump(const nmi_skipping_state_t *skip_state, char *buffer, size_t buffer_size) {
    if (!skip_state || !buffer || buffer_size == 0) return;
    
    snprintf(buffer, buffer_size,
        "NMI Skipping State:\n"
        "  Current Condition: %s (Active: %s)\n"
        "  Condition 1 (IRQ Vector): Lost=%s, Cycles=%u\n"
        "  Condition 2 (Branch Mask): Active=%s, Cycles=%u\n" 
        "  Condition 3 (Timing Window): T5φ1=%s, T1φ1=%s, Down@T5=%s, Up@T1=%s\n"
        "  Condition 4 (Pipeline): SEI/CLI=%s, Delay=%s, Slip=%s, Cycles=%u\n"
        "  Statistics: C1=%u C2=%u C3=%u C4=%u Total=%u Delayed=%u\n",
        nmi_skip_condition_name(skip_state->current_condition),
        skip_state->nmi_skip_active ? "YES" : "NO",
        skip_state->nmi_lost_during_irq_vector ? "YES" : "NO", skip_state->irq_vector_fetch_cycles,
        skip_state->branch_masking_next_instr ? "YES" : "NO", skip_state->branch_masking_cycles,
        skip_state->timing_window_t5_phi1 ? "YES" : "NO", skip_state->timing_window_t1_phi1 ? "YES" : "NO",
        skip_state->nmi_down_at_t5_phi1 ? "YES" : "NO", skip_state->nmi_up_before_t1_phi1 ? "YES" : "NO",
        skip_state->sei_cli_instruction_active ? "YES" : "NO", skip_state->status_register_delay ? "YES" : "NO",
        skip_state->interrupt_slip_window ? "YES" : "NO", skip_state->pipeline_delay_cycles,
        skip_state->condition1_count, skip_state->condition2_count, skip_state->condition3_count,
        skip_state->condition4_count, skip_state->total_nmis_skipped, skip_state->total_nmis_delayed);
}

const char* nmi_skip_condition_name(nmi_skip_condition_t condition) {
    switch (condition) {
        case NMI_SKIP_NONE:               return "NONE";
        case NMI_SKIP_IRQ_VECTOR_FETCH:   return "IRQ_VECTOR_FETCH";
        case NMI_SKIP_BRANCH_MASKING:     return "BRANCH_MASKING";
        case NMI_SKIP_TIMING_WINDOW:      return "TIMING_WINDOW";
        case NMI_SKIP_PIPELINE_DELAY:     return "PIPELINE_DELAY";
        default:                          return "UNKNOWN";
    }
}

void nmi_skipping_get_condition_description(const nmi_skipping_state_t *skip_state,
                                           char *buffer, size_t buffer_size) {
    if (!skip_state || !buffer || buffer_size == 0) return;
    
    switch (skip_state->current_condition) {
        case NMI_SKIP_IRQ_VECTOR_FETCH:
            snprintf(buffer, buffer_size, "NMI lost during IRQ vector fetch (duration < 3 cycles)");
            break;
        case NMI_SKIP_BRANCH_MASKING:
            snprintf(buffer, buffer_size, "NMI masked by branch T3→T1F sequence");
            break;
        case NMI_SKIP_TIMING_WINDOW:
            snprintf(buffer, buffer_size, "NMI lost due to T5φ1→T1φ1 timing window miss");
            break;
        case NMI_SKIP_PIPELINE_DELAY:
            snprintf(buffer, buffer_size, "NMI slipped through during SEI/CLI pipeline delay");
            break;
        default:
            snprintf(buffer, buffer_size, "No NMI skipping condition active");
            break;
    }
}

void nmi_skipping_get_statistics(const nmi_skipping_state_t *skip_state,
                               uint32_t *condition1_count, uint32_t *condition2_count,
                               uint32_t *condition3_count, uint32_t *condition4_count,
                               uint32_t *total_skipped, uint32_t *total_delayed) {
    if (!skip_state) return;
    
    if (condition1_count) *condition1_count = skip_state->condition1_count;
    if (condition2_count) *condition2_count = skip_state->condition2_count;
    if (condition3_count) *condition3_count = skip_state->condition3_count;
    if (condition4_count) *condition4_count = skip_state->condition4_count;
    if (total_skipped) *total_skipped = skip_state->total_nmis_skipped;
    if (total_delayed) *total_delayed = skip_state->total_nmis_delayed;
}

// ===== INTEGRATION WITH INTERRUPT RECOGNITION =====

bool nmi_skipping_should_block_nmi(const nmi_skipping_state_t *skip_state,
                                  const struct interrupt_recognition_t *int_rec) {
    if (!skip_state || !int_rec) return false;
    
    // Block NMI if any skipping condition is active
    return nmi_skipping_should_skip_nmi(skip_state);
}

void nmi_skipping_modify_interrupt_recognition(const nmi_skipping_state_t *skip_state,
                                             struct interrupt_recognition_t *int_rec) {
    if (!skip_state || !int_rec) return;
    
    // If NMI should be skipped, prevent it from advancing through recognition stages
    if (nmi_skipping_should_skip_nmi(skip_state)) {
        // This would modify the interrupt recognition to block NMI advancement
        // Implementation would depend on the interrupt recognition internal structure
        // For now, this is a placeholder for the integration point
    }
}

void nmi_skipping_handle_lost_nmi(nmi_skipping_state_t *skip_state,
                                struct interrupt_recognition_t *int_rec) {
    if (!skip_state || !int_rec) return;
    
    // Handle NMI that was completely lost due to skipping conditions
    // This might involve clearing NMI edge latches or other cleanup
    
    if (skip_state->current_condition == NMI_SKIP_IRQ_VECTOR_FETCH ||
        skip_state->current_condition == NMI_SKIP_TIMING_WINDOW) {
        // These conditions cause complete NMI loss
        // Clear any pending NMI state in interrupt recognition
        interrupt_recognition_clear_interrupt(int_rec, VECTOR_NMI);
    }
}