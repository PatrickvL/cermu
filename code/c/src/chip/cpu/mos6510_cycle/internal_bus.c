#include "internal_bus.h"
#include "mos6510_state.h"
#include "register_access.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// ===== BUS INITIALIZATION AND MANAGEMENT =====

// Initialize internal bus system to hardware reset state
void internal_bus_init(internal_bus_state_t *bus) {
    if (!bus) return;
    
    // Initialize all bus values to precharge state (0xFF)
    bus->sb_value = 0xFF;
    bus->adl_value = 0xFF;
    bus->adh_value = 0xFF;
    bus->abl_value = 0xFF;
    bus->abh_value = 0xFF;
    
    // Initialize control state
    bus->transfer_control = 0x00;
    bus->phi1_phase = false;        // Start in φ2
    bus->precharge_active = true;   // φ2 precharge active
    
    // Initialize driver conflict detection
    bus->sb_drivers = 0;
    bus->adl_drivers = 0;
    bus->adh_drivers = 0;
    
    // Initialize transfer queue
    bus->pending_transfers = 0x00;
}

// Reset all buses to precharge state (φ2 behavior)
void internal_bus_reset(internal_bus_state_t *bus) {
    if (!bus) return;
    
    // All internal buses precharged high during φ2
    bus->sb_value = 0xFF;
    bus->adl_value = 0xFF;
    bus->adh_value = 0xFF;
    
    // External latches maintain their values (not precharged)
    // bus->abl_value and bus->abh_value unchanged
    
    bus->precharge_active = true;
    bus->transfer_control = 0x00;
}

// Validate bus state for conflicts and consistency
bool internal_bus_validate(const internal_bus_state_t *bus) {
    if (!bus) return false;
    
    // Check for driver conflicts
    if (!internal_bus_check_conflicts(bus)) {
        return false;
    }
    
    // Validate phase consistency
    if (bus->phi1_phase && bus->precharge_active) {
        return false; // Cannot have precharge active during φ1
    }
    
    return true;
}

// ===== CLOCK PHASE OPERATIONS =====

// Set current clock phase and update bus behavior accordingly
void internal_bus_set_phase(internal_bus_state_t *bus, bool phi1_phase) {
    if (!bus) return;
    
    bus->phi1_phase = phi1_phase;
    
    if (phi1_phase) {
        // φ1 phase: execute transfers, disable precharge
        bus->precharge_active = false;
    } else {
        // φ2 phase: enable precharge
        bus->precharge_active = true;
    }
}

// Execute φ2 phase: precharge all buses, latch transfer control
void internal_bus_phi2_precharge(internal_bus_state_t *bus, uint8_t transfer_control) {
    if (!bus) return;
    
    // Set φ2 phase
    internal_bus_set_phase(bus, false);
    
    // Precharge all internal buses high
    internal_bus_reset(bus);
    
    // Latch transfer control for next φ1 execution
    bus->pending_transfers = transfer_control;
    
    // Clear current transfer control
    bus->transfer_control = 0x00;
    
    // Reset driver counters
    bus->sb_drivers = 0;
    bus->adl_drivers = 0;
    bus->adh_drivers = 0;
}

// Execute φ1 phase: execute pending transfers, update bus values
void internal_bus_phi1_execute(internal_bus_state_t *bus, mos6510_state_t *cpu) {
    if (!bus || !cpu) return;
    
    // Set φ1 phase
    internal_bus_set_phase(bus, true);
    
    // Move pending transfers to active
    bus->transfer_control = bus->pending_transfers;
    bus->pending_transfers = 0x00;
    
    // Execute all active transfers
    for (uint8_t bit = 0; bit < 8; bit++) {
        uint8_t control_bit = 1 << bit;
        if (bus->transfer_control & control_bit) {
            internal_bus_execute_transfer(bus, cpu, control_bit);
        }
    }
    
    // Check for conflicts and resolve if necessary
    if (!internal_bus_check_conflicts(bus)) {
        internal_bus_resolve_conflicts(bus);
    }
}

// ===== BUS TRANSFER OPERATIONS =====

// Queue bus transfers for execution during next φ1 phase
void internal_bus_queue_transfers(internal_bus_state_t *bus, uint8_t transfer_control) {
    if (!bus) return;
    
    // Only queue transfers during φ2
    if (bus->phi1_phase) {
        // Execute immediately if in φ1
        bus->transfer_control |= transfer_control;
    } else {
        // Queue for next φ1 if in φ2
        bus->pending_transfers |= transfer_control;
    }
}

// Execute a specific bus transfer (called during φ1)
void internal_bus_execute_transfer(internal_bus_state_t *bus, mos6510_state_t *cpu, uint8_t control_bit) {
    if (!bus || !cpu) return;
    
    switch (control_bit) {
        case BUS_DL_ADL:
            // DL → ADL (operand to address low)
            bus->adl_value = CPU_DL(cpu);
            bus->adl_drivers++;
            break;
            
        case BUS_DL_ADH:
            // DL → ADH (operand to address high)
            bus->adh_value = CPU_DL(cpu);
            bus->adh_drivers++;
            break;
            
        case BUS_PCL_ADL:
            // PCL → ADL (PC low to address)
            bus->adl_value = CPU_PCL(cpu);
            bus->adl_drivers++;
            break;
            
        case BUS_PCH_ADH:
            // PCH → ADH (PC high to address)
            bus->adh_value = CPU_PCH(cpu);
            bus->adh_drivers++;
            break;
            
        case BUS_S_ADL:
            // S → ADL (stack pointer to address)
            bus->adl_value = CPU_SP(cpu);
            bus->adl_drivers++;
            break;
            
        case BUS_ZERO_ADH:
            // 0 → ADH (zero page/stack addressing)
            bus->adh_value = 0x00;
            bus->adh_drivers++;
            break;
            
        case BUS_SB_ROUTE:
            // SB routing (complex internal routing)
            internal_bus_route_sb(bus, cpu, bus->sb_value);
            bus->sb_drivers++;
            break;
            
        case BUS_ADDR_LATCH:
            // ADL/ADH → ABL/ABH (latch to output)
            internal_bus_latch_address(bus);
            break;
    }
}

// Update address bus latches (ABL/ABH) from internal address buses (ADL/ADH)
void internal_bus_latch_address(internal_bus_state_t *bus) {
    if (!bus) return;
    
    // Latch internal address buses to external latches
    bus->abl_value = bus->adl_value;
    bus->abh_value = bus->adh_value;
}

// ===== BUS ROUTING FUNCTIONS =====

// Route data through Special Bus (SB) - complex internal routing
void internal_bus_route_sb(internal_bus_state_t *bus, mos6510_state_t *cpu, uint8_t data) {
    if (!bus || !cpu) return;
    
    // SB routing is complex and depends on the current operation
    // For now, implement basic data routing through SB
    bus->sb_value = data;
    SET_CPU_SB(cpu, data);
}

// Set up address calculation on ADL/ADH buses
void internal_bus_setup_address(internal_bus_state_t *bus, uint8_t adl_value, uint8_t adh_value) {
    if (!bus) return;
    
    bus->adl_value = adl_value;
    bus->adh_value = adh_value;
}

// Zero the address high bus (for zero page and stack operations)
void internal_bus_zero_adh(internal_bus_state_t *bus) {
    if (!bus) return;
    
    bus->adh_value = 0x00;
    bus->adh_drivers++;
}

// ===== DRIVER CONFLICT PREVENTION =====

// Check for electrical conflicts ("at most one driver" rule)
bool internal_bus_check_conflicts(const internal_bus_state_t *bus) {
    if (!bus) return false;
    
    // Check each bus for multiple drivers
    if (bus->sb_drivers > 1) return false;
    if (bus->adl_drivers > 1) return false;
    if (bus->adh_drivers > 1) return false;
    
    return true;
}

// Count active drivers for a specific bus
uint8_t internal_bus_count_drivers(uint8_t transfer_control, uint8_t bus_mask) {
    uint8_t count = 0;
    
    // Count how many control bits affect this bus
    for (uint8_t bit = 0; bit < 8; bit++) {
        if ((transfer_control & (1 << bit)) && (bus_mask & (1 << bit))) {
            count++;
        }
    }
    
    return count;
}

// Resolve bus conflicts (log warning, use first driver)
void internal_bus_resolve_conflicts(internal_bus_state_t *bus) {
    if (!bus) return;
    
    // Reset driver counters - conflicts resolved
    bus->sb_drivers = (bus->sb_drivers > 0) ? 1 : 0;
    bus->adl_drivers = (bus->adl_drivers > 0) ? 1 : 0;
    bus->adh_drivers = (bus->adh_drivers > 0) ? 1 : 0;
    
    // In a real implementation, this would log a warning
    // For now, silently resolve by keeping the current values
}

// ===== DEBUGGING AND INSPECTION =====

// Generate human-readable bus state dump
void internal_bus_dump(const internal_bus_state_t *bus, char *buffer, size_t buffer_size) {
    if (!bus || !buffer || buffer_size == 0) return;
    
    size_t pos = 0;
    
    // Clock phase and control state
    pos += snprintf(buffer + pos, buffer_size - pos,
                   "Bus State: φ%d precharge=%s control=0x%02X pending=0x%02X\n",
                   bus->phi1_phase ? 1 : 2, 
                   bus->precharge_active ? "ON" : "OFF",
                   bus->transfer_control, 
                   bus->pending_transfers);
    
    if (pos >= buffer_size) return;
    
    // Internal bus values
    pos += snprintf(buffer + pos, buffer_size - pos,
                   "Internal: SB=0x%02X ADL=0x%02X ADH=0x%02X (addr=0x%04X)\n",
                   bus->sb_value, bus->adl_value, bus->adh_value,
                   internal_bus_get_address(bus));
    
    if (pos >= buffer_size) return;
    
    // External latch values
    pos += snprintf(buffer + pos, buffer_size - pos,
                   "External: ABL=0x%02X ABH=0x%02X (addr=0x%04X)\n",
                   bus->abl_value, bus->abh_value,
                   internal_bus_get_external_address(bus));
    
    if (pos >= buffer_size) return;
    
    // Driver conflict information
    pos += snprintf(buffer + pos, buffer_size - pos,
                   "Drivers: SB=%d ADL=%d ADH=%d conflicts=%s\n",
                   bus->sb_drivers, bus->adl_drivers, bus->adh_drivers,
                   internal_bus_check_conflicts(bus) ? "NONE" : "DETECTED");
}

// Get transfer control bit names for debugging
const char* internal_bus_get_control_name(uint8_t control_bit) {
    switch (control_bit) {
        case BUS_DL_ADL:     return "DL→ADL";
        case BUS_DL_ADH:     return "DL→ADH";
        case BUS_PCL_ADL:    return "PCL→ADL";
        case BUS_PCH_ADH:    return "PCH→ADH";
        case BUS_S_ADL:      return "S→ADL";
        case BUS_ZERO_ADH:   return "0→ADH";
        case BUS_SB_ROUTE:   return "SB_ROUTE";
        case BUS_ADDR_LATCH: return "ADL/ADH→ABL/ABH";
        default:             return "UNKNOWN";
    }
}

// Get bus routing description for debugging
void internal_bus_get_routing_description(uint8_t transfer_control, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return;
    
    buffer[0] = '\0';
    bool first = true;
    
    for (uint8_t bit = 0; bit < 8; bit++) {
        uint8_t control_bit = 1 << bit;
        if (transfer_control & control_bit) {
            if (!first) {
                strncat(buffer, " | ", buffer_size - strlen(buffer) - 1);
            }
            strncat(buffer, internal_bus_get_control_name(control_bit), 
                   buffer_size - strlen(buffer) - 1);
            first = false;
        }
    }
    
    if (first) {
        strncpy(buffer, "NO_TRANSFERS", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
}