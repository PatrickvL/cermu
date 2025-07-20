// ============================================================================
// CHIP IMPLEMENTATION STUBS WITH BUS STATE THREADING
// These are example implementations showing the interface patterns
// Replace with your actual chip implementations
// ============================================================================

#include "c64_bus_optimized.h"

// ============================================================================
// VIC-II CHIP IMPLEMENTATION STUB
// ============================================================================

typedef struct vic_state_s {
    uint8_t registers[64];      // VIC-II registers
    uint16_t raster_line;       // Current raster line
    uint16_t cycle;             // Current cycle within raster line
    uint8_t* video_memory;      // Pointer to video memory
    uint8_t* character_rom;     // Pointer to character ROM
    bool badline_active;        // True when VIC is stealing cycles
    bool irq_pending;           // IRQ flag state
    // ... other VIC-II state
} vic_state_t;

FORCE_INLINE REGISTER_CALL generic_bus_state_t vic_advance_cycle(vic_state_t* vic, generic_bus_state_t bus) {
    if (!vic) return bus;
    
    // Advance VIC-II timing
    vic->cycle++;
    if (vic->cycle >= 63) {  // PAL: 63 cycles per line
        vic->cycle = 0;
        vic->raster_line++;
        if (vic->raster_line >= 312) {  // PAL: 312 lines per frame
            vic->raster_line = 0;
        }
    }
    
    // Update raster line register
    vic->registers[0x11] = (vic->registers[0x11] & 0x7F) | ((vic->raster_line & 0x100) >> 1);
    vic->registers[0x12] = vic->raster_line & 0xFF;
    
    // Check for raster interrupt
    uint16_t raster_compare = ((vic->registers[0x11] & 0x80) << 1) | vic->registers[0x12];
    if (vic->raster_line == raster_compare) {
        vic->registers[0x19] |= 0x01;  // Set raster interrupt flag
        vic->irq_pending = true;
        // Set generic IRQ line
        bus.lines |= GENERIC_IRQ_LINE;
    }
    
    // Check for badline condition (DMA cycles)
    bool badline_condition = (vic->raster_line >= 0x30) && (vic->raster_line <= 0xF7) && 
                            ((vic->raster_line & 0x07) == (vic->registers[0x11] & 0x07));
    
    if (badline_condition && vic->cycle >= 15 && vic->cycle <= 54) {
        vic->badline_active = true;
        // Assert BA line (Bus Available = 0) for DMA
        bus.lines &= ~GENERIC_BA_LINE;
        // Keep AEC high so VIC can read from bus
        bus.lines |= GENERIC_AEC_LINE;
    } else {
        vic->badline_active = false;
        // Release BA line
        bus.lines |= GENERIC_BA_LINE;
        bus.lines |= GENERIC_AEC_LINE;
    }
    
    // Generate video output, handle sprites, etc.
    // This is where your actual VIC-II display generation would go
    
    return bus;
}

FORCE_INLINE REGISTER_CALL generic_bus_state_t vic_read(vic_state_t* vic, generic_bus_state_t bus) {
    if (!vic) return bus;
    
    // Handle VIC-II register read
    switch (bus.addr & 0x3F) {
        case 0x11:  // Control Register 1
            bus.data = vic->registers[0x11];
            break;
        case 0x12:  // Raster Line
            bus.data = vic->registers[0x12];
            break;
        case 0x16:  // Control Register 2
            bus.data = vic->registers[0x16];
            break;
        case 0x19:  // Interrupt Request Register
            bus.data = vic->registers[0x19];
            break;
        case 0x1A:  // Interrupt Mask Register
            bus.data = vic->registers[0x1A];
            break;
        default:
            bus.data = vic->registers[bus.addr & 0x3F];
            break;
    }
    
    return bus;
}

FORCE_INLINE REGISTER_CALL generic_bus_state_t vic_write(vic_state_t* vic, generic_bus_state_t bus) {
    if (!vic) return bus;
    
    // Handle VIC-II register write
    switch (bus.addr & 0x3F) {
        case 0x11:  // Control Register 1
            vic->registers[0x11] = (vic->registers[0x11] & 0x80) | (bus.data & 0x7F);
            break;
        case 0x12:  // Raster Line Compare
            vic->registers[0x12] = bus.data;
            break;
        case 0x16:  // Control Register 2
            vic->registers[0x16] = bus.data;
            break;
        case 0x19:  // Interrupt Request Register (write clears)
            vic->registers[0x19] &= ~bus.data;
            // Clear IRQ if no more interrupt sources active
            if (!(vic->registers[0x19] & vic->registers[0x1A] & 0x0F)) {
                vic->irq_pending = false;
                bus.lines &= ~GENERIC_IRQ_LINE;  // Clear generic IRQ line
            }
            break;
        case 0x1A:  // Interrupt Mask Register
            vic->registers[0x1A] = bus.data & 0x0F;
            break;
        default:
            vic->registers[bus.addr & 0x3F] = bus.data;
            break;
    }
    
    return bus;
}

// ============================================================================
// SID CHIP IMPLEMENTATION STUB
// ============================================================================

typedef struct sid_state_s {
    uint8_t registers[32];      // SID registers
    uint32_t voice1_counter;    // Voice 1 oscillator counter
    uint32_t voice2_counter;    // Voice 2 oscillator counter
    uint32_t voice3_counter;    // Voice 3 oscillator counter
    uint16_t lfsr;              // Linear feedback shift register for noise
    // ... other SID state
} sid_state_t;

FORCE_INLINE REGISTER_CALL generic_bus_state_t sid_advance_cycle(sid_state_t* sid, generic_bus_state_t bus_state) {
    if (!sid) return bus_state;
    
    // Advance SID oscillators (simplified)
    uint16_t freq1 = sid->registers[0] | (sid->registers[1] << 8);
    uint16_t freq2 = sid->registers[7] | (sid->registers[8] << 8);
    uint16_t freq3 = sid->registers[14] | (sid->registers[15] << 8);
    
    sid->voice1_counter += freq1;
    sid->voice2_counter += freq2;
    sid->voice3_counter += freq3;
    
    // Update noise LFSR
    if (sid->voice3_counter & 0x800000) {
        sid->lfsr = (sid->lfsr << 1) | (((sid->lfsr >> 22) ^ (sid->lfsr >> 17)) & 1);
    }
    
    // Generate audio samples, apply filters, etc.
    // This is where your actual SID audio synthesis would go
    // SID doesn't typically modify bus control lines
    
    return bus_state;  // Bus state unchanged by SID
}

FORCE_INLINE REGISTER_CALL generic_bus_state_t sid_read(sid_state_t* sid, generic_bus_state_t bus) {
    if (!sid) return bus;
    
    // Handle SID register read
    switch (bus.addr & 0x1F) {
        case 0x19:  // A/D Converter X
            bus.data = 0xFF;  // Stub implementation
            break;
        case 0x1A:  // A/D Converter Y  
            bus.data = 0xFF;  // Stub implementation
            break;
        case 0x1B:  // Voice 3 Oscillator Output
            bus.data = (sid->voice3_counter >> 16) & 0xFF;
            break;
        case 0x1C:  // Voice 3 Envelope Output
            bus.data = 0xFF;  // Stub implementation
            break;
        default:
            // Most SID registers are write-only - return floating bus
            // bus.data remains unchanged (retains floating bus value)
            break;
    }
    
    return bus;
}

FORCE_INLINE REGISTER_CALL generic_bus_state_t sid_write(sid_state_t* sid, generic_bus_state_t bus) {
    if (!sid) return bus;
    
    // Handle SID register write
    sid->registers[bus.addr & 0x1F] = bus.data;
    
    // Handle specific register updates
    switch (bus.addr & 0x1F) {
        case 0x04:  // Voice 1 Control Register
        case 0x0B:  // Voice 2 Control Register  
        case 0x12:  // Voice 3 Control Register
            // Handle gate bit, waveform selection, etc.
            break;
        case 0x17:  // Filter Resonance/Routing
        case 0x18:  // Filter Mode/Volume
            // Update filter parameters
            break;
    }
    
    return bus;
}

// ============================================================================
// CIA CHIP IMPLEMENTATION STUB  
// ============================================================================

typedef struct cia_state_s {
    uint8_t registers[16];      // CIA registers
    uint8_t timer_a_latch[2];   // Timer A latch
    uint8_t timer_b_latch[2];   // Timer B latch
    uint16_t timer_a_counter;   // Timer A current value
    uint16_t timer_b_counter;   // Timer B current value
    uint8_t shift_register;     // Serial shift register
    bool irq_pending;           // IRQ state for CIA1, NMI state for CIA2
    // ... other CIA state
} cia_state_t;

FORCE_INLINE REGISTER_CALL generic_bus_state_t cia_advance_cycle(cia_state_t* cia, generic_bus_state_t bus) {
    if (!cia) return bus;
    
    // Handle Timer A
    if (cia->registers[0x0E] & 0x01) {  // Timer A enabled
        if (cia->timer_a_counter > 0) {
            cia->timer_a_counter--;
            if (cia->timer_a_counter == 0) {
                // Timer A underflow
                cia->registers[0x0D] |= 0x01;  // Set interrupt flag
                
                // Check if interrupt is enabled
                if (cia->registers[0x0D] & 0x80) {  // Master interrupt enable
                    if (cia->registers[0x0D] & 0x01) {  // Timer A interrupt enabled
                        cia->irq_pending = true;
                        // Set IRQ line (CIA1) or NMI line (CIA2) in bus state
                        // This would be handled differently for CIA1 vs CIA2
                        // For now, just set a general interrupt flag
                        bus.lines |= GENERIC_IRQ_LINE;
                    }
                }
                
                if (cia->registers[0x0E] & 0x08) {  // One-shot mode
                    cia->registers[0x0E] &= ~0x01;  // Disable timer
                } else {  // Continuous mode
                    cia->timer_a_counter = cia->timer_a_latch[0] | (cia->timer_a_latch[1] << 8);
                }
            }
        }
    }
    
    // Handle Timer B
    if (cia->registers[0x0F] & 0x01) {  // Timer B enabled
        bool count_timer_b = true;
        
        // Check Timer B input source
        if ((cia->registers[0x0F] & 0x60) == 0x40) {  // Count Timer A underflows
            count_timer_b = (cia->registers[0x0D] & 0x01) != 0;
        }
        
        if (count_timer_b && cia->timer_b_counter > 0) {
            cia->timer_b_counter--;
            if (cia->timer_b_counter == 0) {
                // Timer B underflow
                cia->registers[0x0D] |= 0x02;  // Set interrupt flag
                
                // Check if interrupt is enabled
                if (cia->registers[0x0D] & 0x80) {  // Master interrupt enable
                    if (cia->registers[0x0D] & 0x02) {  // Timer B interrupt enabled
                        cia->irq_pending = true;
                        bus.lines |= GENERIC_IRQ_LINE;
                    }
                }
                
                if (cia->registers[0x0F] & 0x08) {  // One-shot mode
                    cia->registers[0x0F] &= ~0x01;  // Disable timer
                } else {  // Continuous mode
                    cia->timer_b_counter = cia->timer_b_latch[0] | (cia->timer_b_latch[1] << 8);
                }
            }
        }
    }
    
    // Handle serial shift register, TOD clock, etc.
    // This is where your actual CIA functionality would go
    
    return bus;
}

FORCE_INLINE REGISTER_CALL generic_bus_state_t cia_read(cia_state_t* cia, generic_bus_state_t bus) {
    if (!cia) return bus;
    
    // Handle CIA register read
    switch (bus.addr & 0x0F) {
        case 0x00:  // Data Port A
        case 0x01:  // Data Port B
            bus.data = cia->registers[bus.addr & 0x0F];
            break;
        case 0x02:  // Data Direction Register A
        case 0x03:  // Data Direction Register B
            bus.data = cia->registers[bus.addr & 0x0F];
            break;
        case 0x04:  // Timer A Low
            bus.data = cia->timer_a_counter & 0xFF;
            break;
        case 0x05:  // Timer A High
            bus.data = (cia->timer_a_counter >> 8) & 0xFF;
            break;
        case 0x06:  // Timer B Low
            bus.data = cia->timer_b_counter & 0xFF;
            break;
        case 0x07:  // Timer B High
            bus.data = (cia->timer_b_counter >> 8) & 0xFF;
            break;
        case 0x0D:  // Interrupt Control/Status
            bus.data = cia->registers[0x0D];
            cia->registers[0x0D] = 0;  // Reading ICR clears it
            cia->irq_pending = false;  // Clear pending interrupt
            bus.lines &= ~GENERIC_IRQ_LINE;  // Clear generic IRQ line
            break;
        default:
            bus.data = cia->registers[bus.addr & 0x0F];
            break;
    }
    
    return bus;
}

FORCE_INLINE REGISTER_CALL generic_bus_state_t cia_write(cia_state_t* cia, generic_bus_state_t bus) {
    if (!cia) return bus;
    
    // Handle CIA register write
    switch (bus.addr & 0x0F) {
        case 0x00:  // Data Port A
        case 0x01:  // Data Port B
        case 0x02:  // Data Direction Register A
        case 0x03:  // Data Direction Register B
            cia->registers[bus.addr & 0x0F] = bus.data;
            break;
        case 0x04:  // Timer A Low
            cia->timer_a_latch[0] = bus.data;
            break;
        case 0x05:  // Timer A High
            cia->timer_a_latch[1] = bus.data;
            if (!(cia->registers[0x0E] & 0x01)) {  // Load if stopped
                cia->timer_a_counter = cia->timer_a_latch[0] | (cia->timer_a_latch[1] << 8);
            }
            break;
        case 0x06:  // Timer B Low
            cia->timer_b_latch[0] = bus.data;
            break;
        case 0x07:  // Timer B High
            cia->timer_b_latch[1] = bus.data;
            if (!(cia->registers[0x0F] & 0x01)) {  // Load if stopped
                cia->timer_b_counter = cia->timer_b_latch[0] | (cia->timer_b_latch[1] << 8);
            }
            break;
        case 0x0D:  // Interrupt Control/Status
            if (bus.data & 0x80) {  // Set bits
                cia->registers[0x0D] |= (bus.data & 0x7F);
            } else {  // Clear bits
                cia->registers[0x0D] &= ~(bus.data & 0x7F);
            }
            break;
        case 0x0E:  // Timer A Control
            cia->registers[0x0E] = bus.data;
            if (bus.data & 0x10) {  // Force load
                cia->timer_a_counter = cia->timer_a_latch[0] | (cia->timer_a_latch[1] << 8);
                cia->registers[0x0E] &= ~0x10;  // Clear force load bit
            }
            break;
        case 0x0F:  // Timer B Control
            cia->registers[0x0F] = bus.data;
            if (bus.data & 0x10) {  // Force load
                cia->timer_b_counter = cia->timer_b_latch[0] | (cia->timer_b_latch[1] << 8);
                cia->registers[0x0F] &= ~0x10;  // Clear force load bit
            }
            break;
        default:
            cia->registers[bus.addr & 0x0F] = bus.data;
            break;
    }
    
    return bus;
}cia->registers[bus.addr & 0x0F] = bus.data;
            break;
        case 0x04:  // Timer A Low
            cia->timer_a_latch[0] = bus.data;
            break;
        case 0x05:  // Timer A High
            cia->timer_a_latch[1] = bus.data;
            if (!(cia->registers[0x0E] & 0x01)) {  // Load if stopped
                cia->timer_a_counter = cia->timer_a_latch[0] | (cia->timer_a_latch[1] << 8);
            }
            break;
        case 0x06:  // Timer B Low
            cia->timer_b_latch[0] = bus.data;
            break;
        case 0x07:  // Timer B High
            cia->timer_b_latch[1] = bus.data;
            if (!(cia->registers[0x0F] & 0x01)) {  // Load if stopped
                cia->timer_b_counter = cia->timer_b_latch[0] | (cia->timer_b_latch[1] << 8);
            }
            break;
        case 0x0D:  // Interrupt Control/Status
            if (bus.data & 0x80) {  // Set bits
                cia->registers[0x0D] |= (bus.data & 0x7F);
            } else {  // Clear bits
                cia->registers[0x0D] &= ~(bus.data & 0x7F);
            }
            break;
        case 0x0E:  // Timer A Control
            cia->registers[0x0E] = bus.data;
            if (bus.data & 0x10) {  // Force load
                cia->timer_a_counter = cia->timer_a_latch[0] | (cia->timer_a_latch[1] << 8);
                cia->registers[0x0E] &= ~0x10;  // Clear force load bit
            }
            break;
        case 0x0F:  // Timer B Control
            cia->registers[0x0F] = bus.data;
            if (bus.data & 0x10) {  // Force load
                cia->timer_b_counter = cia->timer_b_latch[0] | (cia->timer_b_latch[1] << 8);
                cia->registers[0x0F] &= ~0x10;  // Clear force load bit
            }
            break;
        default:
            cia->registers[bus.addr & 0x0F] = bus.data;
            break;
    }
    
    return bus.raw;
}