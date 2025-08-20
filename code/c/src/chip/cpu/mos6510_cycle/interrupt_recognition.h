#ifndef MOS6510_CYCLE_INTERRUPT_RECOGNITION_H
#define MOS6510_CYCLE_INTERRUPT_RECOGNITION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * MOS6510 4-Stage Interrupt Recognition System
 * Based on visual6502.org transistor-level analysis (spec lines 256-268)
 * 
 * Implements complete hardware-accurate interrupt recognition process
 * with all timing dependencies and edge/level detection.
 */

// Forward declarations
struct mos6510_state_s;

// ===== 4-STAGE INTERRUPT RECOGNITION STATES =====

/**
 * 4-Stage interrupt recognition process (visual6502 discovery)
 * Each interrupt type follows this staged progression
 */
typedef enum {
    INTERRUPT_STAGE_IDLE        = 0,    // No interrupt activity
    INTERRUPT_STAGE_ASYNC_SYNC  = 1,    // Stage 0: Asynchronous → Synchronous (φ2 sampling)
    INTERRUPT_STAGE_EDGE_LEVEL  = 2,    // Stage 1: Edge/level detection and pending status
    INTERRUPT_STAGE_PENDING     = 3,    // Stage 2: Pending → Active (timing dependent)
    INTERRUPT_STAGE_BRK_SUBST   = 4     // Stage 3: BRK substitution during instruction fetch
} interrupt_stage_t;

// ===== HARDWARE INTERRUPT NODES =====

/**
 * Hardware node simulation (visual6502 internal nodes)
 * These represent actual internal CPU nodes discovered through analysis
 */
typedef struct {
    // NMI Hardware Nodes
    bool nmig_node;         // ~NMIG: NMI stage 1 (node grounded low)
    bool nmi_pending;       // NMI pending status (stage 2)
    bool nmi_active;        // NMI active and ready for vector
    
    // IRQ Hardware Nodes  
    bool irqp_node;         // IRQP: IRQ stage 1 (node high)
    bool irq_pending;       // IRQ pending status (stage 2)
    bool irq_active;        // IRQ active and ready for vector
    
    // Reset Hardware Nodes
    bool resp_node;         // RESP: Reset stage 1 (node high)
    bool reset_pending;     // Reset pending status (stage 2)
    bool reset_active;      // Reset active and taking control
    
    // Stage 2 Control Nodes
    bool intg_node;         // INTG: NMI/IRQ stage 2 (node high)
    bool resg_node;         // RESG: Reset stage 2 (node high)
    
    // φ2 Sampling State
    bool phi2_sample_nmi;   // φ2 sampled NMI state
    bool phi2_sample_irq;   // φ2 sampled IRQ state  
    bool phi2_sample_reset; // φ2 sampled Reset state
} interrupt_nodes_t;

// ===== INTERRUPT RECOGNITION STATE =====

/**
 * Complete interrupt recognition state machine
 * Tracks all 4 stages for all interrupt types simultaneously
 */
typedef struct {
    // Current interrupt stages
    interrupt_stage_t nmi_stage;        // Current NMI recognition stage
    interrupt_stage_t irq_stage;        // Current IRQ recognition stage
    interrupt_stage_t reset_stage;      // Current Reset recognition stage
    
    // Hardware node states
    interrupt_nodes_t nodes;            // Hardware node simulation
    
    // Edge detection state
    bool nmi_last_state;               // Previous NMI pin state for edge detection
    bool nmi_edge_detected;            // NMI edge detection latch
    bool nmi_edge_latched;             // NMI edge latched until serviced
    
    // Timing dependencies
    uint8_t recognition_timing;        // Recognition timing based on instruction type
    bool branch_instruction_active;    // Branch instruction affects timing
    bool page_crossing_active;         // Page crossing affects timing
    
    // Stage 2 triggers (timing dependent)
    bool stage2_trigger_t0_phi2;       // Normal instructions: T0 φ2
    bool stage2_trigger_t2_phi2;       // Branch instructions: T2 φ2
    bool stage2_trigger_page_cross;    // Page-crossing branches: T0 φ2
    
    // BRK substitution state (Stage 3)
    bool brk_substitution_ready;       // Ready to substitute BRK
    uint8_t interrupt_vector_type;     // Which interrupt vector to use
    
    // Debug and monitoring
    uint32_t nmi_recognition_cycles;   // Cycles spent in NMI recognition
    uint32_t irq_recognition_cycles;   // Cycles spent in IRQ recognition
    uint32_t total_interrupts_handled; // Total interrupt count
} interrupt_recognition_t;

// ===== INTERRUPT VECTOR TYPES =====

/**
 * Interrupt vector identifiers
 */
typedef enum {
    VECTOR_NONE     = 0,    // No vector
    VECTOR_RESET    = 1,    // Reset vector ($FFFC/$FFFD)
    VECTOR_NMI      = 2,    // NMI vector ($FFFA/$FFFB)
    VECTOR_IRQ_BRK  = 3     // IRQ/BRK vector ($FFFE/$FFFF)
} interrupt_vector_t;

// ===== INTERRUPT RECOGNITION FUNCTIONS =====

/**
 * Initialize interrupt recognition system
 */
void interrupt_recognition_init(interrupt_recognition_t *int_rec);

/**
 * Reset interrupt recognition to hardware reset state
 */
void interrupt_recognition_reset(interrupt_recognition_t *int_rec);

/**
 * Update interrupt recognition state (called during φ2 phase)
 * This is the main function that processes all 4 stages
 */
void interrupt_recognition_update(interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu, 
                                bool nmi_pin, bool irq_pin, bool reset_pin);

/**
 * Check if any interrupt is ready for vector fetch
 */
bool interrupt_recognition_vector_ready(const interrupt_recognition_t *int_rec);

/**
 * Get the highest priority interrupt vector type
 */
interrupt_vector_t interrupt_recognition_get_vector_type(const interrupt_recognition_t *int_rec);

/**
 * Get interrupt vector address for current interrupt
 */
uint16_t interrupt_recognition_get_vector_address(interrupt_vector_t vector_type);

/**
 * Clear interrupt after vector fetch completed
 */
void interrupt_recognition_clear_interrupt(interrupt_recognition_t *int_rec, interrupt_vector_t vector_type);

// ===== STAGE-SPECIFIC FUNCTIONS =====

/**
 * Stage 0: Asynchronous → Synchronous conversion (φ2 sampling)
 */
void interrupt_stage0_async_to_sync(interrupt_recognition_t *int_rec, 
                                   bool nmi_pin, bool irq_pin, bool reset_pin);

/**
 * Stage 1: Edge/level detection and pending status
 */
void interrupt_stage1_edge_level_detect(interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu);

/**
 * Stage 2: Pending → Active (timing dependent) 
 */
void interrupt_stage2_pending_to_active(interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu);

/**
 * Stage 3: BRK substitution during instruction fetch
 */
void interrupt_stage3_brk_substitution(interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu);

// ===== TIMING DEPENDENCY FUNCTIONS =====

/**
 * Update recognition timing based on current instruction type
 */
void interrupt_recognition_update_timing(interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu);

/**
 * Check if stage 2 trigger condition is met
 */
bool interrupt_recognition_stage2_triggered(const interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu);

/**
 * Handle different timing for different instruction types:
 * - Normal instructions: Stage 2 triggered on T0 φ2  
 * - Branch instructions: Stage 2 triggered on T2 φ2
 * - Page-crossing branches: Stage 2 triggered on T0 φ2
 */
void interrupt_recognition_set_timing_mode(interrupt_recognition_t *int_rec, 
                                          bool is_branch, bool page_crossing);

// ===== HARDWARE NODE SIMULATION =====

/**
 * Update hardware node states based on recognition stages
 */
void interrupt_nodes_update(interrupt_nodes_t *nodes, const interrupt_recognition_t *int_rec);

/**
 * Simulate ~NMIG node behavior (node grounded low)
 */
void interrupt_nodes_update_nmig(interrupt_nodes_t *nodes, bool nmi_stage1_active);

/**
 * Simulate IRQP node behavior (node high)  
 */
void interrupt_nodes_update_irqp(interrupt_nodes_t *nodes, bool irq_stage1_active);

/**
 * Simulate RESP node behavior (node high)
 */
void interrupt_nodes_update_resp(interrupt_nodes_t *nodes, bool reset_stage1_active);

/**
 * Simulate INTG node behavior (NMI/IRQ stage 2, node high)
 */
void interrupt_nodes_update_intg(interrupt_nodes_t *nodes, bool nmi_or_irq_stage2);

/**
 * Simulate RESG node behavior (Reset stage 2, node high)
 */
void interrupt_nodes_update_resg(interrupt_nodes_t *nodes, bool reset_stage2);

// ===== INTERRUPT PRIORITY HANDLING =====

/**
 * Determine interrupt priority (Reset > NMI > IRQ)
 */
interrupt_vector_t interrupt_recognition_get_highest_priority(const interrupt_recognition_t *int_rec);

/**
 * Check if I flag should mask IRQ (doesn't affect NMI or Reset)
 */
bool interrupt_recognition_irq_masked(const struct mos6510_state_s *cpu);

/**
 * Handle interrupt priority resolution when multiple interrupts are active
 */
void interrupt_recognition_resolve_priority(interrupt_recognition_t *int_rec);

// ===== NMI EDGE DETECTION =====

/**
 * Detect NMI falling edge (high-to-low transition)
 */
void interrupt_recognition_nmi_edge_detect(interrupt_recognition_t *int_rec, bool current_nmi_state);

/**
 * Latch NMI edge until interrupt is serviced
 */
void interrupt_recognition_nmi_edge_latch(interrupt_recognition_t *int_rec);

/**
 * Clear NMI edge latch after interrupt vector fetch
 */
void interrupt_recognition_nmi_edge_clear(interrupt_recognition_t *int_rec);

// ===== DEBUGGING AND VALIDATION =====

/**
 * Validate interrupt recognition state for consistency
 */
bool interrupt_recognition_validate(const interrupt_recognition_t *int_rec);

/**
 * Generate human-readable dump of interrupt recognition state
 */
void interrupt_recognition_dump(const interrupt_recognition_t *int_rec, char *buffer, size_t buffer_size);

/**
 * Get interrupt stage name for debugging
 */
const char* interrupt_stage_name(interrupt_stage_t stage);

/**
 * Get interrupt vector name for debugging
 */
const char* interrupt_vector_name(interrupt_vector_t vector);

/**
 * Get hardware node state summary for debugging
 */
void interrupt_nodes_dump(const interrupt_nodes_t *nodes, char *buffer, size_t buffer_size);

/**
 * Statistics and performance monitoring
 */
void interrupt_recognition_get_stats(const interrupt_recognition_t *int_rec, 
                                   uint32_t *nmi_count, uint32_t *irq_count, 
                                   uint32_t *reset_count, uint32_t *total_cycles);

// ===== INLINE HELPER FUNCTIONS =====

/**
 * Quick check if any interrupt is in progress
 */
static inline bool interrupt_recognition_active(const interrupt_recognition_t *int_rec) {
    return int_rec->nmi_stage != INTERRUPT_STAGE_IDLE ||
           int_rec->irq_stage != INTERRUPT_STAGE_IDLE ||
           int_rec->reset_stage != INTERRUPT_STAGE_IDLE;
}

/**
 * Check if NMI edge has been detected and latched
 */
static inline bool interrupt_recognition_nmi_edge_latched(const interrupt_recognition_t *int_rec) {
    return int_rec->nmi_edge_latched;
}

/**
 * Check if IRQ is currently active (not masked)
 */
static inline bool interrupt_recognition_irq_active(const interrupt_recognition_t *int_rec, 
                                                   const struct mos6510_state_s *cpu) {
    return int_rec->irq_stage >= INTERRUPT_STAGE_PENDING && 
           !interrupt_recognition_irq_masked(cpu);
}

/**
 * Get current recognition stage for specific interrupt type
 */
static inline interrupt_stage_t interrupt_recognition_get_stage(const interrupt_recognition_t *int_rec,
                                                              interrupt_vector_t vector_type) {
    switch (vector_type) {
        case VECTOR_NMI:     return int_rec->nmi_stage;
        case VECTOR_IRQ_BRK: return int_rec->irq_stage;
        case VECTOR_RESET:   return int_rec->reset_stage;
        default:             return INTERRUPT_STAGE_IDLE;
    }
}

#endif // MOS6510_CYCLE_INTERRUPT_RECOGNITION_H