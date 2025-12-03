#include "mos6522.h"
#include "../input/commodore_keyboard.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// MOS6522 VIA chip descriptor
chip_descriptor_t mos6522_descriptor = {
    .description = "MOS6522 VIA Versatile Interface Adapter",
    .create = mos6522_create,
    .destroy = mos6522_destroy,
    .bus_attach = (void (*)(void *, void *))mos6522_bus_attach,
    .bank_change = NULL
};

void* mos6522_create(chip_descriptor_t* desc) {
    mos6522_t* via = (mos6522_t*)calloc(1, sizeof(mos6522_t));
    if (!via) return NULL;

    via->desc = desc;

    // Initialize registers
    memset(via->registers, 0, sizeof(via->registers));

    // Initialize ports
    via->port_a_data = 0xFF;
    via->port_b_data = 0xFF;
    via->port_a_ddr = 0x00;
    via->port_b_ddr = 0x00;

    // Initialize timers
    via->timer1_latch = 0xFFFF;
    via->timer1_counter = 0xFFFF;
    via->timer2_latch = 0xFFFF;
    via->timer2_counter = 0xFFFF;

    // Initialize control registers
    via->acr = 0x00;
    via->pcr = 0x00;
    via->ifr = 0x00;
    via->ier = 0x00;

    // Initialize timer state
    via->timer1_running = false;
    via->timer2_running = false;
    via->timer1_continuous = false;
    via->timer2_continuous = false;

    // Initialize interrupt state
    via->interrupt_active = false;
    via->interrupt_line = 0;

    return via;
}

void mos6522_destroy(void* chip) {
    if (!chip) return;
    mos6522_t* via = (mos6522_t*)chip;
    free(via);
}

void mos6522_connect_keyboard(void* chip, void* keyboard) {
    if (!chip) return;
    mos6522_t* via = (mos6522_t*)chip;
    via->keyboard_reference = keyboard;

    // Connect keyboard ports to VIA ports
    if (keyboard) {
        commodore_keyboard_connect_ports((commodore_keyboard_t*)keyboard, &via->port_a_data, &via->port_b_data);
    }
}

void mos6522_update_keyboard_matrix(void* chip) {
    if (!chip) return;
    mos6522_t* via = (mos6522_t*)chip;

    if (via->keyboard_reference) {
        commodore_keyboard_t* keyboard = (commodore_keyboard_t*)via->keyboard_reference;

        // Update keyboard matrix based on current port states
        commodore_keyboard_update_matrix(keyboard);

        // Synchronize keyboard contacts with VIA port data
        // Port A (columns) - update based on keyboard column contacts
        uint8_t keyboard_cols = 0xFF; // Default: all columns open

        for (int col = 0; col < 8; col++) {
            bool col_closed = false;
            for (int row = 0; row < 8; row++) {
                if (commodore_keyboard_is_col_closed(keyboard, row, col)) {
                    col_closed = true;
                    break;
                }
            }
            if (col_closed) {
                keyboard_cols &= ~(1 << col);
            }
        }

        // Port B (rows) - update based on keyboard row contacts
        uint8_t keyboard_rows = 0xFF; // Default: all rows open

        for (int row = 0; row < 8; row++) {
            bool row_closed = false;
            for (int col = 0; col < 8; col++) {
                if (commodore_keyboard_is_row_closed(keyboard, row, col)) {
                    row_closed = true;
                    break;
                }
            }
            if (row_closed) {
                keyboard_rows &= ~(1 << row);
            }
        }

        // Update VIA port data based on keyboard state
        // Only update if ports are configured as input
        if ((via->port_a_ddr & 0xFF) == 0x00) { // Port A as input
            via->port_a_data = keyboard_cols;
        }

        if ((via->port_b_ddr & 0xFF) == 0x00) { // Port B as input
            via->port_b_data = keyboard_rows;
        }
    }
}

void mos6522_reset(mos6522_t* via) {
    if (!via) return;

    // Reset registers
    memset(via->registers, 0, sizeof(via->registers));

    // Reset ports
    via->port_a_data = 0xFF;
    via->port_b_data = 0xFF;
    via->port_a_ddr = 0x00;
    via->port_b_ddr = 0x00;

    // Reset timers
    via->timer1_latch = 0xFFFF;
    via->timer1_counter = 0xFFFF;
    via->timer2_latch = 0xFFFF;
    via->timer2_counter = 0xFFFF;

    // Reset control registers
    via->acr = 0x00;
    via->pcr = 0x00;
    via->ifr = 0x00;
    via->ier = 0x00;

    // Reset timer state
    via->timer1_running = false;
    via->timer2_running = false;
    via->timer1_continuous = false;
    via->timer2_continuous = false;

    // Reset interrupt state
    via->interrupt_active = false;
}

void mos6522_bus_attach(void* chip, void* bus) {
    if (!chip) return;
    mos6522_t* via = (mos6522_t*)chip;
    via->bus = bus;
}

bus_state_t mos6522_registers_read(void* chip, bus_state_t bus_state) {
    mos6522_t* via = (mos6522_t*)chip;
    if (!via) return bus_state;

    uint8_t reg = BUS_GET_ADDR(bus_state) & 0x0F; // 16 registers

    switch (reg) {
        case MOS6522_PORTB:
            // Port B read - check if keyboard is connected
            if (via->keyboard_reference) {
                // Keyboard matrix integration for Port B (rows)
                // Bits represent row states based on keyboard contacts
                uint8_t keyboard_rows = 0xFF; // Default: all rows open

                // Check each row for closed contacts
                for (int row = 0; row < 8; row++) {
                    bool row_closed = false;
                    // Check if any column in this row has a closed contact
                    for (int col = 0; col < 8; col++) {
                        if (commodore_keyboard_is_row_closed((commodore_keyboard_t*)via->keyboard_reference, row, col)) {
                            row_closed = true;
                            break;
                        }
                    }
                    if (row_closed) {
                        keyboard_rows &= ~(1 << row);
                    }
                }

                // Combine with actual port data
                BUS_SET_DATA(bus_state, via->port_b_data & keyboard_rows);
            } else {
                BUS_SET_DATA(bus_state, via->port_b_data);
            }
            break;
        case MOS6522_PORTA:
            // Port A read - check if keyboard is connected
            if (via->keyboard_reference) {
                // Keyboard matrix integration for Port A (columns)
                // Bits represent column states based on keyboard contacts
                uint8_t keyboard_cols = 0xFF; // Default: all columns open

                // Check each column for closed contacts
                for (int col = 0; col < 8; col++) {
                    bool col_closed = false;
                    // Check if any row in this column has a closed contact
                    for (int row = 0; row < 8; row++) {
                        if (commodore_keyboard_is_col_closed((commodore_keyboard_t*)via->keyboard_reference, row, col)) {
                            col_closed = true;
                            break;
                        }
                    }
                    if (col_closed) {
                        keyboard_cols &= ~(1 << col);
                    }
                }

                // Combine with actual port data
                BUS_SET_DATA(bus_state, via->port_a_data & keyboard_cols);
            } else {
                BUS_SET_DATA(bus_state, via->port_a_data);
            }
            break;
        case MOS6522_DDRB:
            BUS_SET_DATA(bus_state, via->port_b_ddr);
            break;
        case MOS6522_DDRA:
            BUS_SET_DATA(bus_state, via->port_a_ddr);
            break;
        case MOS6522_T1CL:
            BUS_SET_DATA(bus_state, (uint8_t)(via->timer1_counter & 0xFF));
            break;
        case MOS6522_T1CH:
            BUS_SET_DATA(bus_state, (uint8_t)(via->timer1_counter >> 8));
            break;
        case MOS6522_T1LL:
            BUS_SET_DATA(bus_state, (uint8_t)(via->timer1_latch & 0xFF));
            break;
        case MOS6522_T1LH:
            BUS_SET_DATA(bus_state, (uint8_t)(via->timer1_latch >> 8));
            break;
        case MOS6522_T2CL:
            BUS_SET_DATA(bus_state, (uint8_t)(via->timer2_counter & 0xFF));
            break;
        case MOS6522_T2CH:
            BUS_SET_DATA(bus_state, (uint8_t)(via->timer2_counter >> 8));
            break;
        case MOS6522_SR:
            BUS_SET_DATA(bus_state, via->shift_register);
            break;
        case MOS6522_ACR:
            BUS_SET_DATA(bus_state, via->acr);
            break;
        case MOS6522_PCR:
            BUS_SET_DATA(bus_state, via->pcr);
            break;
        case MOS6522_IFR:
            BUS_SET_DATA(bus_state, via->ifr);
            // Reading IFR clears interrupt flags
            via->ifr = 0;
            break;
        case MOS6522_IER:
            BUS_SET_DATA(bus_state, via->ier);
            break;
        case MOS6522_PORTA_NH:
            BUS_SET_DATA(bus_state, via->port_a_data);
            break;
        default:
            // Unknown register - return last bus data
            break;
    }

    return bus_state;
}

bus_state_t mos6522_registers_write(void* chip, bus_state_t bus_state) {
    mos6522_t* via = (mos6522_t*)chip;
    if (!via) return bus_state;

    uint8_t reg = BUS_GET_ADDR(bus_state) & 0x0F; // 16 registers
    uint8_t value = BUS_GET_DATA(bus_state);

    switch (reg) {
        case MOS6522_PORTB:
            via->port_b_data = value;
            break;
        case MOS6522_PORTA:
            via->port_a_data = value;
            break;
        case MOS6522_DDRB:
            via->port_b_ddr = value;
            break;
        case MOS6522_DDRA:
            via->port_a_ddr = value;
            break;
        case MOS6522_T1LL:
            via->timer1_latch = (via->timer1_latch & 0xFF00) | value;
            break;
        case MOS6522_T1LH:
            via->timer1_latch = (via->timer1_latch & 0x00FF) | (value << 8);
            // If timer is not running, load counter
            if (!via->timer1_running) {
                via->timer1_counter = via->timer1_latch;
            }
            break;
        case 0x08: // T2LL
            via->timer2_latch = (via->timer2_latch & 0xFF00) | value;
            break;
        case 0x09: // T2LH
            via->timer2_latch = (via->timer2_latch & 0x00FF) | (value << 8);
            // If timer is not running, load counter
            if (!via->timer2_running) {
                via->timer2_counter = via->timer2_latch;
            }
            break;
        case MOS6522_SR:
            via->shift_register = value;
            break;
        case MOS6522_ACR:
            via->acr = value;
            break;
        case MOS6522_PCR:
            via->pcr = value;
            break;
        case MOS6522_IFR:
            // Writing to IFR clears interrupt flags
            via->ifr = 0;
            break;
        case MOS6522_IER:
            via->ier = value;
            break;
        case MOS6522_PORTA_NH:
            via->port_a_data = value;
            break;
        default:
            // Unknown register - ignore write
            break;
    }

    return bus_state;
}

bus_state_t mos6522_tick(void* chip, bus_state_t bus_state) {
    mos6522_t* via = (mos6522_t*)chip;
    if (!via) return bus_state;

    // Timer 1 processing
    if (via->timer1_running) {
        via->timer1_counter--;
        if (via->timer1_counter == 0) {
            // Timer 1 underflow
            via->ifr |= MOS6522_IFR_T1;

            if (via->timer1_continuous) {
                // Reload timer in continuous mode
                via->timer1_counter = via->timer1_latch;
            } else {
                // Stop timer in one-shot mode
                via->timer1_running = false;
            }
        }
    }

    // Timer 2 processing
    if (via->timer2_running) {
        via->timer2_counter--;
        if (via->timer2_counter == 0) {
            // Timer 2 underflow
            via->ifr |= MOS6522_IFR_T2;

            if (via->timer2_continuous) {
                // Reload timer in continuous mode
                via->timer2_counter = via->timer2_latch;
            } else {
                // Stop timer in one-shot mode
                via->timer2_running = false;
            }
        }
    }

    // Interrupt processing
    if ((via->ifr & via->ier) && !(via->ifr & MOS6522_IFR_IRQ)) {
        via->ifr |= MOS6522_IFR_IRQ;
        // Set interrupt line if configured
        if (via->interrupt_line > 0) {
            BUS_SET_LINES(bus_state, BUS_GET_LINES(bus_state) | via->interrupt_line);
        }
    }

    // Keyboard matrix update (called every tick for responsive keyboard scanning)
    mos6522_update_keyboard_matrix(via);

    return bus_state;
}

// GUI functions (stub implementations)
#ifdef IMGUI_VERSION
void mos6522_render_debug_window(void* chip, bool* show_window) {
    // TODO: Implement GUI debug window
}

void mos6522_render_settings_window(void* chip, bool* show_window) {
    // TODO: Implement GUI settings window
}
#endif