#include "c64_bus_optimized.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// OPTIMIZED SYSTEM TICK FUNCTIONS
// ============================================================================

FORCE_INLINE REGISTER_CALL c64_bus_state_t c64_system_tick_read(c64_t* c64, c64_bus_state_t bus_state) {
    // Ultra-fast chip selection with branchless I/O detection
    uint8_t bank = bus_state.bus.addr >> 12;  // 4KB bank (0-15)
    uint8_t encoded = c64->bus->cpu_encoded_chip_per_bank[bank];
    uint8_t chip = (encoded >> 4) & 0x0F;  // Extract read chip (upper 4 bits)
    
    // Branchless I/O sub-page detection (16 pages of $100 bytes each)
    uint8_t is_io = -(base_chip == CHIP_IO);
    chip += is_io & ((bus_state.bus.addr >> 8) & 0xF);
    
    // Initialize floating bus data
    bus_state.bus.data = c64->bus->data;  // Retain last bus value
    
    // All chips advance their internal timing WITH bus state for interrupt handling
    bus_state.bus = vicii_advance_cycle(c64->vicii, bus_state.bus);
    bus_state.bus = sid_advance_cycle(c64->sid, bus_state.bus);
    bus_state.bus = cia_advance_cycle(c64->cia1, bus_state.bus);
    bus_state.bus = cia_advance_cycle(c64->cia2, bus_state.bus);
    
    // Switch dispatch for selected chip - compiler generates jump table
    switch (selected_chip) {
        case CHIP_RAM:
            bus_state.bus.data = c64->ram.memory[bus_state.bus.addr];
            break;
        case CHIP_BASIC:
            bus_state.bus.data = c64->basic_rom[bus_state.bus.addr & 0x1FFF];
            break;
        case CHIP_KERNAL:
            bus_state.bus.data = c64->kernal_rom[bus_state.bus.addr & 0x1FFF];
            break;
        case CHIP_ROML:
            bus_state.bus.data = c64->cartridge.roml[bus_state.bus.addr & 0x1FFF];
            break;
        case CHIP_ROMH:
            bus_state.bus.data = c64->cartridge.romh[bus_state.bus.addr & 0x1FFF];
            break;
        case CHIP_CHARROM:
            bus_state.bus.data = c64->char_rom[bus_state.bus.addr & 0x0FFF];
            break;
        case CHIP_COLORRAM:
            bus_state.bus.data = c64->colorram.memory[bus_state.bus.addr & 0x3FF] | 0xF0; // High nibble always set
            break;
        case CHIP_D0_VIC:
        case CHIP_D1_VIC:
        case CHIP_D2_VIC:
        case CHIP_D3_VIC:
            // All VIC pages map to the same VIC chip
            bus_state.bus = vicii_read(c64->vicii, bus_state.bus);
            break;
        case CHIP_D4_SID:
        case CHIP_D5_SID:
        case CHIP_D6_SID:
        case CHIP_D7_SID:
            // All SID pages map to the same SID chip
            bus_state.bus = sid_read(c64->sid, bus_state.bus);
            break;
        case CHIP_D8_COLORRAM:
            // Color RAM accessed via I/O area ($D800-$D8FF) - handled by VIC
            bus_state.bus.data = c64->colorram.memory[bus_state.bus.addr & 0x3FF] | 0xF0;
            break;
        case CHIP_DC_CIA1:
            bus_state.bus = cia_read(c64->cia1, bus_state.bus);
            break;
        case CHIP_DD_CIA2:
            bus_state.bus = cia_read(c64->cia2, bus_state.bus);
            break;
        case CHIP_DE_IO1:
            bus_state.bus = c64->cartridge.io1_read(c64->cartridge.context, bus_state.bus);
            break;
        case CHIP_DF_IO2:
            bus_state.bus = c64->cartridge.io2_read(c64->cartridge.context, bus_state.bus);
            break;
        case CHIP_D9_UNMAPPED:
        case CHIP_DA_UNMAPPED:
        case CHIP_DB_UNMAPPED:
        case CHIP_UNMAPPED:
        default:
            // bus.data retains floating bus value (no change)
            break;
    }
    
    // Update system bus state and return final bus state
    c64->bus->data = bus_state.bus.data;
    return bus_state;
}

FORCE_INLINE REGISTER_CALL c64_bus_state_t c64_system_tick_write(c64_t* c64, c64_bus_state_t bus) {
    // Ultra-fast chip selection with branchless I/O detection
    uint8_t bank = bus.addr >> 12;
    uint8_t encoded = c64->bus->cpu_encoded_chip_per_bank[bank];
    uint8_t chip = encoded & 0x0F;  // Extract write chip (lower 4 bits)
    
    // Branchless I/O sub-page detection (16 pages of $100 bytes each)
    uint8_t is_io = -(base_chip == CHIP_IO);
    chip += is_io & ((bus.addr >> 8) & 0xF);
    
    // All chips advance their internal timing WITH bus state for interrupt handling
    bus = vicii_advance_cycle(c64->vicii, bus);
    bus = sid_advance_cycle(c64->sid, bus);
    bus = cia_advance_cycle(c64->cia1, bus);
    bus = cia_advance_cycle(c64->cia2, bus);
    
    // Switch dispatch optimized for writes - dead code elimination removes read-only cases
    switch (chip) {
        case CHIP_RAM:
            c64->ram.memory[bus_state.bus.addr] = bus_state.bus.data;
            break;
        case CHIP_COLORRAM:
            c64->colorram.memory[bus_state.bus.addr & 0x3FF] = bus_state.bus.data & 0x0F; // Only low nibble stored
            break;
        case CHIP_BASIC:
        case CHIP_KERNAL:
        case CHIP_CHARROM:
        case CHIP_ROML:
        case CHIP_ROMH:
        case CHIP_UNMAPPED:
        case CHIP_D9_UNMAPPED:
        case CHIP_DA_UNMAPPED:
        case CHIP_DB_UNMAPPED:
        default:
            // Read-only or unmapped - ignore writes
            break;
        case CHIP_D0_VIC:
        case CHIP_D1_VIC:
        case CHIP_D2_VIC:
        case CHIP_D3_VIC:
            // All VIC pages map to the same VIC chip
            bus_state.bus = vicii_write(c64->vicii, bus_state.bus);
            break;
        case CHIP_D4_SID:
        case CHIP_D5_SID:
        case CHIP_D6_SID:
        case CHIP_D7_SID:
            // All SID pages map to the same SID chip
            bus_state.bus = sid_write(c64->sid, bus_state.bus);
            break;
        case CHIP_D8_COLORRAM:
            // Color RAM accessed via I/O area ($D800-$D8FF)
            c64->colorram.memory[bus_state.bus.addr & 0x3FF] = bus_state.bus.data & 0x0F;
            break;
        case CHIP_DC_CIA1:
            bus_state.bus = cia_write(c64->cia1, bus_state.bus);
            break;
        case CHIP_DD_CIA2:
            bus_state.bus = cia_write(c64->cia2, bus_state.bus);
            break;
        case CHIP_DE_IO1:
            bus_state.bus = c64->cartridge.io1_write(c64->cartridge.context, bus_state.bus);
            break;
        case CHIP_DF_IO2:
            bus_state.bus = c64->cartridge.io2_write(c64->cartridge.context, bus_state.bus);
            break;
    }
    
    // Update system bus state and return final bus state
    c64->bus->data = bus_state.bus.data;
    return bus_state;
}

// ============================================================================
// BUS MANAGEMENT FUNCTIONS
// ============================================================================

c64_bus_t* c64_bus_create(void) {
    c64_bus_t* bus = (c64_bus_t*)calloc(1, sizeof(c64_bus_t));
    if (!bus) return NULL;
    
    // Initialize bus state
    bus->address = 0;
    bus->data = 0;
    bus->control_lines = BUS_MASK_BA | BUS_MASK_AEC | BUS_MASK_RDY;
    
    // Initialize system lines with default cartridge signals (no cartridge)
    bus->system_lines = SYS_MASK_EXROM | SYS_MASK_GAME;  // Both high = no cartridge
    
    return bus;
}

void c64_bus_destroy(c64_bus_t* bus) {
    if (bus) {
        free(bus);
    }
}

void c64_bus_attach_c64(c64_bus_t* bus, c64_t* c64) {
    if (bus) {
        bus->c64 = c64;
    }
}

// ============================================================================
// OPTIMIZED MEMORY ACCESS FUNCTIONS
// ============================================================================

// Waits for the bus to be ready, ticking non-CPU chips
static REGISTER_CALL c64_bus_state_t c64_wait_for_bus_ready(c64_bus_t* bus, c64_bus_state_t bus_state, bool is_read_cycle) {
    c64_t* c64 = bus->c64;

    if (is_read_cycle) {
        // CPU read must wait for VIC to release the bus (AEC high) AND
        // for the BA/RDY line to be high
        while (!(bus->control_lines & BUS_MASK_AEC) || !(bus->control_lines & BUS_MASK_BA)) {
            bus_state = c64_non_cpu_cycle(c64, bus_state);
            // Update control lines from returned bus state
            bus->control_lines = bus_state.bus.lines;
        }
    } else {
        // CPU write only needs to wait for VIC to release the address bus
        // It is NOT affected by the BA/RDY line
        while (!(bus->control_lines & BUS_MASK_AEC)) {
            bus_state = c64_non_cpu_cycle(c64, bus_state);
            // Update control lines from returned bus state
            bus->control_lines = bus_state.bus.lines;
        }
    }
    
    return bus_state;
}

REGISTER_CALL c64_bus_state_t c64_bus_read_cycle(c64_bus_t* bus, c64_bus_state_t bus_state) {
    // Handle bus contention and wait for bus availability
    bus_state = c64_wait_for_bus_ready(bus, bus_state, true);

    // The bus is now guaranteed to be ready for the CPU
    bus->address = bus_state.bus.addr;

    // Optimized system tick with compile-time read optimization
    // Set R/W line high for reads
    bus_state.bus.lines |= BUS_MASK_RW;
    
    return c64_system_tick_read(bus->c64, bus_state);
}

REGISTER_CALL c64_bus_state_t c64_bus_write_cycle(c64_bus_t* bus, c64_bus_state_t bus_state) {
    // Handle bus contention and wait for bus availability
    bus_state = c64_wait_for_bus_ready(bus, bus_state, false);

    // The bus is now guaranteed to be ready for the CPU
    bus->address = bus_state.bus.addr;
    bus->data = bus_state.bus.data;  // CPU puts data on bus for chips to read

    // Optimized system tick with compile-time write optimization  
    // Ensure R/W line is clear for writes
    bus_state.bus.lines &= ~BUS_MASK_RW;
    
    return c64_system_tick_write(bus->c64, bus_state);
}

// ============================================================================
// PLA MODE MANAGEMENT
// ============================================================================

static inline uint8_t encode_chip_select(uint8_t read_chip, uint8_t write_chip) {
    return (read_chip << 4) | (write_chip & 0x0F);
}

void c64_bus_mode_switch(c64_bus_t* bus, uint8_t mode) {
    if (!bus) return;
    
    // Update the optimized banking for the current mode
    bus->pla_banking_mode = mode & 0x1F;
    
    // Copy precalculated chip select data for the new mode
    memcpy(bus->cpu_encoded_chip_per_bank, 
           bus->cpu_encoded_chip_per_bank_per_mode[mode], 
           16);
}

uint8_t c64_bus_generate_pla_mode(c64_bus_t* bus, uint8_t cpu_port_bits) {
    if (!bus) return 0;
    
    // The PLA expects a 5-bit mode value with the following bit mapping:
    // Bit 0: LORAM (from CPU port bit 0)
    // Bit 1: HIRAM (from CPU port bit 1) 
    // Bit 2: CHAREN (from CPU port bit 2)
    // Bit 3: EXROM (from cartridge signal)
    // Bit 4: GAME (from cartridge signal)
    
    // Extract CPU I/O port control bits (bits 0-2 of $0001)
    uint8_t pla_mode = cpu_port_bits & 0x07; // LORAM | HIRAM | CHAREN
    
    // Add cartridge control signals from system lines
    pla_mode |= ((bus->system_lines & SYS_MASK_EXROM) ? 0x08 : 0); // EXROM (bit 3)
    pla_mode |= ((bus->system_lines & SYS_MASK_GAME) ? 0x10 : 0);  // GAME (bit 4)
    
    return pla_mode;
}

// ============================================================================
// CARTRIDGE CONTROL FUNCTIONS
// ============================================================================

void c64_bus_set_cartridge_signals(c64_bus_t* bus, bool exrom_active, bool game_active) {
    if (!bus) return;
    
    // Update EXROM signal
    if (exrom_active) {
        bus->system_lines &= ~SYS_MASK_EXROM;  // Clear bit (signal low)
    } else {
        bus->system_lines |= SYS_MASK_EXROM;   // Set bit (signal high)
    }
    
    // Update GAME signal
    if (game_active) {
        bus->system_lines &= ~SYS_MASK_GAME;   // Clear bit (signal low)
    } else {
        bus->system_lines |= SYS_MASK_GAME;    // Set bit (signal high)
    }
    
    // Update PLA mode based on new signals
    uint8_t cpu_port_bits = bus->pla_banking_mode & 0x07;
    uint8_t pla_mode = c64_bus_generate_pla_mode(bus, cpu_port_bits);
    c64_bus_mode_switch(bus, pla_mode);
}

// ============================================================================
// SYSTEM INTEGRATION FUNCTIONS
// ============================================================================

REGISTER_CALL c64_bus_state_t c64_non_cpu_cycle(c64_t* c64, c64_bus_state_t bus_state) {
    if (!c64) return bus_state;
    
    // Tick all non-CPU chips during bus wait states
    // This maintains proper timing during DMA and bus contention
    // Each chip can modify the bus state (e.g., set interrupt flags)
    bus_state.bus = vicii_advance_cycle(c64->vicii, bus_state.bus);
    bus_state.bus = sid_advance_cycle(c64->sid, bus_state.bus);
    bus_state.bus = cia_advance_cycle(c64->cia1, bus_state.bus);
    bus_state.bus = cia_advance_cycle(c64->cia2, bus_state.bus);
    
    return bus_state;
}

// ============================================================================
// EXAMPLE PLA INTEGRATION FUNCTIONS
// ============================================================================

// Example function to populate chip select arrays from PLA logic
// You would integrate this with your existing PLA code
void c64_bus_populate_chip_select_from_pla(c64_bus_t* bus) {
    if (!bus) return;
    
    // Generate all 32 PLA modes (5-bit combinations of LORAM, HIRAM, CHAREN, EXROM, GAME)
    for (int mode = 0; mode < 32; mode++) {
        for (uint32_t bank = 0; bank < 16; bank++) {
            // This would interface with your existing PLA logic
            // to determine read and write chip IDs for each bank/mode combination
            
            // Example hardcoded values (replace with actual PLA logic):
            uint8_t read_chip = CHIP_RAM;   // Default to RAM
            uint8_t write_chip = CHIP_RAM;  // Default to RAM
            
            // Your PLA logic would set these based on:
            // - mode bits (LORAM, HIRAM, CHAREN, EXROM, GAME)
            // - bank address (A15-A12)
            // - Read/Write mode
            
            if (bank == 13) {  // $D000-$DFFF I/O area example
                read_chip = CHIP_IO;   // I/O region - gets refined to specific I/O chip
                write_chip = CHIP_IO;  // I/O region - gets refined to specific I/O chip
            }
            
            // Encode both read and write chips
            bus->cpu_encoded_chip_per_bank_per_mode[mode][bank] = encode_chip_select(read_chip, write_chip);
        }
    }
    
    // Set initial mode (typically mode 7: LORAM=1, HIRAM=1, CHAREN=1, no cartridge)
    c64_bus_mode_switch(bus, 7);
}