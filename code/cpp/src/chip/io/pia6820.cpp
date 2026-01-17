#include "pia6820.h"
#include <cstring>

// PIA 6820 Register offsets
#define PIA_REG_A_DATA      0x00
#define PIA_REG_A_CONTROL   0x01
#define PIA_REG_B_DATA      0x02
#define PIA_REG_B_CONTROL   0x03

// Control register bits
#define PIA_CTRL_IRQ1       0x80
#define PIA_CTRL_IRQ2       0x40
#define PIA_CTRL_DDR_SELECT 0x04  // 0=DDR, 1=Data
#define PIA_CTRL_IRQ_ENABLE 0x01

void pia6820_init(pia6820_t* pia) {
    if (!pia) return;
    
    memset(pia, 0, sizeof(pia6820_t));
    
    // Apple 1 defaults: Port A = input (keyboard), Port B = output (display)
    pia->port_a_direction = 0x00;  // All inputs
    pia->port_b_direction = 0xFF;  // All outputs
    
    pia->port_a_control = PIA_CTRL_DDR_SELECT;  // Select data register
    pia->port_b_control = PIA_CTRL_DDR_SELECT;  // Select data register
}

void pia6820_reset(pia6820_t* pia) {
    if (!pia) return;
    
    pia->port_a_data = 0x00;
    pia->port_b_data = 0x00;
    pia->port_a_control = 0x00;
    pia->port_b_control = 0x00;
    pia->irq_a = false;
    pia->irq_b = false;
    
    // Reset to DDR mode
    pia->port_a_direction = 0x00;
    pia->port_b_direction = 0x00;
}

uint8_t pia6820_read(pia6820_t* pia, uint16_t addr) {
    if (!pia) return 0xFF;
    
    uint8_t reg = addr & 0x03;
    switch (reg) {
        case PIA_REG_A_DATA: {
            // If DDR select bit is clear, return DDR
            if (!(pia->port_a_control & PIA_CTRL_DDR_SELECT)) {
                return pia->port_a_direction;
            }
            // Otherwise return port data
            // Call callback if provided
            if (pia->on_port_a_read) {
                pia->on_port_a_read(pia->user_data, &pia->port_a_data);
            }
            // Apple 1: Reading keyboard clears bit 7
            uint8_t data = pia->port_a_data;
            pia->port_a_data &= 0x7F;  // Clear strobe bit
            return data;
        }
            
        case PIA_REG_A_CONTROL:
            return pia->port_a_control;
            
        case PIA_REG_B_DATA:
            // If DDR select bit is clear, return DDR
            if (!(pia->port_b_control & PIA_CTRL_DDR_SELECT)) {
                return pia->port_b_direction;
            }
            // Otherwise return port data
            return pia->port_b_data;
            
        case PIA_REG_B_CONTROL:
            return pia->port_b_control;
    }
    
    return 0xFF;
}

void pia6820_write(pia6820_t* pia, uint16_t addr, uint8_t data) {
    if (!pia) return;
    
    uint8_t reg = addr & 0x03;
    
    switch (reg) {
        case PIA_REG_A_DATA:
            // If DDR select bit is clear, write DDR
            if (!(pia->port_a_control & PIA_CTRL_DDR_SELECT)) {
                pia->port_a_direction = data;
            } else {
                // Write port data (only affects output pins)
                pia->port_a_data = data;
            }
            break;
            
        case PIA_REG_A_CONTROL:
            pia->port_a_control = data;
            break;
            
        case PIA_REG_B_DATA:
            // If DDR select bit is clear, write DDR
            if (!(pia->port_b_control & PIA_CTRL_DDR_SELECT)) {
                pia->port_b_direction = data;
            } else {
                // Write port data
                pia->port_b_data = data;
                // Call callback if provided (Apple 1 display)
                if (pia->on_port_b_write) {
                    pia->on_port_b_write(pia->user_data, data);
                }
            }
            break;
            
        case PIA_REG_B_CONTROL:
            pia->port_b_control = data;
            break;
    }
}

void pia6820_set_keyboard_data(pia6820_t* pia, uint8_t key_code) {
    if (!pia) return;
    
    // Apple 1: Set bit 7 (strobe) and key code in bits 0-6
    pia->port_a_data = 0x80 | (key_code & 0x7F);
}

bool pia6820_keyboard_ready(pia6820_t* pia) {
    if (!pia) return false;
    
    // Check if bit 7 is set (keyboard data available)
    return (pia->port_a_data & 0x80) != 0;
}