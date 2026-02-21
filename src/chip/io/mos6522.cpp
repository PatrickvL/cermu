#include "mos6522.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// MOS6522 VIA chip descriptor
chip_descriptor_t mos6522_descriptor = {
    .description = "MOS6522 VIA Versatile Interface Adapter",
    .create = [](chip_descriptor_t*) -> void* { return mos6522_create(); },
    .destroy = [](void* chip) { mos6522_destroy(static_cast<mos6522_t*>(chip)); },
    .bus_attach = (void (*)(void *, void *))mos6522_bus_attach
};

mos6522_t* mos6522_create() {
    mos6522_t* via = (mos6522_t*)calloc(1, sizeof(mos6522_t));
    if (!via) return NULL;

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

void mos6522_destroy(mos6522_t* via) {
    if (!via) return;
    free(via);
}

void mos6522_set_port_a_read_callback(mos6522_t* via, uint8_t (*callback)(void*, uint8_t), void* context) {
    if (!via) return;
    via->port_a_read_callback = callback;
    via->port_a_read_context = context;
}

void mos6522_set_port_b_read_callback(mos6522_t* via, uint8_t (*callback)(void*, uint8_t), void* context) {
    if (!via) return;
    via->port_b_read_callback = callback;
    via->port_b_read_context = context;
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

    // Reset interrupt state (preserves interrupt_line — that's hardware wiring config)
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
        case MOS6522_PORTB: {
            // 6522 Port B read: output pins (DDR=1) return ORA, input pins (DDR=0) return pin state
            uint8_t pb_pin_state = 0xFF;  // Default: all pins pulled HIGH (no external device)
            if (via->port_b_read_callback) {
                uint8_t port_b_output = via->port_b_data & via->port_b_ddr;
                pb_pin_state = via->port_b_read_callback(via->port_b_read_context, port_b_output);
            }
            BUS_SET_DATA(bus_state, (via->port_b_data & via->port_b_ddr) | (pb_pin_state & ~via->port_b_ddr));
            break;
        }
        case MOS6522_PORTA: {
            // 6522 Port A read: output pins (DDR=1) return ORA, input pins (DDR=0) return pin state
            uint8_t pa_pin_state = 0xFF;  // Default: all pins pulled HIGH (no external device)
            if (via->port_a_read_callback) {
                uint8_t port_a_output = via->port_a_data & via->port_a_ddr;
                pa_pin_state = via->port_a_read_callback(via->port_a_read_context, port_a_output);
            }
            BUS_SET_DATA(bus_state, (via->port_a_data & via->port_a_ddr) | (pa_pin_state & ~via->port_a_ddr));
            break;
        }
        case MOS6522_DDRB:
            BUS_SET_DATA(bus_state, via->port_b_ddr);
            break;
        case MOS6522_DDRA:
            BUS_SET_DATA(bus_state, via->port_a_ddr);
            break;
        case MOS6522_T1CL:
            BUS_SET_DATA(bus_state, (uint8_t)(via->timer1_counter & 0xFF));
            // Reading T1CL clears Timer 1 interrupt flag (per 6522 datasheet)
            via->ifr &= ~MOS6522_IFR_T1;
            if (!(via->ifr & via->ier & 0x7F)) {
                via->ifr &= ~MOS6522_IFR_IRQ;
            }
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
            // Reading T2CL clears Timer 2 interrupt flag (per 6522 datasheet)
            via->ifr &= ~MOS6522_IFR_T2;
            if (!(via->ifr & via->ier & 0x7F)) {
                via->ifr &= ~MOS6522_IFR_IRQ;
            }
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
            // Reading IFR returns current flags - does NOT clear them
            // (Flags are cleared by reading T1CL/T2CL or writing to IFR)
            BUS_SET_DATA(bus_state, via->ifr);
            break;
        case MOS6522_IER:
            // Reading IER returns enable bits with bit 7 always set (per 6522 datasheet)
            BUS_SET_DATA(bus_state, via->ier | 0x80);
            break;
        case MOS6522_PORTA_NH: {
            // Port A read without handshake - same logic as MOS6522_PORTA
            uint8_t pa_nh_pin_state = 0xFF;  // Default: all pins pulled HIGH
            if (via->port_a_read_callback) {
                uint8_t port_a_output = via->port_a_data & via->port_a_ddr;
                pa_nh_pin_state = via->port_a_read_callback(via->port_a_read_context, port_a_output);
            }
            BUS_SET_DATA(bus_state, (via->port_a_data & via->port_a_ddr) | (pa_nh_pin_state & ~via->port_a_ddr));
            break;
        }
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
            // Write to T1 Low Latch only (does not affect counter or start timer)
            via->timer1_latch = (via->timer1_latch & 0xFF00) | value;
            break;
        case MOS6522_T1CH:
            // Write to T1 Counter High (register 0x05):
            // 1. Load latch high byte
            // 2. Transfer latch to counter
            // 3. START the timer (this is the key missing piece!)
            // 4. Clear T1 interrupt flag
            via->timer1_latch = (via->timer1_latch & 0x00FF) | (value << 8);
            via->timer1_counter = via->timer1_latch;
            via->timer1_running = true;
            via->ifr &= ~MOS6522_IFR_T1;  // Clear T1 interrupt flag
            // If IRQ master bit was only set due to T1, clear it
            if (!(via->ifr & via->ier & 0x7F)) {
                via->ifr &= ~MOS6522_IFR_IRQ;
            }
            break;
        case MOS6522_T1LH:
            // Write to T1 Latch High only (does not start timer)
            via->timer1_latch = (via->timer1_latch & 0x00FF) | (value << 8);
            // NOTE: Unlike T1CH, writing T1LH does NOT load counter or start timer
            // It only updates the latch for the next reload
            break;
        case MOS6522_T2CL: // 0x08 - T2 Low Latch (write) / Counter Low (read)
            // Write to T2 Low Latch only
            via->timer2_latch = (via->timer2_latch & 0xFF00) | value;
            break;
        case MOS6522_T2CH: // 0x09 - T2 Counter High (write starts timer)
            // Write to T2 Counter High:
            // 1. Load latch high byte (low byte was already loaded)
            // 2. Transfer latch to counter
            // 3. START the timer
            // 4. Clear T2 interrupt flag
            via->timer2_latch = (via->timer2_latch & 0x00FF) | (value << 8);
            via->timer2_counter = via->timer2_latch;
            via->timer2_running = true;
            via->ifr &= ~MOS6522_IFR_T2;  // Clear T2 interrupt flag
            // If IRQ master bit was only set due to T2, clear it
            if (!(via->ifr & via->ier & 0x7F)) {
                via->ifr &= ~MOS6522_IFR_IRQ;
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
            // Writing to IFR: bit 7 is set/clear control for lower 7 bits
            // Writing with bit 7=0: clear the specified bits in lower 7
            // (Bit 7 of IFR cannot be directly set/cleared - it's computed)
            via->ifr &= ~(value & 0x7F);
            // Recalculate master IRQ bit (bit 7)
            if (via->ifr & via->ier & 0x7F) {
                via->ifr |= MOS6522_IFR_IRQ;
            } else {
                via->ifr &= ~MOS6522_IFR_IRQ;
            }
            break;
        case MOS6522_IER:
            // IER bit 7 is set/clear control:
            // Bit 7=1: SET the specified lower 7 bits (enable interrupts)
            // Bit 7=0: CLEAR the specified lower 7 bits (disable interrupts)
            if (value & 0x80) {
                via->ier |= (value & 0x7F);   // Set specified bits
            } else {
                via->ier &= ~(value & 0x7F);  // Clear specified bits
            }
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

    // Read timer modes from ACR (Auxiliary Control Register)
    // ACR bit 6: Timer 1 control - 0=one-shot, 1=continuous (free-running)
    // ACR bit 5: Timer 2 control - 0=timed interrupt, 1=count pulses on PB6
    bool timer1_continuous = (via->acr & MOS6522_ACR_T1_CONT) != 0;
    bool timer2_pulse_count_mode = (via->acr & MOS6522_ACR_T2_CONT) != 0;  // T2 in pulse counting mode, not used here

    // Timer 1 processing
    if (via->timer1_running) {
        // Decrement counter first
        uint16_t old_counter = via->timer1_counter;
        via->timer1_counter--;
        
        // Check for underflow: counter wrapped from 0x0000 to 0xFFFF
        // This happens when old_counter was 0
        if (old_counter == 0) {
            // Timer 1 underflow - set interrupt flag
            via->ifr |= MOS6522_IFR_T1;

            if (timer1_continuous) {
                // Free-running mode (ACR bit 6 = 1): Reload from latch and keep running
                via->timer1_counter = via->timer1_latch;
            } else {
                // One-shot mode (ACR bit 6 = 0): Keep counting down (wraps to 0xFFFF)
                // Timer keeps running but only generates one interrupt per T1CH write
                // The timer doesn't stop - it just doesn't reload from latch
            }
        }
    }

    // Timer 2 processing (always one-shot mode in timed interrupt mode)
    if (via->timer2_running && !timer2_pulse_count_mode) {
        // Decrement counter first
        uint16_t old_counter = via->timer2_counter;
        via->timer2_counter--;
        
        // Check for underflow: counter wrapped from 0x0000 to 0xFFFF
        if (old_counter == 0) {
            // Timer 2 underflow - set interrupt flag
            via->ifr |= MOS6522_IFR_T2;
            
            // Timer 2 is always one-shot: stop after underflow
            via->timer2_running = false;
        }
    }

    // Interrupt processing - CONTINUOUS ASSERTION
    // The VIA continuously asserts IRQ as long as any enabled interrupt flag is set
    // Check if any enabled interrupt is active (bits 0-6 of IFR AND IER)
    if (via->ifr & via->ier & 0x7F) {
        // Set the master IRQ bit in IFR
        via->ifr |= MOS6522_IFR_IRQ;
        
        // Assert interrupt line (active-low, so we set the legacy mask bit)
        // This must happen EVERY tick, not just once, because the bus state is
        // reset to default at the start of each cycle
        if (via->interrupt_line > 0) {
            BUS_SET_LINES(bus_state, BUS_GET_LINES(bus_state) | via->interrupt_line);
        }
    } else {
        // Clear the master IRQ bit if no enabled interrupts are active
        via->ifr &= ~MOS6522_IFR_IRQ;
    }

    return bus_state;
}
