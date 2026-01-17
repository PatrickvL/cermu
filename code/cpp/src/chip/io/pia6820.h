#pragma once

#include <cstdint>

/**
 * Motorola 6820 PIA (Peripheral Interface Adapter)
 * Used in Apple 1, early Apple II, and other 6502-based systems
 * 
 * Simplified implementation focused on Apple 1 keyboard/display needs:
 * - Port A: Keyboard input (read-only for Apple 1)
 * - Port B: Display output (write-only for Apple 1)
 */

struct pia6820_t {
    // Port A (typically keyboard in Apple 1)
    uint8_t port_a_data;        // Data register
    uint8_t port_a_control;     // Control register
    uint8_t port_a_direction;   // Data direction register (0=input, 1=output)
    
    // Port B (typically display in Apple 1)
    uint8_t port_b_data;        // Data register
    uint8_t port_b_control;     // Control register
    uint8_t port_b_direction;   // Data direction register
    
    // Interrupt flags
    bool irq_a;
    bool irq_b;
    
    // Callbacks for I/O
    void* user_data;
    void (*on_port_a_read)(void* user_data, uint8_t* data);
    void (*on_port_b_write)(void* user_data, uint8_t data);
};

// Initialize PIA
void pia6820_init(pia6820_t* pia);

// Reset PIA to power-on state
void pia6820_reset(pia6820_t* pia);

// Memory-mapped register access
uint8_t pia6820_read(pia6820_t* pia, uint16_t addr);
void pia6820_write(pia6820_t* pia, uint16_t addr, uint8_t data);

// Set keyboard input (for Apple 1: sets bit 7 and character in bits 0-6)
void pia6820_set_keyboard_data(pia6820_t* pia, uint8_t key_code);

// Check if keyboard data has been read (clears bit 7)
bool pia6820_keyboard_ready(pia6820_t* pia);