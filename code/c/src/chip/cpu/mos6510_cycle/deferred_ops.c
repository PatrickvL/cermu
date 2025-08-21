#include "deferred_ops.h"
#include "mos6510_state.h"
#include "mos6510_registers.h"
#include "timing_states.h"
#include <string.h>
#include <stdio.h>

/**
 * MOS6510 Deferred Operation Architecture Implementation
 * 
 * Implements hardware-accurate φ1/φ2 phase execution timing according to 
 * visual6502.org analysis. Critical requirements:
 * - Address setup must be last operation (spec lines 353, 534-536)
 * - Deferred data operations until next tick (spec lines 541-549)
 * - RDY line handling (read cycles only, spec lines 552-554)
 */

// ===== INITIALIZATION AND RESET =====

void deferred_ops_init(deferred_ops_state_t* deferred_state) {
    if (!deferred_state) return;
    
    // Initialize operation queue
    memset(&deferred_state->queue, 0, sizeof(deferred_op_queue_t));
    deferred_state->queue.head = 0;
    deferred_state->queue.tail = 0;
    deferred_state->queue.count = 0;
    deferred_state->queue.queue_full = false;
    
    // Initialize phase coordination
    deferred_state->phi1_phase_active = false;
    deferred_state->phi2_phase_active = false;
    deferred_state->address_setup_deferred = false;
    
    // Initialize RDY line handling
    deferred_state->rdy_line_state = true;  // RDY inactive (high) by default
    deferred_state->rdy_affects_cycle = false;
    deferred_state->waiting_for_rdy = false;
    
    // Initialize address setup state
    deferred_state->deferred_address = 0x0000;
    deferred_state->address_setup_pending = false;
    deferred_state->address_setup_enabled = true;
    
    // Initialize execution coordination
    deferred_state->current_cycle = 0;
    deferred_state->deferred_until_cycle = 0;
    deferred_state->execution_stalled = false;
    
    // Initialize statistics
    deferred_state->phi1_operations_executed = 0;
    deferred_state->phi2_address_setups = 0;
    deferred_state->rdy_stall_cycles = 0;
}

void deferred_ops_reset(deferred_ops_state_t* deferred_state) {
    if (!deferred_state) return;
    
    // Clear operation queue but preserve statistics
    uint32_t total_ops = deferred_state->queue.total_operations;
    uint32_t peak_depth = deferred_state->queue.peak_queue_depth;
    uint32_t overflows = deferred_state->queue.queue_overflows;
    
    deferred_ops_init(deferred_state);
    
    // Restore statistics
    deferred_state->queue.total_operations = total_ops;
    deferred_state->queue.peak_queue_depth = peak_depth;
    deferred_state->queue.queue_overflows = overflows;
}

// ===== OPERATION QUEUE MANAGEMENT =====

bool deferred_ops_enqueue(deferred_ops_state_t* deferred_state, 
                         const deferred_operation_t* op) {
    if (!deferred_state || !op) return false;
    
    deferred_op_queue_t* queue = &deferred_state->queue;
    
    // Check for queue overflow
    if (queue->count >= DEFERRED_OP_QUEUE_SIZE) {
        queue->queue_full = true;
        queue->queue_overflows++;
        return false;  // Queue full
    }
    
    // Add operation to queue
    queue->operations[queue->tail] = *op;
    queue->tail = (queue->tail + 1) % DEFERRED_OP_QUEUE_SIZE;
    queue->count++;
    queue->total_operations++;
    
    // Update peak depth
    if (queue->count > queue->peak_queue_depth) {
        queue->peak_queue_depth = queue->count;
    }
    
    return true;
}

bool deferred_ops_dequeue(deferred_ops_state_t* deferred_state, 
                         deferred_operation_t* op) {
    if (!deferred_state || !op) return false;
    
    deferred_op_queue_t* queue = &deferred_state->queue;
    
    if (queue->count == 0) {
        return false;  // Queue empty
    }
    
    // Find highest priority operation
    uint8_t best_index = queue->head;
    deferred_op_priority_t highest_priority = queue->operations[queue->head].priority;
    
    for (uint8_t i = 1; i < queue->count; i++) {
        uint8_t index = (queue->head + i) % DEFERRED_OP_QUEUE_SIZE;
        if (queue->operations[index].priority > highest_priority) {
            highest_priority = queue->operations[index].priority;
            best_index = index;
        }
    }
    
    // Get the highest priority operation
    *op = queue->operations[best_index];
    
    // Remove it from queue by shifting
    if (best_index != queue->head) {
        // Move operations to fill the gap
        while (best_index != queue->head) {
            uint8_t prev_index = (best_index - 1 + DEFERRED_OP_QUEUE_SIZE) % DEFERRED_OP_QUEUE_SIZE;
            queue->operations[best_index] = queue->operations[prev_index];
            best_index = prev_index;
        }
    }
    
    // Update queue pointers
    queue->head = (queue->head + 1) % DEFERRED_OP_QUEUE_SIZE;
    queue->count--;
    queue->queue_full = false;
    
    return true;
}

bool deferred_ops_has_ready_operations(const deferred_ops_state_t* deferred_state) {
    if (!deferred_state) return false;
    
    const deferred_op_queue_t* queue = &deferred_state->queue;
    
    // Check if any operations have completed their deferral period
    for (uint8_t i = 0; i < queue->count; i++) {
        uint8_t index = (queue->head + i) % DEFERRED_OP_QUEUE_SIZE;
        if (queue->operations[index].remaining_cycles == 0) {
            return true;
        }
    }
    
    return false;
}

void deferred_ops_clear_queue(deferred_ops_state_t* deferred_state) {
    if (!deferred_state) return;
    
    deferred_op_queue_t* queue = &deferred_state->queue;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->queue_full = false;
}

// ===== OPERATION CREATION HELPERS =====

deferred_operation_t deferred_ops_create(deferred_op_type_t type,
                                        deferred_op_priority_t priority,
                                        uint8_t source_reg, uint8_t dest_reg,
                                        uint8_t data, uint16_t address) {
    deferred_operation_t op;
    memset(&op, 0, sizeof(deferred_operation_t));
    
    op.type = type;
    op.priority = priority;
    op.source_reg = source_reg;
    op.dest_reg = dest_reg;
    op.data = data;
    op.address = address;
    
    // Set operation flags based on type
    switch (type) {
        case DEFERRED_OP_FLAG_UPDATE:
            op.updates_flags = true;
            op.defer_cycles = 0;  // Immediate
            break;
            
        case DEFERRED_OP_PC_INCREMENT:
            op.affects_pc = true;
            op.defer_cycles = 0;  // Immediate
            break;
            
        case DEFERRED_OP_ALU_OPERATION:
            op.updates_flags = true;  // Most ALU ops update flags
            op.defer_cycles = 1;
            break;
            
        default:
            op.defer_cycles = 1;  // Default deferral
            break;
    }
    
    op.remaining_cycles = op.defer_cycles;
    return op;
}

deferred_operation_t deferred_ops_register_load(uint8_t dest_reg, uint8_t data) {
    return deferred_ops_create(DEFERRED_OP_REGISTER_LOAD, DEFERRED_PRIORITY_NORMAL,
                              0, dest_reg, data, 0);
}

deferred_operation_t deferred_ops_register_store(uint8_t source_reg, uint16_t address) {
    return deferred_ops_create(DEFERRED_OP_REGISTER_STORE, DEFERRED_PRIORITY_NORMAL,
                              source_reg, 0, 0, address);
}

deferred_operation_t deferred_ops_alu_operation(uint8_t source_reg, uint8_t dest_reg) {
    return deferred_ops_create(DEFERRED_OP_ALU_OPERATION, DEFERRED_PRIORITY_NORMAL,
                              source_reg, dest_reg, 0, 0);
}

deferred_operation_t deferred_ops_flag_update(uint8_t flag_mask, bool set_flags) {
    return deferred_ops_create(DEFERRED_OP_FLAG_UPDATE, DEFERRED_PRIORITY_URGENT,
                              0, 0, flag_mask | (set_flags ? 0x80 : 0), 0);
}

// ===== φ1/φ2 PHASE COORDINATION =====

void deferred_ops_set_phase(deferred_ops_state_t* deferred_state, 
                           bool phi1_active, bool phi2_active) {
    if (!deferred_state) return;
    
    deferred_state->phi1_phase_active = phi1_active;
    deferred_state->phi2_phase_active = phi2_active;
}

void deferred_ops_execute_phi1(deferred_ops_state_t* deferred_state, 
                              struct mos6510_state_s* cpu) {
    if (!deferred_state || !cpu) return;
    if (!deferred_state->phi1_phase_active) return;  // Only execute during φ1
    
    // Execute all ready deferred operations
    deferred_operation_t op;
    while (deferred_ops_has_ready_operations(deferred_state) && 
           deferred_ops_dequeue(deferred_state, &op)) {
        
        // Check RDY line for read operations
        if (op.is_read_cycle && deferred_ops_process_rdy_stall(deferred_state, &op)) {
            // Re-queue operation if stalled by RDY
            deferred_ops_enqueue(deferred_state, &op);
            continue;
        }
        
        // Execute the operation
        switch (op.type) {
            case DEFERRED_OP_REGISTER_LOAD:
                if (op.dest_reg < 16) {
                    cpu->registers[op.dest_reg] = op.data;
                }
                break;
                
            case DEFERRED_OP_REGISTER_STORE:
                if (op.source_reg < 16) {
                    // Store to data latch for memory write
                    cpu->registers[REG_DL] = cpu->registers[op.source_reg];
                }
                break;
                
            case DEFERRED_OP_ALU_OPERATION:
                // ALU operations would be handled here
                // This would integrate with ALU system
                break;
                
            case DEFERRED_OP_FLAG_UPDATE:
                // Update processor status flags
                if (op.data & 0x80) {  // Set flags
                    cpu->registers[REG_P] |= (op.data & 0x7F);
                } else {  // Clear flags
                    cpu->registers[REG_P] &= ~(op.data & 0x7F);
                }
                break;
                
            case DEFERRED_OP_PC_INCREMENT:
                cpu->registers[REG_PCL]++;
                if (cpu->registers[REG_PCL] == 0) {
                    cpu->registers[REG_PCH]++;
                }
                break;
                
            default:
                // Other operations handled as needed
                break;
        }
        
        deferred_state->phi1_operations_executed++;
    }
    
    // Update remaining cycles for queued operations
    deferred_op_queue_t* queue = &deferred_state->queue;
    for (uint8_t i = 0; i < queue->count; i++) {
        uint8_t index = (queue->head + i) % DEFERRED_OP_QUEUE_SIZE;
        if (queue->operations[index].remaining_cycles > 0) {
            queue->operations[index].remaining_cycles--;
        }
    }
}

void deferred_ops_address_setup_phi2(deferred_ops_state_t* deferred_state, 
                                     struct mos6510_state_s* cpu) {
    if (!deferred_state || !cpu) return;
    if (!deferred_state->phi2_phase_active) return;  // Only execute during φ2
    if (!deferred_state->address_setup_enabled) return;
    
    // Execute pending address setup - MUST be last operation per spec
    if (deferred_state->address_setup_pending) {
        deferred_ops_execute_address_setup(deferred_state, cpu);
    }
}

// ===== RDY LINE HANDLING (SPEC LINES 552-554) =====

void deferred_ops_set_rdy_line(deferred_ops_state_t* deferred_state, bool rdy_active) {
    if (!deferred_state) return;
    
    deferred_state->rdy_line_state = rdy_active;
}

bool deferred_ops_should_stall_for_rdy(const deferred_ops_state_t* deferred_state) {
    if (!deferred_state) return false;
    
    // RDY line only affects read cycles, not write cycles
    return !deferred_state->rdy_line_state && deferred_state->rdy_affects_cycle;
}

bool deferred_ops_process_rdy_stall(deferred_ops_state_t* deferred_state, 
                                   const deferred_operation_t* op) {
    if (!deferred_state || !op) return false;
    
    // RDY only affects read operations
    if (!op->is_read_cycle) return false;
    
    // Check if RDY line is holding us
    if (!deferred_state->rdy_line_state) {
        deferred_state->waiting_for_rdy = true;
        deferred_state->rdy_stall_cycles++;
        return true;  // Stalled
    }
    
    deferred_state->waiting_for_rdy = false;
    return false;  // Not stalled
}

// ===== ADDRESS SETUP COORDINATION =====

void deferred_ops_defer_address_setup(deferred_ops_state_t* deferred_state, 
                                      uint16_t address) {
    if (!deferred_state) return;
    
    deferred_state->deferred_address = address;
    deferred_state->address_setup_pending = true;
    deferred_state->address_setup_deferred = true;
}

bool deferred_ops_address_setup_pending(const deferred_ops_state_t* deferred_state) {
    return deferred_state && deferred_state->address_setup_pending;
}

void deferred_ops_execute_address_setup(deferred_ops_state_t* deferred_state, 
                                       struct mos6510_state_s* cpu) {
    if (!deferred_state || !cpu) return;
    if (!deferred_state->address_setup_pending) return;
    
    // Set up address bus - this MUST be the last operation
    cpu->registers[REG_ABL] = deferred_state->deferred_address & 0xFF;
    cpu->registers[REG_ABH] = (deferred_state->deferred_address >> 8) & 0xFF;
    
    // Clear pending state
    deferred_state->address_setup_pending = false;
    deferred_state->address_setup_deferred = false;
    deferred_state->phi2_address_setups++;
}

// ===== VALIDATION AND DEBUGGING =====

bool deferred_ops_validate(const deferred_ops_state_t* deferred_state) {
    if (!deferred_state) return false;
    
    const deferred_op_queue_t* queue = &deferred_state->queue;
    
    // Validate queue consistency
    if (queue->count > DEFERRED_OP_QUEUE_SIZE) return false;
    if (queue->head >= DEFERRED_OP_QUEUE_SIZE) return false;
    if (queue->tail >= DEFERRED_OP_QUEUE_SIZE) return false;
    
    // Validate phase state consistency
    if (deferred_state->phi1_phase_active && deferred_state->phi2_phase_active) {
        return false;  // Both phases cannot be active simultaneously
    }
    
    return true;
}

void deferred_ops_get_queue_status(const deferred_ops_state_t* deferred_state,
                                  uint8_t* current_count, uint8_t* peak_depth,
                                  uint32_t* total_ops, uint32_t* overflows) {
    if (!deferred_state) return;
    
    const deferred_op_queue_t* queue = &deferred_state->queue;
    
    if (current_count) *current_count = queue->count;
    if (peak_depth) *peak_depth = queue->peak_queue_depth;
    if (total_ops) *total_ops = queue->total_operations;
    if (overflows) *overflows = queue->queue_overflows;
}

void deferred_ops_get_statistics(const deferred_ops_state_t* deferred_state,
                                uint32_t* phi1_ops, uint32_t* phi2_setups,
                                uint32_t* rdy_stalls, uint32_t* total_cycles) {
    if (!deferred_state) return;
    
    if (phi1_ops) *phi1_ops = deferred_state->phi1_operations_executed;
    if (phi2_setups) *phi2_setups = deferred_state->phi2_address_setups;
    if (rdy_stalls) *rdy_stalls = deferred_state->rdy_stall_cycles;
    if (total_cycles) *total_cycles = deferred_state->current_cycle;
}

const char* deferred_ops_type_name(deferred_op_type_t type) {
    switch (type) {
        case DEFERRED_OP_NONE:            return "NONE";
        case DEFERRED_OP_REGISTER_LOAD:   return "REGISTER_LOAD";
        case DEFERRED_OP_REGISTER_STORE:  return "REGISTER_STORE";
        case DEFERRED_OP_ALU_OPERATION:   return "ALU_OPERATION";
        case DEFERRED_OP_FLAG_UPDATE:     return "FLAG_UPDATE";
        case DEFERRED_OP_BUS_TRANSFER:    return "BUS_TRANSFER";
        case DEFERRED_OP_PC_INCREMENT:    return "PC_INCREMENT";
        case DEFERRED_OP_STACK_PUSH:      return "STACK_PUSH";
        case DEFERRED_OP_STACK_PULL:      return "STACK_PULL";
        case DEFERRED_OP_ADDRESS_CALC:    return "ADDRESS_CALC";
        default:                          return "UNKNOWN";
    }
}

const char* deferred_ops_priority_name(deferred_op_priority_t priority) {
    switch (priority) {
        case DEFERRED_PRIORITY_LOW:    return "LOW";
        case DEFERRED_PRIORITY_NORMAL: return "NORMAL";
        case DEFERRED_PRIORITY_HIGH:   return "HIGH";
        case DEFERRED_PRIORITY_URGENT: return "URGENT";
        default:                       return "UNKNOWN";
    }
}

void deferred_ops_dump(const deferred_ops_state_t* deferred_state, 
                      char* buffer, size_t buffer_size) {
    if (!deferred_state || !buffer || buffer_size == 0) return;
    
    const deferred_op_queue_t* queue = &deferred_state->queue;
    
    snprintf(buffer, buffer_size,
        "Deferred Operations State:\n"
        "  Queue: %u/%u ops (Peak: %u, Overflows: %u)\n"
        "  Phase: φ1=%s φ2=%s\n"
        "  Address Setup: Pending=%s Deferred=%s Address=0x%04X\n"
        "  RDY: State=%s Waiting=%s Stalls=%u\n"
        "  Statistics: φ1_ops=%u φ2_setups=%u Cycles=%u\n"
        "  Operations in Queue:",
        queue->count, DEFERRED_OP_QUEUE_SIZE, queue->peak_queue_depth, queue->queue_overflows,
        deferred_state->phi1_phase_active ? "ON" : "OFF",
        deferred_state->phi2_phase_active ? "ON" : "OFF",
        deferred_state->address_setup_pending ? "YES" : "NO",
        deferred_state->address_setup_deferred ? "YES" : "NO",
        deferred_state->deferred_address,
        deferred_state->rdy_line_state ? "ACTIVE" : "INACTIVE",
        deferred_state->waiting_for_rdy ? "YES" : "NO",
        deferred_state->rdy_stall_cycles,
        deferred_state->phi1_operations_executed,
        deferred_state->phi2_address_setups,
        deferred_state->current_cycle);
        
    // Add individual operations if there's space
    size_t used = strlen(buffer);
    if (used < buffer_size - 100) {  // Leave room for operations
        for (uint8_t i = 0; i < queue->count && used < buffer_size - 50; i++) {
            uint8_t index = (queue->head + i) % DEFERRED_OP_QUEUE_SIZE;
            const deferred_operation_t* op = &queue->operations[index];
            
            int written = snprintf(buffer + used, buffer_size - used,
                "\n    [%u] %s (%s) cycles=%u src=%u dst=%u data=0x%02X addr=0x%04X",
                i, deferred_ops_type_name(op->type), deferred_ops_priority_name(op->priority),
                op->remaining_cycles, op->source_reg, op->dest_reg, op->data, op->address);
                
            if (written > 0) used += written;
        }
    }
}