#include "chip/io/pia6820.hpp"
#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"
#include <cstring>

// PIA 6820 Register offsets
#define A_DATA      0x00
#define A_CONTROL   0x01
#define B_DATA      0x02
#define B_CONTROL   0x03

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

void pia6820_t::init() {
    // Zero all registers (data, DDR, control for both ports)
    regs_.clear();

    irq_a1 = false;
    irq_a2 = false;
    irq_b1 = false;
    irq_b2 = false;

    ca1_state = false;
    ca2_state = false;
    cb1_state = false;
    cb2_state = false;

    user_data = nullptr;
    on_port_a_read = nullptr;
    on_port_a_write = nullptr;
    on_port_b_read = nullptr;
    on_port_b_write = nullptr;
    on_irq_a = nullptr;
    on_irq_b = nullptr;
    on_ca2_output = nullptr;
    on_cb2_output = nullptr;
}

void pia6820_t::reset() {
    // Reset all registers to power-on state
    regs_.clear();

    // Clear all interrupt flags
    irq_a1 = false;
    irq_a2 = false;
    irq_b1 = false;
    irq_b2 = false;

    // Reset control line states
    ca1_state = false;
    ca2_state = false;
    cb1_state = false;
    cb2_state = false;

    update_irq();
}

uint8_t pia6820_t::read(uint16_t addr) {
    uint8_t reg = addr & 0x03;
    switch (reg) {
        case A_DATA: {
            // If DDR select bit is clear, return DDR
            if (DDR_SELECTED(port_a_control)) {
                return port_a_direction;
            }

            // Reading port A clears CA1 and CA2 interrupt flags
            irq_a1 = false;
            irq_a2 = false;
            update_irq();

            // Read port data with direction control
            uint8_t data = read_port_with_direction(
                port_a_data,
                port_a_direction,
                on_port_a_read,
                user_data
            );

            return data;
        }

        case A_CONTROL: {
            // Bits 7-6: IRQ flags (read-only), Bits 5-0: Control bits
            uint8_t value = port_a_control & 0x3F;
            if (irq_a1) value |= PIA_CTRL_IRQ1;
            if (irq_a2) value |= PIA_CTRL_IRQ2;
            return value;
        }

        case B_DATA: {
            // If DDR select bit is clear, return DDR
            if (DDR_SELECTED(port_b_control)) {
                return port_b_direction;
            }

            // Reading port B clears CB1 and CB2 interrupt flags
            irq_b1 = false;
            irq_b2 = false;
            update_irq();

            // Read port data with direction control
            return read_port_with_direction(
                port_b_data,
                port_b_direction,
                on_port_b_read,
                user_data
            );
        }

        case B_CONTROL: {
            // Bits 7-6: IRQ flags (read-only), Bits 5-0: Control bits
            uint8_t value = port_b_control & 0x3F;
            if (irq_b1) value |= PIA_CTRL_IRQ1;
            if (irq_b2) value |= PIA_CTRL_IRQ2;
            return value;
        }
    }

    return 0xFF;
}

void pia6820_t::write(uint16_t addr, uint8_t data) {
    uint8_t reg = addr & 0x03;

    switch (reg) {
        case A_DATA:
            // If DDR select bit is clear, write DDR
            if (DDR_SELECTED(port_a_control)) {
                port_a_direction = data;
            } else {
                // Write port data (stored in output register)
                port_a_data = data;

                // Call external write callback for output pins only
                if (on_port_a_write) {
                    uint8_t output = port_a_data & port_a_direction;
                    on_port_a_write(user_data, output);
                }

                // If CA2 is in write strobe mode, pulse it
                if (IS_CA2_OUTPUT(port_a_control) && GET_CA2_MODE(port_a_control) == 0b10) {
                    ca2_state = false;
                    if (on_ca2_output) on_ca2_output(user_data, false);
                    // In real hardware this would go high after one E cycle
                    ca2_state = true;
                    if (on_ca2_output) on_ca2_output(user_data, true);
                }
            }
            break;

        case A_CONTROL:
            port_a_control = data & 0x3F;  // Only bits 0-5 are writable
            update_ca2_output_state();
            update_irq();
            break;

        case B_DATA:
            // If DDR select bit is clear, write DDR
            if (DDR_SELECTED(port_b_control)) {
                port_b_direction = data;
            } else {
                // Write port data
                port_b_data = data;

                // Call external write callback for output pins only
                if (on_port_b_write) {
                    uint8_t output = port_b_data & port_b_direction;
                    on_port_b_write(user_data, output);
                }

                // If CB2 is in write strobe mode, pulse it
                if (IS_CB2_OUTPUT(port_b_control) && GET_CB2_MODE(port_b_control) == 0b10) {
                    cb2_state = false;
                    if (on_cb2_output) on_cb2_output(user_data, false);
                    cb2_state = true;
                    if (on_cb2_output) on_cb2_output(user_data, true);
                }
            }
            break;

        case B_CONTROL:
            port_b_control = data & 0x3F;  // Only bits 0-5 are writable
            update_cb2_output_state();
            update_irq();
            break;
    }
}

// External control line inputs for handshaking and interrupts
void pia6820_t::set_ca1(bool state) {
    bool old_state = ca1_state;
    ca1_state = state;

    // Detect edge based on CRA bit 1 (0=falling, 1=rising)
    bool rising_edge = !old_state && state;
    bool falling_edge = old_state && !state;
    bool trigger = (CA1_RISING_EDGE(port_a_control) && rising_edge) ||
                   (!CA1_RISING_EDGE(port_a_control) && falling_edge);

    if (trigger) {
        irq_a1 = true;
        update_irq();
    }
}

void pia6820_t::set_ca2_input(bool state) {
    if (IS_CA2_OUTPUT(port_a_control)) return;  // Ignore if CA2 is output

    bool old_state = ca2_state;
    ca2_state = state;

    // Detect edge based on CRA bit 4
    bool rising_edge = !old_state && state;
    bool falling_edge = old_state && !state;
    bool trigger = ((port_a_control & PIA_CTRL_CA2_MODE) && rising_edge) ||
                   (!(port_a_control & PIA_CTRL_CA2_MODE) && falling_edge);

    if (trigger) {
        irq_a2 = true;
        update_irq();
    }
}

void pia6820_t::set_cb1(bool state) {
    bool old_state = cb1_state;
    cb1_state = state;

    // Detect edge
    bool rising_edge = !old_state && state;
    bool falling_edge = old_state && !state;
    bool trigger = (CB1_RISING_EDGE(port_b_control) && rising_edge) ||
                   (!CB1_RISING_EDGE(port_b_control) && falling_edge);

    if (trigger) {
        irq_b1 = true;
        update_irq();

        // CB1 active transition also affects CB2 in some output modes
        if (IS_CB2_OUTPUT(port_b_control)) {
            update_cb2_output_state();
        }
    }
}

void pia6820_t::set_cb2_input(bool state) {
    if (IS_CB2_OUTPUT(port_b_control)) return;  // Ignore if CB2 is output

    bool old_state = cb2_state;
    cb2_state = state;

    // Detect edge
    bool rising_edge = !old_state && state;
    bool falling_edge = old_state && !state;
    bool trigger = ((port_b_control & PIA_CTRL_CA2_MODE) && rising_edge) ||
                   (!(port_b_control & PIA_CTRL_CA2_MODE) && falling_edge);

    if (trigger) {
        irq_b2 = true;
        update_irq();
    }
}

// Direct port input functions (for external hardware simulation)
void pia6820_t::set_port_a_input(uint8_t value) {
    port_a_data = value;
}

void pia6820_t::set_port_b_input(uint8_t value) {
    port_b_data = value;
}

// Internal helper methods
void pia6820_t::update_irq() {
    // IRQA is asserted if either IRQA1 or IRQA2 is set and enabled
    bool irqa = (irq_a1 && IRQ1_ENABLED(port_a_control)) ||
                (irq_a2 && IRQ2_ENABLED(port_a_control));

    bool irqb = (irq_b1 && IRQ1_ENABLED(port_b_control)) ||
                (irq_b2 && IRQ2_ENABLED(port_b_control));

    if (on_irq_a) {
        on_irq_a(user_data, irqa);
    }

    if (on_irq_b) {
        on_irq_b(user_data, irqb);
    }
}

void pia6820_t::update_ca2_output_state() {
    if (!IS_CA2_OUTPUT(port_a_control)) return;

    uint8_t mode = GET_CA2_MODE(port_a_control);
    bool new_state = false;

    switch (mode) {
        case 0b00:  // Set low on CA1 active transition, reset high on read Port A
        case 0b01:  // Set low on CA1 active transition, reset high on E pulse after read
            new_state = ca2_state;
            break;

        case 0b10:  // Set low on write Port A, reset high on E pulse after write
            new_state = ca2_state;
            break;

        case 0b11:  // Manual output - controlled by bit 3 of CRA
            new_state = port_a_control & PIA_CTRL_CA2_CTRL;
            break;
    }

    if (new_state != ca2_state) {
        ca2_state = new_state;
        if (on_ca2_output) {
            on_ca2_output(user_data, new_state);
        }
    }
}

void pia6820_t::update_cb2_output_state() {
    if (!IS_CB2_OUTPUT(port_b_control)) return;

    uint8_t mode = GET_CB2_MODE(port_b_control);
    bool new_state = false;

    switch (mode) {
        case 0b00:  // Set low on CB1 active transition, reset high on read Port B
        case 0b01:  // Set low on CB1 active transition, reset high on E pulse after read
            new_state = cb2_state;
            break;

        case 0b10:  // Set low on write Port B, reset high on E pulse after write
            new_state = cb2_state;
            break;

        case 0b11:  // Manual output
            new_state = port_b_control & PIA_CTRL_CA2_CTRL;
            break;
    }

    if (new_state != cb2_state) {
        cb2_state = new_state;
        if (on_cb2_output) {
            on_cb2_output(user_data, new_state);
        }
    }
}

uint8_t pia6820_t::read_port_with_direction(uint8_t output_reg, uint8_t ddr,
                                             uint8_t (*read_cb)(void*), void* ud) {
    uint8_t result = 0x00;

    if (read_cb) {
        uint8_t input = read_cb(ud);

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

REGISTER_CHIP_TYPE("PIA6820", pia6820_t)
REGISTER_CHIP_TYPE("PIA6821", pia6821_t)
REGISTER_CHIP_TYPE("MOS6520", mos6520_t)
