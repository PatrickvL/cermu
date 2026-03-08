#pragma once
/*
 * i8255.h — Intel 8255 PPI (Programmable Peripheral Interface)
 *
 * The 8255 PPI provides three 8-bit I/O ports (A, B, C) with programmable
 * direction control.  Port C can be split into upper/lower nibbles with
 * independent direction.
 *
 * Modes:
 *   Mode 0: Basic input/output (no handshaking)
 *   Mode 1: Strobed I/O (handshaking on Ports A/B, Port C provides status)
 *   Mode 2: Bidirectional (Port A only, Port C provides status)
 *
 * Used in: Amstrad CPC (keyboard, AY interface), KC85/4 (I/O),
 *          MSX, PC (original keyboard/timer), many others.
 *
 * 40-pin DIP package.
 */

#include "../../core/chip.h"
#include "../../core/system_lines.h"
#include <cstdint>
#include <cstring>

class i8255_t : public ChipBase {
public:
    i8255_t()
        : ChipBase(ChipInfo("8255", "Intel"))
    {
        category_ = "I/O";
    }

    void init() {
        control_ = 0x9B;  // Mode 0, all ports input (power-on default)
        port_a_out_ = 0x00;
        port_b_out_ = 0x00;
        port_c_out_ = 0x00;
        port_a_in_ = 0xFF;
        port_b_in_ = 0xFF;
        port_c_in_ = 0xFF;
    }

    void reset() { init(); }

    // === Register access (directly through I/O address decoding) ===
    //   A1 A0  Register
    //   0  0   Port A
    //   0  1   Port B
    //   1  0   Port C
    //   1  1   Control

    void write(uint8_t addr, uint8_t data) {
        switch (addr & 0x03) {
        case 0: port_a_out_ = data; break;
        case 1: port_b_out_ = data; break;
        case 2: port_c_out_ = data; break;
        case 3:
            if (data & 0x80) {
                // Mode set (bit 7 = 1)
                control_ = data;
                // Reset output latches
                port_a_out_ = 0x00;
                port_b_out_ = 0x00;
                port_c_out_ = 0x00;
            } else {
                // Bit set/reset on Port C (bit 7 = 0)
                uint8_t bit = (data >> 1) & 0x07;
                if (data & 0x01)
                    port_c_out_ |= (1 << bit);
                else
                    port_c_out_ &= ~(1 << bit);
            }
            break;
        }
    }

    uint8_t read(uint8_t addr) const {
        switch (addr & 0x03) {
        case 0: return port_a_input() ? port_a_in_ : port_a_out_;
        case 1: return port_b_input() ? port_b_in_ : port_b_out_;
        case 2: return read_port_c();
        case 3: return control_;  // Control register read-back
        }
        return 0xFF;
    }

    // === External port inputs from system/devices ===

    void set_port_a_input(uint8_t data) { port_a_in_ = data; }
    void set_port_b_input(uint8_t data) { port_b_in_ = data; }
    void set_port_c_input(uint8_t data) { port_c_in_ = data; }

    uint8_t get_port_a_output() const { return port_a_out_; }
    uint8_t get_port_b_output() const { return port_b_out_; }
    uint8_t get_port_c_output() const { return port_c_out_; }

    // === Direction queries ===
    bool port_a_input() const { return (control_ & 0x10) != 0; }
    bool port_b_input() const { return (control_ & 0x02) != 0; }
    bool port_c_upper_input() const { return (control_ & 0x08) != 0; }
    bool port_c_lower_input() const { return (control_ & 0x01) != 0; }

private:
    uint8_t read_port_c() const {
        uint8_t result = 0;
        // Upper nibble
        if (port_c_upper_input())
            result |= (port_c_in_ & 0xF0);
        else
            result |= (port_c_out_ & 0xF0);
        // Lower nibble
        if (port_c_lower_input())
            result |= (port_c_in_ & 0x0F);
        else
            result |= (port_c_out_ & 0x0F);
        return result;
    }

    uint8_t control_ = 0x9B;
    uint8_t port_a_out_ = 0x00;
    uint8_t port_b_out_ = 0x00;
    uint8_t port_c_out_ = 0x00;
    uint8_t port_a_in_ = 0xFF;
    uint8_t port_b_in_ = 0xFF;
    uint8_t port_c_in_ = 0xFF;
};
