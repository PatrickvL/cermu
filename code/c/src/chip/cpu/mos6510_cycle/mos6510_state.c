#include "mos6510_state.h"
#define mos6510_create mos6510_cycle_create
#define mos6510_destroy mos6510_cycle_destroy
#define mos6510_init    mos6510_cycle_init
#define mos6510_reset   mos6510_cycle_reset
#include "mos6510_registers.h" 
#include "register_access.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ===== CPU STATE MANAGEMENT =====

// Create and initialize a new CPU state
mos6510_state_t* mos6510_create(const cpu_config_t *config) {
    if (!config) return NULL;
    
    mos6510_state_t *cpu = malloc(sizeof(mos6510_state_t));
    if (!cpu) return NULL;
    
    if (!mos6510_init(cpu, config)) {
        free(cpu);
        return NULL;
    }
    
    return cpu;
}

// Destroy CPU state and free memory
void mos6510_destroy(mos6510_state_t *cpu) {
    if (cpu) {
        free(cpu);
    }
}

// Initialize CPU state structure
bool mos6510_init(mos6510_state_t *cpu, const cpu_config_t *config) {
    if (!cpu || !config) return false;
    
    // Validate configuration
    if (!cpu_config_validate(config)) return false;
    
    // Clear entire structure
    memset(cpu, 0, sizeof(mos6510_state_t));
    
    // Copy configuration
    cpu->config = *config;
    
    // Initialize register array to reset values
    mos6510_registers_reset(cpu);
    
    // Initialize timing and execution state
    cpu->timing_state = 0;              // Start in T0 state
    cpu->instruction_register = 0x00;   // No instruction loaded
    cpu->predecode_register = 0x00;     // No predecode
    cpu->cycle_position = 0;            // Start of instruction
    
    // Initialize interrupt handling state
    cpu->interrupt_state = 0;           // No interrupts pending
    cpu->nmi_edge_detected = false;     // No NMI edge
    
    // Initialize deferred operation state
    cpu->deferred_data_operation = 0;   // No deferred operation
    cpu->deferred_address_mode = 0;     // No deferred address mode
    cpu->page_cross_detected = 0;       // No page crossing
    
    // Initialize pipeline and timing control
    cpu->pipeline_state = 0;            // Pipeline empty
    cpu->branch_taken = 0;              // No branch taken
    cpu->rdy_halt_cycles = 0;           // No RDY halt
    
    // Initialize internal bus state
    // Initialize internal bus system
    internal_bus_init(&cpu->internal_bus);
    
    // Initialize debug state
    cpu->total_cycles = 0;              // Zero cycle count
    cpu->last_pc = 0;                   // No last PC
    cpu->last_opcode = 0;               // No last opcode
    cpu->debug_enabled = false;         // Debug disabled by default
    
    return true;
}

// Reset CPU to initial state
void mos6510_reset(mos6510_state_t *cpu) {
    if (!cpu) return;
    
    // Preserve configuration but reset everything else
    cpu_config_t saved_config = cpu->config;
    bool saved_debug = cpu->debug_enabled;
    uint64_t saved_cycles = cpu->total_cycles;
    
    // Reinitialize with saved config
    mos6510_init(cpu, &saved_config);
    
    // Restore some state if desired
    cpu->debug_enabled = saved_debug;
    cpu->total_cycles = saved_cycles;
    
    // Set specific reset state
    SET_CPU_PC(cpu, cpu->config.reset_vector);  // Load PC from reset vector
    SET_CPU_FLAG_I(cpu);                        // Set interrupt disable flag
    SET_CPU_SP(cpu, 0xFD);                      // Stack pointer reset value
}

// ===== STATE VALIDATION =====

// Validate CPU state integrity
bool mos6510_validate_state(const mos6510_state_t *cpu) {
    if (!cpu) return false;
    
    // Validate configuration
    if (!cpu_config_validate(&cpu->config)) return false;
    
    // Validate register array
    if (!mos6510_registers_validate(cpu)) return false;
    
    // Validate timing state (should be reasonable)
    if (cpu->timing_state > 15) return false;  // Max timing states
    
    // Validate cycle position (should be reasonable)
    if (cpu->cycle_position > 8) return false;  // Max 8 cycles per instruction
    
    // Validate interrupt state
    if (cpu->interrupt_state > 15) return false;  // 4-bit interrupt state
    
    // Validate stack pointer (implicitly valid as uint8_t)
    // No additional validation needed
    
    return true;
}

// Generate human-readable state dump
void mos6510_state_dump(const mos6510_state_t *cpu, char *buffer, size_t buffer_size) {
    if (!cpu || !buffer || buffer_size == 0) return;
    
    size_t pos = 0;
    
    // CPU configuration info
    pos += snprintf(buffer + pos, buffer_size - pos,
                   "CPU: %s (variant=%d)\n",
                   cpu_config_get_variant_name(cpu->config.cpu_variant),
                   cpu->config.cpu_variant);
    
    if (pos >= buffer_size) return;
    
    // Execution state
    pos += snprintf(buffer + pos, buffer_size - pos,
                   "State: timing=%02X IR=%02X pos=%d cycles=%llu\n",
                   cpu->timing_state, cpu->instruction_register, 
                   cpu->cycle_position, (unsigned long long)cpu->total_cycles);
    
    if (pos >= buffer_size) return;
    
    // Register dump
    mos6510_registers_dump(cpu, buffer + pos, buffer_size - pos);
}

// ===== STATE SERIALIZATION =====

// Serialize CPU state to binary buffer
size_t mos6510_state_serialize(const mos6510_state_t *cpu, uint8_t *buffer, size_t buffer_size) {
    if (!cpu || !buffer) return 0;
    
    size_t required_size = sizeof(mos6510_state_t);
    if (buffer_size < required_size) return required_size;
    
    // Simple binary copy (assumes same architecture/endianness)
    memcpy(buffer, cpu, sizeof(mos6510_state_t));
    return sizeof(mos6510_state_t);
}

// Deserialize CPU state from binary buffer
bool mos6510_state_deserialize(mos6510_state_t *cpu, const uint8_t *buffer, size_t buffer_size) {
    if (!cpu || !buffer || buffer_size < sizeof(mos6510_state_t)) return false;
    
    // Simple binary copy (assumes same architecture/endianness)
    memcpy(cpu, buffer, sizeof(mos6510_state_t));
    
    // Validate deserialized state
    return mos6510_validate_state(cpu);
}

// ===== UTILITY FUNCTIONS =====

// Get current program counter value
uint16_t mos6510_get_pc(const mos6510_state_t *cpu) {
    if (!cpu) return 0;
    return CPU_PC(cpu);
}

// Set program counter value  
void mos6510_set_pc(mos6510_state_t *cpu, uint16_t pc) {
    if (!cpu) return;
    SET_CPU_PC(cpu, pc);
}

// Get current accumulator value
uint8_t mos6510_get_a(const mos6510_state_t *cpu) {
    if (!cpu) return 0;
    return CPU_A(cpu);
}

// Set accumulator value
void mos6510_set_a(mos6510_state_t *cpu, uint8_t value) {
    if (!cpu) return;
    SET_CPU_A(cpu, value);
}

// Get current processor status
uint8_t mos6510_get_status(const mos6510_state_t *cpu) {
    if (!cpu) return 0;
    return CPU_P(cpu);
}

// Set processor status
void mos6510_set_status(mos6510_state_t *cpu, uint8_t status) {
    if (!cpu) return;
    SET_CPU_P(cpu, status | FLAG_UNUSED);  // Ensure unused flag stays set
}

// Check if CPU is in debug mode
bool mos6510_is_debug_enabled(const mos6510_state_t *cpu) {
    if (!cpu) return false;
    return cpu->debug_enabled;
}

// Enable/disable debug mode
void mos6510_set_debug_enabled(mos6510_state_t *cpu, bool enabled) {
    if (!cpu) return;
    cpu->debug_enabled = enabled;
}

// Get total cycle count
uint64_t mos6510_get_cycle_count(const mos6510_state_t *cpu) {
    if (!cpu) return 0;
    return cpu->total_cycles;
}

// Reset cycle counter
void mos6510_reset_cycle_count(mos6510_state_t *cpu) {
    if (!cpu) return;
    cpu->total_cycles = 0;
}