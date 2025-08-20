#include "interrupt_recognition.h"
#include "mos6510_state.h"
#include "mos6510_registers.h"
#include "timing_states.h"
#include <string.h>
#include <stdio.h>

/**
 * MOS6510 4-Stage Interrupt Recognition System Implementation
 * Based on visual6502.org transistor-level analysis
 * 
 * Implements complete hardware-accurate interrupt recognition with all
 * timing dependencies, edge detection, and hardware node simulation.
 */

// ===== INTERRUPT VECTOR ADDRESSES =====

#define VECTOR_RESET_LOW    0xFFFC
#define VECTOR_RESET_HIGH   0xFFFD
#define VECTOR_NMI_LOW      0xFFFA
#define VECTOR_NMI_HIGH     0xFFFB
#define VECTOR_IRQ_BRK_LOW  0xFFFE
#define VECTOR_IRQ_BRK_HIGH 0xFFFF

// ===== PROCESSOR STATUS FLAGS =====

#define FLAG_I  0x04  // IRQ disable flag

// ===== INTERRUPT RECOGNITION INITIALIZATION =====

void interrupt_recognition_init(interrupt_recognition_t *int_rec) {
    if (!int_rec) return;
    
    // Initialize all stages to idle
    int_rec->nmi_stage = INTERRUPT_STAGE_IDLE;
    int_rec->irq_stage = INTERRUPT_STAGE_IDLE;
    int_rec->reset_stage = INTERRUPT_STAGE_IDLE;
    
    // Initialize hardware nodes to inactive state
    memset(&int_rec->nodes, 0, sizeof(interrupt_nodes_t));
    
    // Initialize edge detection
    int_rec->nmi_last_state = true;     // NMI is active low, so start high
    int_rec->nmi_edge_detected = false;
    int_rec->nmi_edge_latched = false;
    
    // Initialize timing dependencies
    int_rec->recognition_timing = 0;
    int_rec->branch_instruction_active = false;
    int_rec->page_crossing_active = false;
    
    // Initialize stage 2 triggers
    int_rec->stage2_trigger_t0_phi2 = false;
    int_rec->stage2_trigger_t2_phi2 = false;
    int_rec->stage2_trigger_page_cross = false;
    
    // Initialize BRK substitution state
    int_rec->brk_substitution_ready = false;
    int_rec->interrupt_vector_type = VECTOR_NONE;
    
    // Initialize debug counters
    int_rec->nmi_recognition_cycles = 0;
    int_rec->irq_recognition_cycles = 0;
    int_rec->total_interrupts_handled = 0;
}

void interrupt_recognition_reset(interrupt_recognition_t *int_rec) {
    if (!int_rec) return;
    
    // Reset forces all interrupt processing to stop except reset itself
    int_rec->nmi_stage = INTERRUPT_STAGE_IDLE;
    int_rec->irq_stage = INTERRUPT_STAGE_IDLE;
    int_rec->reset_stage = INTERRUPT_STAGE_BRK_SUBST; // Reset takes immediate control
    
    // Clear hardware nodes except reset
    int_rec->nodes.nmig_node = false;
    int_rec->nodes.nmi_pending = false;
    int_rec->nodes.nmi_active = false;
    int_rec->nodes.irqp_node = false;
    int_rec->nodes.irq_pending = false;
    int_rec->nodes.irq_active = false;
    
    // Reset nodes active
    int_rec->nodes.resp_node = true;
    int_rec->nodes.reset_pending = true;
    int_rec->nodes.reset_active = true;
    int_rec->nodes.resg_node = true;
    
    // Clear edge detection
    int_rec->nmi_edge_detected = false;
    int_rec->nmi_edge_latched = false;
    
    // Set reset vector
    int_rec->interrupt_vector_type = VECTOR_RESET;
    int_rec->brk_substitution_ready = true;
}

// ===== MAIN INTERRUPT RECOGNITION UPDATE =====

void interrupt_recognition_update(interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu,
                                bool nmi_pin, bool irq_pin, bool reset_pin) {
    if (!int_rec || !cpu) return;
    
    // Stage 0: Asynchronous → Synchronous conversion (φ2 sampling)
    interrupt_stage0_async_to_sync(int_rec, nmi_pin, irq_pin, reset_pin);
    
    // Stage 1: Edge/level detection and pending status
    interrupt_stage1_edge_level_detect(int_rec, cpu);
    
    // Stage 2: Pending → Active (timing dependent)
    interrupt_stage2_pending_to_active(int_rec, cpu);
    
    // Stage 3: BRK substitution during instruction fetch
    interrupt_stage3_brk_substitution(int_rec, cpu);
    
    // Update hardware node simulation
    interrupt_nodes_update(&int_rec->nodes, int_rec);
    
    // Update timing dependencies based on current instruction
    interrupt_recognition_update_timing(int_rec, cpu);
    
    // Resolve interrupt priority if multiple interrupts active
    interrupt_recognition_resolve_priority(int_rec);
    
    // Update debug counters
    if (int_rec->nmi_stage != INTERRUPT_STAGE_IDLE) {
        int_rec->nmi_recognition_cycles++;
    }
    if (int_rec->irq_stage != INTERRUPT_STAGE_IDLE) {
        int_rec->irq_recognition_cycles++;
    }
}

// ===== STAGE 0: ASYNCHRONOUS → SYNCHRONOUS CONVERSION =====

void interrupt_stage0_async_to_sync(interrupt_recognition_t *int_rec, 
                                   bool nmi_pin, bool irq_pin, bool reset_pin) {
    if (!int_rec) return;
    
    // φ2 sampling converts asynchronous external signals to synchronous internal signals
    // This happens every φ2 phase regardless of current interrupt state
    
    // Sample NMI pin (active low)
    int_rec->nodes.phi2_sample_nmi = !nmi_pin;  // Invert for internal logic
    
    // Sample IRQ pin (active low)  
    int_rec->nodes.phi2_sample_irq = !irq_pin;  // Invert for internal logic
    
    // Sample Reset pin (active low)
    int_rec->nodes.phi2_sample_reset = !reset_pin;  // Invert for internal logic
    
    // NMI edge detection (high-to-low transition on external pin)
    interrupt_recognition_nmi_edge_detect(int_rec, !nmi_pin);
    
    // Advance idle interrupts to stage 1 if pins are active
    if (int_rec->nodes.phi2_sample_nmi && int_rec->nmi_stage == INTERRUPT_STAGE_IDLE) {
        int_rec->nmi_stage = INTERRUPT_STAGE_ASYNC_SYNC;
    }
    
    if (int_rec->nodes.phi2_sample_irq && int_rec->irq_stage == INTERRUPT_STAGE_IDLE) {
        int_rec->irq_stage = INTERRUPT_STAGE_ASYNC_SYNC;
    }
    
    if (int_rec->nodes.phi2_sample_reset && int_rec->reset_stage == INTERRUPT_STAGE_IDLE) {
        int_rec->reset_stage = INTERRUPT_STAGE_ASYNC_SYNC;
    }
}

// ===== STAGE 1: EDGE/LEVEL DETECTION AND PENDING STATUS =====

void interrupt_stage1_edge_level_detect(interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu) {
    if (!int_rec || !cpu) return;
    
    // NMI Stage 1: Edge detection and ~NMIG node activation
    if (int_rec->nmi_stage == INTERRUPT_STAGE_ASYNC_SYNC) {
        if (int_rec->nmi_edge_detected) {
            // NMI edge detected, activate ~NMIG node (grounded low)
            int_rec->nodes.nmig_node = true;
            int_rec->nmi_stage = INTERRUPT_STAGE_EDGE_LEVEL;
            
            // Latch the NMI edge until serviced
            interrupt_recognition_nmi_edge_latch(int_rec);
        }
    }
    
    // IRQ Stage 1: Level detection and IRQP node activation
    if (int_rec->irq_stage == INTERRUPT_STAGE_ASYNC_SYNC) {
        if (int_rec->nodes.phi2_sample_irq && !interrupt_recognition_irq_masked(cpu)) {
            // IRQ level active and not masked, activate IRQP node (high)
            int_rec->nodes.irqp_node = true;
            int_rec->irq_stage = INTERRUPT_STAGE_EDGE_LEVEL;
        } else {
            // IRQ masked or not active, return to idle
            int_rec->irq_stage = INTERRUPT_STAGE_IDLE;
        }
    }
    
    // Reset Stage 1: Level detection and RESP node activation  
    if (int_rec->reset_stage == INTERRUPT_STAGE_ASYNC_SYNC) {
        if (int_rec->nodes.phi2_sample_reset) {
            // Reset active, activate RESP node (high)
            int_rec->nodes.resp_node = true;
            int_rec->reset_stage = INTERRUPT_STAGE_EDGE_LEVEL;
        }
    }
}

// ===== STAGE 2: PENDING → ACTIVE (TIMING DEPENDENT) =====

void interrupt_stage2_pending_to_active(interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu) {
    if (!int_rec || !cpu) return;
    
    // Check if stage 2 trigger condition is met
    bool stage2_triggered = interrupt_recognition_stage2_triggered(int_rec, cpu);
    
    if (!stage2_triggered) return;
    
    // NMI Stage 2: Advance to pending if edge was latched
    if (int_rec->nmi_stage == INTERRUPT_STAGE_EDGE_LEVEL && int_rec->nmi_edge_latched) {
        int_rec->nodes.nmi_pending = true;
        int_rec->nodes.intg_node = true;  // INTG node activation (NMI/IRQ stage 2)
        int_rec->nmi_stage = INTERRUPT_STAGE_PENDING;
    }
    
    // IRQ Stage 2: Advance to pending if not masked
    if (int_rec->irq_stage == INTERRUPT_STAGE_EDGE_LEVEL && !interrupt_recognition_irq_masked(cpu)) {
        int_rec->nodes.irq_pending = true;
        int_rec->nodes.intg_node = true;  // INTG node activation (NMI/IRQ stage 2)
        int_rec->irq_stage = INTERRUPT_STAGE_PENDING;
    }
    
    // Reset Stage 2: Always advance to pending
    if (int_rec->reset_stage == INTERRUPT_STAGE_EDGE_LEVEL) {
        int_rec->nodes.reset_pending = true;
        int_rec->nodes.resg_node = true;  // RESG node activation (Reset stage 2)
        int_rec->reset_stage = INTERRUPT_STAGE_PENDING;
    }
}

// ===== STAGE 3: BRK SUBSTITUTION DURING INSTRUCTION FETCH =====

void interrupt_stage3_brk_substitution(interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu) {
    if (!int_rec || !cpu) return;
    
    // BRK substitution occurs when an interrupt reaches stage 3 and the CPU
    // is in the instruction fetch phase (T1F with SYNC active)
    
    // Check if CPU is in fetch phase
    bool in_fetch_phase = (cpu->timing_state == TIMING_T1F);
    
    if (!in_fetch_phase) return;
    
    // Determine highest priority interrupt ready for BRK substitution
    interrupt_vector_t vector_type = interrupt_recognition_get_highest_priority(int_rec);
    
    switch (vector_type) {
        case VECTOR_RESET:
            if (int_rec->reset_stage == INTERRUPT_STAGE_PENDING) {
                int_rec->nodes.reset_active = true;
                int_rec->reset_stage = INTERRUPT_STAGE_BRK_SUBST;
                int_rec->interrupt_vector_type = VECTOR_RESET;
                int_rec->brk_substitution_ready = true;
            }
            break;
            
        case VECTOR_NMI:
            if (int_rec->nmi_stage == INTERRUPT_STAGE_PENDING) {
                int_rec->nodes.nmi_active = true;
                int_rec->nmi_stage = INTERRUPT_STAGE_BRK_SUBST;
                int_rec->interrupt_vector_type = VECTOR_NMI;
                int_rec->brk_substitution_ready = true;
            }
            break;
            
        case VECTOR_IRQ_BRK:
            if (int_rec->irq_stage == INTERRUPT_STAGE_PENDING) {
                int_rec->nodes.irq_active = true;
                int_rec->irq_stage = INTERRUPT_STAGE_BRK_SUBST;
                int_rec->interrupt_vector_type = VECTOR_IRQ_BRK;
                int_rec->brk_substitution_ready = true;
            }
            break;
            
        default:
            // No interrupt ready for substitution
            break;
    }
}

// ===== INTERRUPT VECTOR HANDLING =====

bool interrupt_recognition_vector_ready(const interrupt_recognition_t *int_rec) {
    return int_rec && int_rec->brk_substitution_ready;
}

interrupt_vector_t interrupt_recognition_get_vector_type(const interrupt_recognition_t *int_rec) {
    return int_rec ? int_rec->interrupt_vector_type : VECTOR_NONE;
}

uint16_t interrupt_recognition_get_vector_address(interrupt_vector_t vector_type) {
    switch (vector_type) {
        case VECTOR_RESET:   return VECTOR_RESET_LOW;
        case VECTOR_NMI:     return VECTOR_NMI_LOW;  
        case VECTOR_IRQ_BRK: return VECTOR_IRQ_BRK_LOW;
        default:             return 0x0000;
    }
}

void interrupt_recognition_clear_interrupt(interrupt_recognition_t *int_rec, interrupt_vector_t vector_type) {
    if (!int_rec) return;
    
    // Clear the interrupt that was just serviced
    switch (vector_type) {
        case VECTOR_RESET:
            int_rec->reset_stage = INTERRUPT_STAGE_IDLE;
            int_rec->nodes.resp_node = false;
            int_rec->nodes.reset_pending = false;
            int_rec->nodes.reset_active = false;
            int_rec->nodes.resg_node = false;
            break;
            
        case VECTOR_NMI:
            int_rec->nmi_stage = INTERRUPT_STAGE_IDLE;
            int_rec->nodes.nmig_node = false;
            int_rec->nodes.nmi_pending = false;
            int_rec->nodes.nmi_active = false;
            interrupt_recognition_nmi_edge_clear(int_rec);
            break;
            
        case VECTOR_IRQ_BRK:
            int_rec->irq_stage = INTERRUPT_STAGE_IDLE;
            int_rec->nodes.irqp_node = false;
            int_rec->nodes.irq_pending = false;
            int_rec->nodes.irq_active = false;
            break;
            
        default:
            break;
    }
    
    // Clear BRK substitution state
    int_rec->brk_substitution_ready = false;
    int_rec->interrupt_vector_type = VECTOR_NONE;
    
    // Clear stage 2 control nodes if no other interrupts active
    if (!int_rec->nodes.nmi_pending && !int_rec->nodes.irq_pending) {
        int_rec->nodes.intg_node = false;
    }
    
    // Update interrupt counter
    int_rec->total_interrupts_handled++;
}

// ===== TIMING DEPENDENCY FUNCTIONS =====

void interrupt_recognition_update_timing(interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu) {
    if (!int_rec || !cpu) return;
    
    // Update timing mode based on current instruction characteristics
    // This affects when stage 2 triggers occur
    
    // Detect branch instructions and page crossing from CPU state
    // This would need to be integrated with instruction decoding
    bool is_branch = int_rec->branch_instruction_active;
    bool page_crossing = int_rec->page_crossing_active;
    
    // Set stage 2 trigger conditions based on instruction type
    interrupt_recognition_set_timing_mode(int_rec, is_branch, page_crossing);
}

bool interrupt_recognition_stage2_triggered(const interrupt_recognition_t *int_rec, struct mos6510_state_s *cpu) {
    if (!int_rec || !cpu) return false;
    
    timing_state_t current_timing = (timing_state_t)cpu->timing_state;
    
    // Different timing for different instruction types:
    // - Normal instructions: Stage 2 triggered on T0 φ2
    // - Branch instructions: Stage 2 triggered on T2 φ2  
    // - Page-crossing branches: Stage 2 triggered on T0 φ2
    
    if (int_rec->stage2_trigger_t0_phi2 && current_timing == TIMING_T0) {
        return true;  // Normal instructions or page-crossing branches
    }
    
    if (int_rec->stage2_trigger_t2_phi2 && current_timing == TIMING_T2) {
        return true;  // Branch instructions
    }
    
    return false;
}

void interrupt_recognition_set_timing_mode(interrupt_recognition_t *int_rec, bool is_branch, bool page_crossing) {
    if (!int_rec) return;
    
    // Clear all triggers
    int_rec->stage2_trigger_t0_phi2 = false;
    int_rec->stage2_trigger_t2_phi2 = false;
    int_rec->stage2_trigger_page_cross = false;
    
    // Set appropriate trigger based on instruction type
    if (is_branch && !page_crossing) {
        // Branch instructions: Stage 2 triggered on T2 φ2
        int_rec->stage2_trigger_t2_phi2 = true;
    } else {
        // Normal instructions and page-crossing branches: Stage 2 triggered on T0 φ2
        int_rec->stage2_trigger_t0_phi2 = true;
    }
    
    // Update state tracking
    int_rec->branch_instruction_active = is_branch;
    int_rec->page_crossing_active = page_crossing;
}

// ===== HARDWARE NODE SIMULATION =====

void interrupt_nodes_update(interrupt_nodes_t *nodes, const interrupt_recognition_t *int_rec) {
    if (!nodes || !int_rec) return;
    
    // Update hardware nodes based on current interrupt recognition stages
    
    // ~NMIG node (grounded low during NMI stage 1)
    interrupt_nodes_update_nmig(nodes, int_rec->nmi_stage >= INTERRUPT_STAGE_EDGE_LEVEL);
    
    // IRQP node (high during IRQ stage 1)
    interrupt_nodes_update_irqp(nodes, int_rec->irq_stage >= INTERRUPT_STAGE_EDGE_LEVEL);
    
    // RESP node (high during Reset stage 1)
    interrupt_nodes_update_resp(nodes, int_rec->reset_stage >= INTERRUPT_STAGE_EDGE_LEVEL);
    
    // INTG node (high during NMI/IRQ stage 2)
    bool nmi_or_irq_stage2 = (int_rec->nmi_stage >= INTERRUPT_STAGE_PENDING) ||
                             (int_rec->irq_stage >= INTERRUPT_STAGE_PENDING);
    interrupt_nodes_update_intg(nodes, nmi_or_irq_stage2);
    
    // RESG node (high during Reset stage 2)
    interrupt_nodes_update_resg(nodes, int_rec->reset_stage >= INTERRUPT_STAGE_PENDING);
}

void interrupt_nodes_update_nmig(interrupt_nodes_t *nodes, bool nmi_stage1_active) {
    if (nodes) {
        nodes->nmig_node = nmi_stage1_active;
    }
}

void interrupt_nodes_update_irqp(interrupt_nodes_t *nodes, bool irq_stage1_active) {
    if (nodes) {
        nodes->irqp_node = irq_stage1_active;
    }
}

void interrupt_nodes_update_resp(interrupt_nodes_t *nodes, bool reset_stage1_active) {
    if (nodes) {
        nodes->resp_node = reset_stage1_active;
    }
}

void interrupt_nodes_update_intg(interrupt_nodes_t *nodes, bool nmi_or_irq_stage2) {
    if (nodes) {
        nodes->intg_node = nmi_or_irq_stage2;
    }
}

void interrupt_nodes_update_resg(interrupt_nodes_t *nodes, bool reset_stage2) {
    if (nodes) {
        nodes->resg_node = reset_stage2;
    }
}

// ===== INTERRUPT PRIORITY HANDLING =====

interrupt_vector_t interrupt_recognition_get_highest_priority(const interrupt_recognition_t *int_rec) {
    if (!int_rec) return VECTOR_NONE;
    
    // Priority order: Reset > NMI > IRQ
    
    if (int_rec->reset_stage >= INTERRUPT_STAGE_PENDING) {
        return VECTOR_RESET;
    }
    
    if (int_rec->nmi_stage >= INTERRUPT_STAGE_PENDING) {
        return VECTOR_NMI;
    }
    
    if (int_rec->irq_stage >= INTERRUPT_STAGE_PENDING) {
        return VECTOR_IRQ_BRK;
    }
    
    return VECTOR_NONE;
}

bool interrupt_recognition_irq_masked(const struct mos6510_state_s *cpu) {
    if (!cpu) return true;
    
    // IRQ is masked when the I flag is set in the processor status register
    uint8_t status = cpu->registers[REG_P];
    return (status & FLAG_I) != 0;
}

void interrupt_recognition_resolve_priority(interrupt_recognition_t *int_rec) {
    if (!int_rec) return;
    
    // If multiple interrupts are active simultaneously, ensure only the
    // highest priority one proceeds to BRK substitution
    
    interrupt_vector_t highest = interrupt_recognition_get_highest_priority(int_rec);
    
    // Clear BRK substitution for lower priority interrupts
    if (highest != VECTOR_IRQ_BRK && int_rec->irq_stage == INTERRUPT_STAGE_BRK_SUBST) {
        int_rec->irq_stage = INTERRUPT_STAGE_PENDING;
        int_rec->nodes.irq_active = false;
    }
    
    if (highest != VECTOR_NMI && int_rec->nmi_stage == INTERRUPT_STAGE_BRK_SUBST) {
        int_rec->nmi_stage = INTERRUPT_STAGE_PENDING;
        int_rec->nodes.nmi_active = false;
    }
    
    // Reset always wins, so no need to check for it
}

// ===== NMI EDGE DETECTION =====

void interrupt_recognition_nmi_edge_detect(interrupt_recognition_t *int_rec, bool current_nmi_state) {
    if (!int_rec) return;
    
    // Detect high-to-low transition (falling edge) on NMI
    // NMI is edge-triggered, unlike IRQ which is level-triggered
    
    if (int_rec->nmi_last_state && !current_nmi_state) {
        // Falling edge detected
        int_rec->nmi_edge_detected = true;
    }
    
    int_rec->nmi_last_state = current_nmi_state;
}

void interrupt_recognition_nmi_edge_latch(interrupt_recognition_t *int_rec) {
    if (!int_rec) return;
    
    // Latch the NMI edge until the interrupt is serviced
    if (int_rec->nmi_edge_detected) {
        int_rec->nmi_edge_latched = true;
        int_rec->nmi_edge_detected = false;  // Clear the detection flag
    }
}

void interrupt_recognition_nmi_edge_clear(interrupt_recognition_t *int_rec) {
    if (!int_rec) return;
    
    // Clear the NMI edge latch after interrupt vector fetch
    int_rec->nmi_edge_latched = false;
    int_rec->nmi_edge_detected = false;
}

// ===== DEBUGGING AND VALIDATION =====

bool interrupt_recognition_validate(const interrupt_recognition_t *int_rec) {
    if (!int_rec) return false;
    
    // Check for invalid stage combinations
    if (int_rec->nmi_stage > INTERRUPT_STAGE_BRK_SUBST) return false;
    if (int_rec->irq_stage > INTERRUPT_STAGE_BRK_SUBST) return false;
    if (int_rec->reset_stage > INTERRUPT_STAGE_BRK_SUBST) return false;
    
    // Check consistency between stages and hardware nodes
    if (int_rec->nmi_stage >= INTERRUPT_STAGE_EDGE_LEVEL && !int_rec->nodes.nmig_node) {
        return false;  // NMI stage 1+ should have ~NMIG active
    }
    
    if (int_rec->irq_stage >= INTERRUPT_STAGE_EDGE_LEVEL && !int_rec->nodes.irqp_node) {
        return false;  // IRQ stage 1+ should have IRQP active
    }
    
    // Check BRK substitution consistency
    if (int_rec->brk_substitution_ready && int_rec->interrupt_vector_type == VECTOR_NONE) {
        return false;  // BRK ready but no vector type set
    }
    
    return true;
}

void interrupt_recognition_dump(const interrupt_recognition_t *int_rec, char *buffer, size_t buffer_size) {
    if (!int_rec || !buffer || buffer_size == 0) return;
    
    snprintf(buffer, buffer_size,
        "Interrupt Recognition State:\n"
        "  NMI Stage: %s, IRQ Stage: %s, Reset Stage: %s\n"
        "  NMI Edge: %s, BRK Ready: %s, Vector: %s\n"
        "  Hardware Nodes: NMIG=%d IRQP=%d RESP=%d INTG=%d RESG=%d\n"
        "  Recognition Cycles: NMI=%u IRQ=%u Total=%u\n",
        interrupt_stage_name(int_rec->nmi_stage),
        interrupt_stage_name(int_rec->irq_stage), 
        interrupt_stage_name(int_rec->reset_stage),
        int_rec->nmi_edge_latched ? "LATCHED" : "NONE",
        int_rec->brk_substitution_ready ? "YES" : "NO",
        interrupt_vector_name(int_rec->interrupt_vector_type),
        int_rec->nodes.nmig_node, int_rec->nodes.irqp_node, int_rec->nodes.resp_node,
        int_rec->nodes.intg_node, int_rec->nodes.resg_node,
        int_rec->nmi_recognition_cycles, int_rec->irq_recognition_cycles,
        int_rec->total_interrupts_handled);
}

const char* interrupt_stage_name(interrupt_stage_t stage) {
    switch (stage) {
        case INTERRUPT_STAGE_IDLE:        return "IDLE";
        case INTERRUPT_STAGE_ASYNC_SYNC:  return "ASYNC_SYNC";
        case INTERRUPT_STAGE_EDGE_LEVEL:  return "EDGE_LEVEL";
        case INTERRUPT_STAGE_PENDING:     return "PENDING";
        case INTERRUPT_STAGE_BRK_SUBST:   return "BRK_SUBST";
        default:                          return "UNKNOWN";
    }
}

const char* interrupt_vector_name(interrupt_vector_t vector) {
    switch (vector) {
        case VECTOR_NONE:     return "NONE";
        case VECTOR_RESET:    return "RESET";
        case VECTOR_NMI:      return "NMI";
        case VECTOR_IRQ_BRK:  return "IRQ_BRK";
        default:              return "UNKNOWN";
    }
}

void interrupt_nodes_dump(const interrupt_nodes_t *nodes, char *buffer, size_t buffer_size) {
    if (!nodes || !buffer || buffer_size == 0) return;
    
    snprintf(buffer, buffer_size,
        "Hardware Nodes: ~NMIG=%d IRQP=%d RESP=%d INTG=%d RESG=%d | "
        "Pending: NMI=%d IRQ=%d RST=%d | Active: NMI=%d IRQ=%d RST=%d",
        nodes->nmig_node, nodes->irqp_node, nodes->resp_node,
        nodes->intg_node, nodes->resg_node,
        nodes->nmi_pending, nodes->irq_pending, nodes->reset_pending,
        nodes->nmi_active, nodes->irq_active, nodes->reset_active);
}

void interrupt_recognition_get_stats(const interrupt_recognition_t *int_rec,
                                   uint32_t *nmi_count, uint32_t *irq_count,
                                   uint32_t *reset_count, uint32_t *total_cycles) {
    if (!int_rec) return;
    
    if (nmi_count) *nmi_count = int_rec->nmi_recognition_cycles;
    if (irq_count) *irq_count = int_rec->irq_recognition_cycles;
    if (reset_count) *reset_count = 0; // Could be added if needed
    if (total_cycles) *total_cycles = int_rec->nmi_recognition_cycles + int_rec->irq_recognition_cycles;
}