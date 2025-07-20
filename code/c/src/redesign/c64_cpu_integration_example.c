#include "c64_bus_optimized.h"

// ============================================================================
// CPU OPCODE HANDLERS - NOSTRADAMUS DISTRIBUTOR PATTERN
// ============================================================================

// ============================================================================
// GENERIC BUS INTERFACE FOR CPU INDEPENDENCE
// ============================================================================

// Generic bus controller interface that any system can implement
typedef struct {
    // Generic bus cycle functions that any system must provide
    REGISTER_CALL generic_bus_state_t (*read_cycle)(void* context, generic_bus_state_t bus_state);
    REGISTER_CALL generic_bus_state_t (*write_cycle)(void* context, generic_bus_state_t bus_state);
    REGISTER_CALL generic_bus_state_t (*non_cpu_cycle)(void* context, generic_bus_state_t bus_state);
    void* context;  // System-specific context (c64_t*, c128_t*, etc.)
} generic_bus_controller_t;

// CPU state structure - completely system-independent
typedef struct {
    uint8_t A, X, Y, SP, P;  // Registers
    uint16_t PC;             // Program counter
    generic_bus_controller_t* bus;  // Generic bus interface
} cpu_state_t;

// Handler function pointer type with register calling convention
typedef REGISTER_CALL void* (*PFNDUOP)(cpu_state_t* cpu, generic_bus_state_t* bus_state);

// Special handler for flow control/exit
REGISTER_CALL void* cpu_fire_escape(cpu_state_t* cpu, generic_bus_state_t* bus_state) {
    return cpu_fire_escape;  // Return itself to maintain handler invariant
}

// ============================================================================
// EXAMPLE CPU OPCODE HANDLERS
// ============================================================================

// LDA Absolute - $AD
REGISTER_CALL void* cpu_lda_abs(cpu_state_t* cpu, generic_bus_state_t* bus) {
    // Fetch low byte of address (triggers system tick)
    bus->addr = cpu->PC++;
    bus->lines |= GENERIC_RW_LINE;  // Read operation
    *bus = cpu->bus->read_cycle(cpu->bus->context, *bus);
    uint8_t addr_lo = bus->data;
    
    // Fetch high byte of address (triggers system tick)  
    bus->addr = cpu->PC++;
    *bus = cpu->bus->read_cycle(cpu->bus->context, *bus);
    uint8_t addr_hi = bus->data;
    
    // Calculate absolute address
    uint16_t addr = addr_lo | (addr_hi << 8);
    
    // Read data from address (triggers system tick)
    bus->addr = addr;
    *bus = cpu->bus->read_cycle(cpu->bus->context, *bus);
    cpu->A = bus->data;
    
    // Update flags (N and Z)
    cpu->P = (cpu->P & ~0x82) | (cpu->A & 0x80) | (cpu->A ? 0 : 0x02);
    
    // Return next handler instead of calling it (eliminates stack growth)
    return get_next_handler(cpu, bus);
}

// STA Absolute - $8D  
REGISTER_CALL void* cpu_sta_abs(cpu_state_t* cpu, generic_bus_state_t* bus) {
    // Fetch address (2 system ticks)
    bus->addr = cpu->PC++;
    bus->lines |= GENERIC_RW_LINE;  // Read operation for address fetch
    *bus = cpu->bus->read_cycle(cpu->bus->context, *bus);
    uint8_t addr_lo = bus->data;
    
    bus->addr = cpu->PC++;
    *bus = cpu->bus->read_cycle(cpu->bus->context, *bus);
    uint8_t addr_hi = bus->data;
    
    uint16_t addr = addr_lo | (addr_hi << 8);
    
    // Write A register to address (1 system tick)
    bus->addr = addr;
    bus->data = cpu->A;
    bus->lines &= ~GENERIC_RW_LINE;  // Write operation (R/W = 0)
    *bus = cpu->bus->write_cycle(cpu->bus->context, *bus);
    
    return get_next_handler(cpu, bus);
}

// INX - $E8
REGISTER_CALL void* cpu_inx(cpu_state_t* cpu, generic_bus_state_t* bus) {
    // Internal operation - still need to advance system one cycle
    // Internal cycle - CPU doesn't use bus but chips still advance
    *bus = cpu->bus->non_cpu_cycle(cpu->bus->context, *bus);
    
    cpu->X++;
    
    // Update N and Z flags
    cpu->P = (cpu->P & ~0x82) | (cpu->X & 0x80) | (cpu->X ? 0 : 0x02);
    
    return get_next_handler(cpu, bus);
}

// NOP - $EA
REGISTER_CALL void* cpu_nop(cpu_state_t* cpu, generic_bus_state_t* bus) {
    // Internal operation - advance system one cycle
    // Internal cycle - CPU doesn't use bus but chips still advance
    *bus = cpu->bus->non_cpu_cycle(cpu->bus->context, *bus);
    
    return get_next_handler(cpu, bus);
}

// BRK - $00 (example of flow control)
REGISTER_CALL void* cpu_brk(cpu_state_t* cpu, generic_bus_state_t* bus) {
    // Internal cycle for BRK instruction decode
    *bus = cpu->bus->non_cpu_cycle(cpu->bus->context, *bus);
    
    // Push PC+2 to stack (high byte first)
    bus->addr = 0x0100 + cpu->SP--;
    bus->data = (cpu->PC + 1) >> 8;
    bus->lines &= ~GENERIC_RW_LINE;  // Write operation (R/W = 0)
    *bus = cpu->bus->write_cycle(cpu->bus->context, *bus);
    
    bus->addr = 0x0100 + cpu->SP--;
    bus->data = (cpu->PC + 1) & 0xFF;
    *bus = cpu->bus->write_cycle(cpu->bus->context, *bus);
    
    // Push status register with B flag set
    bus->addr = 0x0100 + cpu->SP--;
    bus->data = cpu->P | 0x10;  // Set B flag
    *bus = cpu->bus->write_cycle(cpu->bus->context, *bus);
    
    // Set interrupt disable flag
    cpu->P |= 0x04;
    
    // Load interrupt vector from $FFFE/$FFFF
    bus->addr = 0xFFFE;
    bus->lines |= GENERIC_RW_LINE;  // Read operation
    *bus = cpu->bus->read_cycle(cpu->bus->context, *bus);
    uint8_t vec_lo = bus->data;
    
    bus->addr = 0xFFFF;
    *bus = cpu->bus->read_cycle(cpu->bus->context, *bus);
    uint8_t vec_hi = bus->data;
    
    cpu->PC = vec_lo | (vec_hi << 8);
    
    return get_next_handler(cpu, bus);
}

// IRQ Handler
REGISTER_CALL void* cpu_handle_irq(cpu_state_t* cpu, generic_bus_state_t* bus) {
    // IRQ is only serviced if I flag is clear
    if (cpu->P & 0x04) {
        return get_next_handler(cpu, bus);  // IRQ disabled, continue normal execution
    }
    
    // Internal cycle for interrupt detection
    *bus = cpu->bus->non_cpu_cycle(cpu->bus->context, *bus);
    
    // Push PC to stack (high byte first)
    bus->addr = 0x0100 + cpu->SP--;
    bus->data = cpu->PC >> 8;
    bus->lines &= ~GENERIC_RW_LINE;  // Write operation (R/W = 0)
    *bus = cpu->bus->write_cycle(cpu->bus->context, *bus);
    
    bus->addr = 0x0100 + cpu->SP--;
    bus->data = cpu->PC & 0xFF;
    *bus = cpu->bus->write_cycle(cpu->bus->context, *bus);
    
    // Push status register without B flag
    bus->addr = 0x0100 + cpu->SP--;
    bus->data = cpu->P & ~0x10;  // Clear B flag for IRQ
    *bus = cpu->bus->write_cycle(cpu->bus->context, *bus);
    
    // Set interrupt disable flag
    cpu->P |= 0x04;
    
    // Load IRQ vector from $FFFE/$FFFF
    bus->addr = 0xFFFE;
    bus->lines |= GENERIC_RW_LINE;  // Read operation
    *bus = cpu->bus->read_cycle(cpu->bus->context, *bus);
    uint8_t vec_lo = bus->data;
    
    bus->addr = 0xFFFF;
    *bus = cpu->bus->read_cycle(cpu->bus->context, *bus);
    uint8_t vec_hi = bus->data;
    
    cpu->PC = vec_lo | (vec_hi << 8);
    
    return get_next_handler(cpu, bus);
}

// ============================================================================
// C64 BUS ADAPTER FUNCTIONS - ADAPT C64 BUS TO GENERIC INTERFACE
// ============================================================================

// Wrapper functions that adapt C64 bus functions to generic interface
static REGISTER_CALL generic_bus_state_t c64_generic_read_cycle(void* context, generic_bus_state_t bus_state) {
    c64_t* c64 = (c64_t*)context;
    c64_bus_state_t c64_bus_state = { .bus = bus_state };
    c64_bus_state = c64_bus_read_cycle(c64->bus, c64_bus_state);
    return c64_bus_state.bus;
}

static REGISTER_CALL generic_bus_state_t c64_generic_write_cycle(void* context, generic_bus_state_t bus_state) {
    c64_t* c64 = (c64_t*)context;
    c64_bus_state_t c64_bus_state = { .bus = bus_state };
    c64_bus_state = c64_bus_write_cycle(c64->bus, c64_bus_state);
    return c64_bus_state.bus;
}

static REGISTER_CALL generic_bus_state_t c64_generic_non_cpu_cycle(void* context, generic_bus_state_t bus_state) {
    c64_t* c64 = (c64_t*)context;
    c64_bus_state_t c64_bus_state = { .bus = bus_state };
    c64_bus_state = c64_non_cpu_cycle(c64, c64_bus_state);
    return c64_bus_state.bus;
}

// ============================================================================
// HANDLER TABLE AND DISPATCH SYSTEM
// ============================================================================

// Handler table indexed by opcode
static PFNDUOP handler_table[256] = {
    [0x00] = cpu_brk,      // BRK
    [0x8D] = cpu_sta_abs,  // STA Absolute
    [0xAD] = cpu_lda_abs,  // LDA Absolute
    [0xE8] = cpu_inx,      // INX
    [0xEA] = cpu_nop,      // NOP
    // ... fill in remaining opcodes
};

// Get next handler based on current PC and interrupt state
REGISTER_CALL void* get_next_handler(cpu_state_t* cpu, generic_bus_state_t* bus_state) {
    // Fast combined interrupt check - most cycles have no interrupts
    if (bus_state->lines & (GENERIC_NMI_LINE | GENERIC_IRQ_LINE)) {
        // Only differentiate when interrupts are actually pending
        if (bus_state->lines & GENERIC_NMI_LINE) {
            // NMI has highest priority and cannot be masked
            return cpu_handle_nmi;
        } else if (!(cpu->P & 0x04)) {
            // IRQ only if not masked by I flag
            return cpu_handle_irq;
        }
    }
    
    // Fetch next opcode
    bus_state->addr = cpu->PC++;
    bus_state->lines |= GENERIC_RW_LINE;  // Read operation
    *bus_state = cpu->bus->read_cycle(cpu->bus->context, *bus_state);
    uint8_t opcode = bus_state->data;
    
    return handler_table[opcode];
}

// ============================================================================
// NOSTRADAMUS DISTRIBUTOR EXECUTION ENGINE
// ============================================================================

void cpu_execute_nostradamus(cpu_state_t* cpu, int max_instructions) {
    // Initialize bus state - allocated on stack for all handlers to share
    c64_bus_state_t shared_bus = {0};
    shared_bus.bus.lines = GENERIC_BA_LINE | GENERIC_AEC_LINE | GENERIC_RDY_LINE;  // Default line states
    
    PFNDUOP current = get_next_handler(cpu, &shared_bus.bus);  // Pass generic bus pointer
    
    // Nostradamus Distributor pattern with proper nesting depth
    // All handlers operate on the same shared bus state via pointer
    for (int count = 0; count < max_instructions && current != cpu_fire_escape; count++) {
        current = (PFNDUOP)current(cpu, &shared_bus.bus);
        if (current != cpu_fire_escape) {
            current = (PFNDUOP)current(cpu, &shared_bus.bus);
            if (current != cpu_fire_escape) {
                current = (PFNDUOP)current(cpu, &shared_bus.bus);
                if (current != cpu_fire_escape) {
                    current = (PFNDUOP)current(cpu, &shared_bus.bus);
                    if (current != cpu_fire_escape) {
                        current = (PFNDUOP)current(cpu, &shared_bus.bus);
                        if (current != cpu_fire_escape) {
                            current = (PFNDUOP)current(cpu, &shared_bus.bus);
                            if (current != cpu_fire_escape) {
                                current = (PFNDUOP)current(cpu, &shared_bus.bus);
                                if (current != cpu_fire_escape) {
                                    current = (PFNDUOP)current(cpu, &shared_bus.bus);
                                    // Can continue nesting as needed for longer traces
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Check for breakpoints, debug conditions, etc.
        if (should_break_execution(cpu)) {
            break;
        }
    }
}

// ============================================================================
// EXAMPLE USAGE
// ============================================================================

bool should_break_execution(cpu_state_t* cpu) {
    // Example: break on specific PC or after certain instruction count
    // You can add breakpoints, watchpoints, etc. here
    return false;
}

void example_c64_execution() {
    // Create and initialize C64 system
    c64_t* c64 = calloc(1, sizeof(c64_t));
    c64->bus = c64_bus_create();
    c64_bus_attach_c64(c64->bus, c64);
    
    // Initialize VIC, SID, CIA chips (stub allocation)
    c64->vic = calloc(1, sizeof(vic_state_t));
    c64->sid = calloc(1, sizeof(sid_state_t));
    c64->cia1 = calloc(1, sizeof(cia_state_t));
    c64->cia2 = calloc(1, sizeof(cia_state_t));
    
    // Create generic bus interface for CPU
    generic_bus_controller_t generic_bus = {
        .read_cycle = c64_generic_read_cycle,
        .write_cycle = c64_generic_write_cycle,
        .non_cpu_cycle = c64_generic_non_cpu_cycle,
        .context = c64  // C64 system as context
    };
    
    // Initialize CPU state
    cpu_state_t cpu = {0};
    cpu.bus = &generic_bus;  // CPU uses generic bus interface
    cpu.PC = 0x0400;  // Start at $0400
    cpu.SP = 0xFF;    // Stack starts at $01FF
    cpu.P = 0x20;     // Initial status register (unused bit always set)
    
    // Populate chip select arrays from PLA logic
    c64_bus_populate_chip_select_from_pla(c64->bus);
    
    // Execute CPU using Nostradamus Distributor
    // This eliminates stack growth while maintaining cycle accuracy
    cpu_execute_nostradamus(&cpu, 1000);  // Execute up to 1000 instructions
    
    // Cleanup
    free(c64->vic);
    free(c64->sid);
    free(c64->cia1);
    free(c64->cia2);
    c64_bus_destroy(c64->bus);
    free(c64);
}