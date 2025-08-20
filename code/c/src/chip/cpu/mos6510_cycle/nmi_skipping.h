#ifndef MOS6510_CYCLE_NMI_SKIPPING_H
#define MOS6510_CYCLE_NMI_SKIPPING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * MOS6510 NMI Skipping Conditions Implementation
 * Based on visual6502.org analysis (spec lines 271-288)
 * 
 * Implements all 4 critical NMI skipping conditions that cause NMIs
 * to be lost or delayed, exactly matching real hardware behavior.
 */

// Forward declarations
struct mos6510_state_s;
struct interrupt_recognition_t;

// ===== NMI SKIPPING CONDITION TYPES =====

/**
 * The 4 NMI skipping conditions discovered by visual6502 analysis
 */
typedef enum {
    NMI_SKIP_NONE               = 0,    // No skipping condition active
    NMI_SKIP_IRQ_VECTOR_FETCH   = 1,    // Lost NMI during IRQ vector fetch (Condition 1)
    NMI_SKIP_BRANCH_MASKING     = 2,    // Branch instruction masking (Condition 2)
    NMI_SKIP_TIMING_WINDOW      = 3,    // Critical timing window miss (Condition 3)
    NMI_SKIP_PIPELINE_DELAY     = 4     // Pipeline-induced delays with SEI/CLI (Condition 4)
} nmi_skip_condition_t;

// ===== NMI SKIPPING STATE TRACKING =====

/**
 * Complete NMI skipping state machine
 * Tracks all conditions and timing windows that can cause NMI loss
 */
typedef struct {
    // Current skipping condition (if any)
    nmi_skip_condition_t current_condition;
    bool nmi_skip_active;               // True if NMI is currently being skipped
    
    // Condition 1: Lost NMI during IRQ vector fetch
    bool irq_vector_fetch_active;       // IRQ vector fetch in progress
    uint8_t irq_vector_fetch_cycles;    // Cycles remaining in IRQ vector fetch
    bool nmi_lost_during_irq_vector;    // NMI was lost during IRQ vector fetch
    
    // Condition 2: Branch instruction masking
    bool branch_t3_to_t1f_sequence;     // T3→T1F branch sequence detected
    bool branch_masking_next_instr;     // Next instruction will be masked
    uint8_t branch_masking_cycles;      // Cycles remaining for masking
    
    // Condition 3: Critical timing window miss
    bool timing_window_t5_phi1;         // T5 φ1 timing window active
    bool timing_window_t1_phi1;         // T1 φ1 timing window active  
    bool nmi_down_at_t5_phi1;          // NMI went down at T5 φ1
    bool nmi_up_before_t1_phi1;        // NMI went up before T1 φ1
    uint8_t critical_timing_cycles;     // Cycles in critical timing window
    
    // Condition 4: Pipeline-induced delays with SEI/CLI
    bool sei_cli_instruction_active;    // SEI/CLI instruction executing
    bool status_register_delay;         // Status register update delayed by pipeline  
    uint8_t pipeline_delay_cycles;      // Cycles remaining for pipeline delay
    bool interrupt_slip_window;         // Window where interrupts can slip through
    
    // Timing and state tracking
    uint8_t last_timing_state;          // Previous timing state for transition detection
    bool phi1_phase_active;             // Current φ1/φ2 phase
    uint16_t skip_detection_cycles;     // Total cycles spent in skip detection
    
    // Statistics and debugging
    uint32_t condition1_count;          // Count of Condition 1 skips
    uint32_t condition2_count;          // Count of Condition 2 skips  
    uint32_t condition3_count;          // Count of Condition 3 skips
    uint32_t condition4_count;          // Count of Condition 4 skips
    uint32_t total_nmis_skipped;        // Total NMIs lost to skipping
    uint32_t total_nmis_delayed;        // Total NMIs delayed (but not lost)
} nmi_skipping_state_t;

// ===== NMI SKIPPING DETECTION AND MANAGEMENT =====

/**
 * Initialize NMI skipping detection system
 */
void nmi_skipping_init(nmi_skipping_state_t *skip_state);

/**
 * Reset NMI skipping state to hardware reset condition
 */
void nmi_skipping_reset(nmi_skipping_state_t *skip_state);

/**
 * Update NMI skipping detection (called every CPU cycle)
 * This is the main function that detects all 4 skipping conditions
 */
void nmi_skipping_update(nmi_skipping_state_t *skip_state,
                        struct mos6510_state_s *cpu,
                        struct interrupt_recognition_t *int_rec,
                        bool nmi_pin, bool irq_pin);

/**
 * Check if NMI should be skipped due to any active condition
 */
bool nmi_skipping_should_skip_nmi(const nmi_skipping_state_t *skip_state);

/**
 * Get the current NMI skipping condition (if any)
 */
nmi_skip_condition_t nmi_skipping_get_condition(const nmi_skipping_state_t *skip_state);

/**
 * Clear NMI skipping condition after handling
 */
void nmi_skipping_clear_condition(nmi_skipping_state_t *skip_state);

// ===== CONDITION 1: LOST NMI DURING IRQ VECTOR FETCH =====

/**
 * Detect and handle Condition 1: Lost NMI during IRQ vector fetch
 * 
 * From spec: "NMI duration < 3 cycles during IRQ vector fetch"
 * Result: "NMI completely lost, never serviced"
 */
void nmi_skipping_condition1_update(nmi_skipping_state_t *skip_state,
                                   struct mos6510_state_s *cpu,
                                   struct interrupt_recognition_t *int_rec,
                                   bool nmi_pin);

/**
 * Check if IRQ vector fetch is currently active
 */
bool nmi_skipping_irq_vector_fetch_active(const struct mos6510_state_s *cpu,
                                         const struct interrupt_recognition_t *int_rec);

/**
 * Detect NMI loss during IRQ vector fetch sequence
 */
void nmi_skipping_detect_nmi_loss_during_irq(nmi_skipping_state_t *skip_state,
                                            bool nmi_pin);

// ===== CONDITION 2: BRANCH INSTRUCTION MASKING =====

/**
 * Detect and handle Condition 2: Branch instruction masking
 * 
 * From spec: "T1F is preceded by T3 (not T0 or T2), so no interrupt can happen on the next instruction"
 * Result: "You can mask NMIs this way even"
 */
void nmi_skipping_condition2_update(nmi_skipping_state_t *skip_state,
                                   struct mos6510_state_s *cpu,
                                   bool nmi_pin);

/**
 * Detect T3→T1F branch sequence that masks following instruction
 */
bool nmi_skipping_detect_branch_t3_to_t1f(const nmi_skipping_state_t *skip_state,
                                         const struct mos6510_state_s *cpu);

/**
 * Check if next instruction will be masked by branch sequence
 */
bool nmi_skipping_next_instruction_masked(const nmi_skipping_state_t *skip_state);

// ===== CONDITION 3: CRITICAL TIMING WINDOW MISS =====

/**
 * Detect and handle Condition 3: Critical timing window miss
 * 
 * From spec: "Putting NMI down at T5 phase 1 or later and raising it back up again before T1 phase 1"
 * Result: "NMI lost due to timing window miss"
 */
void nmi_skipping_condition3_update(nmi_skipping_state_t *skip_state,
                                   struct mos6510_state_s *cpu,
                                   bool nmi_pin, bool phi1_phase);

/**
 * Detect T5 φ1 timing window entry
 */
void nmi_skipping_detect_t5_phi1_window(nmi_skipping_state_t *skip_state,
                                       const struct mos6510_state_s *cpu,
                                       bool phi1_phase);

/**
 * Detect T1 φ1 timing window and check for NMI loss
 */
void nmi_skipping_detect_t1_phi1_window(nmi_skipping_state_t *skip_state,
                                       const struct mos6510_state_s *cpu,
                                       bool phi1_phase, bool nmi_pin);

// ===== CONDITION 4: PIPELINE-INDUCED DELAYS =====

/**
 * Detect and handle Condition 4: Pipeline-induced delays with SEI/CLI
 * 
 * From spec: "SEI/CLI affect status register during the following instruction, due to the 6502 pipelining"
 * Result: "1-instruction delay where interrupts can slip through"
 */
void nmi_skipping_condition4_update(nmi_skipping_state_t *skip_state,
                                   struct mos6510_state_s *cpu,
                                   bool nmi_pin);

/**
 * Detect SEI/CLI instruction execution
 */
bool nmi_skipping_detect_sei_cli_instruction(const struct mos6510_state_s *cpu);

/**
 * Check if status register update is delayed by pipeline
 */
bool nmi_skipping_status_register_delayed(const nmi_skipping_state_t *skip_state,
                                         const struct mos6510_state_s *cpu);

/**
 * Detect interrupt slip window during pipeline delay
 */
void nmi_skipping_detect_interrupt_slip_window(nmi_skipping_state_t *skip_state,
                                              bool nmi_pin);

// ===== TIMING STATE TRACKING =====

/**
 * Update timing state tracking for skip detection
 */
void nmi_skipping_update_timing_tracking(nmi_skipping_state_t *skip_state,
                                        const struct mos6510_state_s *cpu,
                                        bool phi1_phase);

/**
 * Detect timing state transitions for skip condition triggers
 */
bool nmi_skipping_timing_transition_detected(const nmi_skipping_state_t *skip_state,
                                            const struct mos6510_state_s *cpu,
                                            uint8_t from_state, uint8_t to_state);

/**
 * Check if CPU is in specific timing state
 * Note: Implementation moved to .c file to avoid incomplete type issues
 */
bool nmi_skipping_in_timing_state(const struct mos6510_state_s *cpu, uint8_t timing_state);

// ===== VALIDATION AND DEBUGGING =====

/**
 * Validate NMI skipping state for consistency
 */
bool nmi_skipping_validate(const nmi_skipping_state_t *skip_state);

/**
 * Generate human-readable dump of NMI skipping state
 */
void nmi_skipping_dump(const nmi_skipping_state_t *skip_state, char *buffer, size_t buffer_size);

/**
 * Get NMI skip condition name for debugging
 */
const char* nmi_skip_condition_name(nmi_skip_condition_t condition);

/**
 * Get detailed description of current skipping condition
 */
void nmi_skipping_get_condition_description(const nmi_skipping_state_t *skip_state,
                                           char *buffer, size_t buffer_size);

/**
 * Get statistics about NMI skipping occurrences
 */
void nmi_skipping_get_statistics(const nmi_skipping_state_t *skip_state,
                               uint32_t *condition1_count, uint32_t *condition2_count,
                               uint32_t *condition3_count, uint32_t *condition4_count,
                               uint32_t *total_skipped, uint32_t *total_delayed);

// ===== INTEGRATION WITH INTERRUPT RECOGNITION =====

/**
 * Check if NMI should be blocked due to skipping condition
 * This integrates with the interrupt recognition system
 */
bool nmi_skipping_should_block_nmi(const nmi_skipping_state_t *skip_state,
                                  const struct interrupt_recognition_t *int_rec);

/**
 * Modify interrupt recognition based on skipping conditions
 */
void nmi_skipping_modify_interrupt_recognition(const nmi_skipping_state_t *skip_state,
                                             struct interrupt_recognition_t *int_rec);

/**
 * Handle NMI that was skipped/lost due to hardware conditions
 */
void nmi_skipping_handle_lost_nmi(nmi_skipping_state_t *skip_state,
                                struct interrupt_recognition_t *int_rec);

// ===== INLINE HELPER FUNCTIONS =====

/**
 * Quick check if any NMI skipping condition is active
 */
static inline bool nmi_skipping_any_condition_active(const nmi_skipping_state_t *skip_state) {
    return skip_state && skip_state->current_condition != NMI_SKIP_NONE;
}

/**
 * Check if specific skip condition is active
 */
static inline bool nmi_skipping_condition_active(const nmi_skipping_state_t *skip_state,
                                                nmi_skip_condition_t condition) {
    return skip_state && (skip_state->current_condition == condition);
}

/**
 * Get total count of all NMI skipping events
 */
static inline uint32_t nmi_skipping_get_total_events(const nmi_skipping_state_t *skip_state) {
    if (!skip_state) return 0;
    return skip_state->condition1_count + skip_state->condition2_count +
           skip_state->condition3_count + skip_state->condition4_count;
}

#endif // MOS6510_CYCLE_NMI_SKIPPING_H