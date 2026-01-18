#include "pia6820.h"
#include <cstring>

// PIA 6820 Register offsets
#define PIA_REG_A_DATA      0x00
#define PIA_REG_A_CONTROL   0x01
#define PIA_REG_B_DATA      0x02
#define PIA_REG_B_CONTROL   0x03

// Control register bits
#define PIA_CTRL_IRQ1       0x80  // IRQ1 flag (read-only)
#define PIA_CTRL_IRQ2       0x40  // IRQ2 flag (read-only)
#define PIA_CTRL_CA2_OUTPUT 0x20  // CA2 direction: 1=output, 0=input
#define PIA_CTRL_CA2_MODE   0x10  // CA2 mode bit (combined with bit 3)
#define PIA_CTRL_CA2_CTRL   0x08  // CA2 control bit (combined with bit 4)
#define PIA_CTRL_DDR_SELECT 0x04  // Data Direction Register select: 0=DDR, 1=Data
#define PIA_CTRL_CA1_EDGE   0x02  // CA1 transition mode: 0=falling, 1=rising
#define PIA_CTRL_IRQ_ENABLE 0x01  // IRQ1 enable

// Helper macros for control register bit extraction
#define IS_CA2_OUTPUT(cra)      ((cra) & PIA_CTRL_CA2_OUTPUT)
#define IS_CB2_OUTPUT(crb)      ((crb) & PIA_CTRL_CA2_OUTPUT)
#define GET_CA2_MODE(cra)       (((cra) >> 3) & 0x03)
#define GET_CB2_MODE(crb)       (((crb) >> 3) & 0x03)
#define DDR_SELECTED(cr)        (!((cr) & PIA_CTRL_DDR_SELECT))
#define CA1_RISING_EDGE(cra)    ((cra) & PIA_CTRL_CA1_EDGE)
#define CB1_RISING_EDGE(crb)    ((crb) & PIA_CTRL_CA1_EDGE)
#define IRQ1_ENABLED(cr)        ((cr) & PIA_CTRL_IRQ_ENABLE)
#define IRQ2_ENABLED(cr)        (!IS_CA2_OUTPUT(cr) && ((cr) & PIA_CTRL_CA2_CTRL))

// Forward declarations for internal functions
static void update_irq(pia6820_t* pia);
static void update_ca2_output(pia6820_t* pia);
static void update_cb2_output(pia6820_t* pia);
static uint8_t read_port_with_direction(uint8_t output_reg, uint8_t ddr, 
                                         uint8_t (*read_callback)(void*), void* user_data);

void pia6820_init(pia6820_t* pia) {
    if (!pia) return;
    
    memset(pia, 0, sizeof(pia6820_t));
    
    // Default: Both ports configured as inputs
    pia->port_a_direction = 0x00;  // All inputs
    pia->port_b_direction = 0x00;  // All inputs
    
    pia->port_a_control = PIA_CTRL_DDR_SELECT;  // Select data register
    pia->port_b_control = PIA_CTRL_DDR_SELECT;  // Select data register
}

void pia6820_reset(pia6820_t* pia) {
    if (!pia) return;
    
    // Reset all registers to power-on state
    pia->port_a_data = 0x00;
    pia->port_b_data = 0x00;
    pia->port_a_control = 0x00;
    pia->port_b_control = 0x00;
    
    // Clear all interrupt flags
    pia->irq_a1 = false;
    pia->irq_a2 = false;
    pia->irq_b1 = false;
    pia->irq_b2 = false;
    
    // Reset control line states
    pia->ca1_state = false;
    pia->ca2_state = false;
    pia->cb1_state = false;
    pia->cb2_state = false;
    
    // Reset to DDR mode
    pia->port_a_direction = 0x00;
    pia->port_b_direction = 0x00;
    
    update_irq(pia);
}

uint8_t pia6820_read(pia6820_t* pia, uint16_t addr) {
    if (!pia) return 0xFF;
    
    uint8_t reg = addr & 0x03;
    switch (reg) {
        case PIA_REG_A_DATA: {
            // If DDR select bit is clear, return DDR
            if (DDR_SELECTED(pia->port_a_control)) {
                return pia->port_a_direction;
            }
            
            // Reading port A clears CA1 and CA2 interrupt flags
            pia->irq_a1 = false;
            pia->irq_a2 = false;
            update_irq(pia);
            
            // Read port data with direction control
            uint8_t data = read_port_with_direction(
                pia->port_a_data, 
                pia->port_a_direction,
                pia->on_port_a_read,
                pia->user_data
            );
            
            // Note: Some systems (e.g. Apple 1) may clear bit 7 of port A data
            // after reading, but this is system-specific behavior handled externally
            
            return data;
        }
            
        case PIA_REG_A_CONTROL: {
            // Bits 7-6: IRQ flags (read-only), Bits 5-0: Control bits
            uint8_t value = pia->port_a_control & 0x3F;
            if (pia->irq_a1) value |= PIA_CTRL_IRQ1;
            if (pia->irq_a2) value |= PIA_CTRL_IRQ2;
            return value;
        }
            
        case PIA_REG_B_DATA: {
            // If DDR select bit is clear, return DDR
            if (DDR_SELECTED(pia->port_b_control)) {
                return pia->port_b_direction;
            }
            
            // Reading port B clears CB1 and CB2 interrupt flags
            pia->irq_b1 = false;
            pia->irq_b2 = false;
            update_irq(pia);
            
            // Read port data with direction control
            return read_port_with_direction(
                pia->port_b_data,
                pia->port_b_direction,
                pia->on_port_b_read,
                pia->user_data
            );
        }
            
        case PIA_REG_B_CONTROL: {
            // Bits 7-6: IRQ flags (read-only), Bits 5-0: Control bits
            uint8_t value = pia->port_b_control & 0x3F;
            if (pia->irq_b1) value |= PIA_CTRL_IRQ1;
            if (pia->irq_b2) value |= PIA_CTRL_IRQ2;
            return value;
        }
    }
    
    return 0xFF;
}

void pia6820_write(pia6820_t* pia, uint16_t addr, uint8_t data) {
    if (!pia) return;
    
    uint8_t reg = addr & 0x03;
    
    switch (reg) {
        case PIA_REG_A_DATA:
            // If DDR select bit is clear, write DDR
            if (DDR_SELECTED(pia->port_a_control)) {
                pia->port_a_direction = data;
            } else {
                // Write port data (stored in output register)
                pia->port_a_data = data;
                
                // Call external write callback for output pins only
                if (pia->on_port_a_write) {
                    uint8_t output = pia->port_a_data & pia->port_a_direction;
                    pia->on_port_a_write(pia->user_data, output);
                }
                
                // If CA2 is in write strobe mode, pulse it
                if (IS_CA2_OUTPUT(pia->port_a_control) && GET_CA2_MODE(pia->port_a_control) == 0b10) {
                    pia->ca2_state = false;
                    if (pia->on_ca2_output) pia->on_ca2_output(pia->user_data, false);
                    // In real hardware this would go high after one E cycle
                    pia->ca2_state = true;
                    if (pia->on_ca2_output) pia->on_ca2_output(pia->user_data, true);
                }
            }
            break;
            
        case PIA_REG_A_CONTROL:
            pia->port_a_control = data & 0x3F;  // Only bits 0-5 are writable
            update_ca2_output(pia);
            update_irq(pia);
            break;
            
        case PIA_REG_B_DATA:
            // If DDR select bit is clear, write DDR
            if (DDR_SELECTED(pia->port_b_control)) {
                pia->port_b_direction = data;
            } else {
                // Write port data
                pia->port_b_data = data;
                
                // Call external write callback for output pins only
                if (pia->on_port_b_write) {
                    uint8_t output = pia->port_b_data & pia->port_b_direction;
                    pia->on_port_b_write(pia->user_data, output);
                }
                
                // If CB2 is in write strobe mode, pulse it
                if (IS_CB2_OUTPUT(pia->port_b_control) && GET_CB2_MODE(pia->port_b_control) == 0b10) {
                    pia->cb2_state = false;
                    if (pia->on_cb2_output) pia->on_cb2_output(pia->user_data, false);
                    pia->cb2_state = true;
                    if (pia->on_cb2_output) pia->on_cb2_output(pia->user_data, true);
                }
            }
            break;
            
        case PIA_REG_B_CONTROL:
            pia->port_b_control = data & 0x3F;  // Only bits 0-5 are writable
            update_cb2_output(pia);
            update_irq(pia);
            break;
    }
}

// External control line inputs for handshaking and interrupts
void pia6820_set_ca1(pia6820_t* pia, bool state) {
    if (!pia) return;
    
    bool old_state = pia->ca1_state;
    pia->ca1_state = state;
    
    // Detect edge based on CRA bit 1 (0=falling, 1=rising)
    bool rising_edge = !old_state && state;
    bool falling_edge = old_state && !state;
    bool trigger = (CA1_RISING_EDGE(pia->port_a_control) && rising_edge) || 
                   (!CA1_RISING_EDGE(pia->port_a_control) && falling_edge);
    
    if (trigger) {
        pia->irq_a1 = true;
        update_irq(pia);
    }
}

void pia6820_set_ca2_input(pia6820_t* pia, bool state) {
    if (!pia) return;
    if (IS_CA2_OUTPUT(pia->port_a_control)) return;  // Ignore if CA2 is output
    
    bool old_state = pia->ca2_state;
    pia->ca2_state = state;
    
    // Detect edge based on CRA bit 4
    bool rising_edge = !old_state && state;
    bool falling_edge = old_state && !state;
    bool trigger = ((pia->port_a_control & PIA_CTRL_CA2_MODE) && rising_edge) || 
                   (!(pia->port_a_control & PIA_CTRL_CA2_MODE) && falling_edge);
    
    if (trigger) {
        pia->irq_a2 = true;
        update_irq(pia);
    }
}

void pia6820_set_cb1(pia6820_t* pia, bool state) {
    if (!pia) return;
    
    bool old_state = pia->cb1_state;
    pia->cb1_state = state;
    
    // Detect edge
    bool rising_edge = !old_state && state;
    bool falling_edge = old_state && !state;
    bool trigger = (CB1_RISING_EDGE(pia->port_b_control) && rising_edge) || 
                   (!CB1_RISING_EDGE(pia->port_b_control) && falling_edge);
    
    if (trigger) {
        pia->irq_b1 = true;
        update_irq(pia);
        
        // CB1 active transition also affects CB2 in some output modes
        if (IS_CB2_OUTPUT(pia->port_b_control)) {
            update_cb2_output(pia);
        }
    }
}

void pia6820_set_cb2_input(pia6820_t* pia, bool state) {
    if (!pia) return;
    if (IS_CB2_OUTPUT(pia->port_b_control)) return;  // Ignore if CB2 is output
    
    bool old_state = pia->cb2_state;
    pia->cb2_state = state;
    
    // Detect edge
    bool rising_edge = !old_state && state;
    bool falling_edge = old_state && !state;
    bool trigger = ((pia->port_b_control & PIA_CTRL_CA2_MODE) && rising_edge) || 
                   (!(pia->port_b_control & PIA_CTRL_CA2_MODE) && falling_edge);
    
    if (trigger) {
        pia->irq_b2 = true;
        update_irq(pia);
    }
}

// Direct port input functions (for external hardware simulation)
void pia6820_set_port_a_input(pia6820_t* pia, uint8_t value) {
    if (!pia) return;
    
    // Store the input value (will be read back based on DDR)
    // This bypasses the callback and directly sets port A input state
    pia->port_a_data = value;
}

void pia6820_set_port_b_input(pia6820_t* pia, uint8_t value) {
    if (!pia) return;
    
    // Store the input value (will be read back based on DDR)
    pia->port_b_data = value;
}

// Internal helper functions
static void update_irq(pia6820_t* pia) {
    // IRQA is asserted if either IRQA1 or IRQA2 is set and enabled
    bool irqa = (pia->irq_a1 && IRQ1_ENABLED(pia->port_a_control)) || 
                (pia->irq_a2 && IRQ2_ENABLED(pia->port_a_control));
    
    bool irqb = (pia->irq_b1 && IRQ1_ENABLED(pia->port_b_control)) || 
                (pia->irq_b2 && IRQ2_ENABLED(pia->port_b_control));
    
    if (pia->on_irq_a) {
        pia->on_irq_a(pia->user_data, irqa);
    }
    
    if (pia->on_irq_b) {
        pia->on_irq_b(pia->user_data, irqb);
    }
}

static void update_ca2_output(pia6820_t* pia) {
    if (!IS_CA2_OUTPUT(pia->port_a_control)) return;
    
    uint8_t mode = GET_CA2_MODE(pia->port_a_control);
    bool new_state = false;
    
    switch (mode) {
        case 0b00:  // Set low on CA1 active transition, reset high on read Port A
        case 0b01:  // Set low on CA1 active transition, reset high on E pulse after read
            // These modes are handled by the read/write strobes
            new_state = pia->ca2_state;
            break;
            
        case 0b10:  // Set low on write Port A, reset high on E pulse after write
            // Handled in write function
            new_state = pia->ca2_state;
            break;
            
        case 0b11:  // Manual output - controlled by bit 3 of CRA
            new_state = pia->port_a_control & PIA_CTRL_CA2_CTRL;
            break;
    }
    
    if (new_state != pia->ca2_state) {
        pia->ca2_state = new_state;
        if (pia->on_ca2_output) {
            pia->on_ca2_output(pia->user_data, new_state);
        }
    }
}

static void update_cb2_output(pia6820_t* pia) {
    if (!IS_CB2_OUTPUT(pia->port_b_control)) return;
    
    uint8_t mode = GET_CB2_MODE(pia->port_b_control);
    bool new_state = false;
    
    switch (mode) {
        case 0b00:  // Set low on CB1 active transition, reset high on read Port B
        case 0b01:  // Set low on CB1 active transition, reset high on E pulse after read
            new_state = pia->cb2_state;
            break;
            
        case 0b10:  // Set low on write Port B, reset high on E pulse after write
            new_state = pia->cb2_state;
            break;
            
        case 0b11:  // Manual output
            new_state = pia->port_b_control & PIA_CTRL_CA2_CTRL;
            break;
    }
    
    if (new_state != pia->cb2_state) {
        pia->cb2_state = new_state;
        if (pia->on_cb2_output) {
            pia->on_cb2_output(pia->user_data, new_state);
        }
    }
}

static uint8_t read_port_with_direction(uint8_t output_reg, uint8_t ddr, 
                                         uint8_t (*read_callback)(void*), void* user_data) {
    uint8_t result = 0x00;
    
    if (read_callback) {
        uint8_t input = read_callback(user_data);
        
        // For each bit: if DDR bit is 0 (input), use external input
        //               if DDR bit is 1 (output), use output register
        for (int i = 0; i < 8; i++) {
            if (ddr & (1 << i)) {
                // Output bit - return what we're driving
                result |= (output_reg & (1 << i));
            } else {
                // Input bit - return external value
                result |= (input & (1 << i));
            }
        }
    } else {
        // No callback - just return output register for output pins
        result = output_reg & ddr;
    }
    
    return result;
}
