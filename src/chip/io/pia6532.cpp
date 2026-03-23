/*
 * pia6532.cpp — MOS 6532 RIOT Implementation
 *
 * Timer, I/O ports, and 128 bytes of RAM.
 */

#include "chip/io/pia6532.hpp"
#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"
#include <cstring>

void pia6532_t::init() {
    reset();
}

void pia6532_t::reset() {
    memset(ram, 0, sizeof(ram));
    regs_.clear();

    port_a_input = 0xFF;
    port_b_input = 0xFF;

    timer_value   = 0xFF;
    timer_divider = 1024;
    timer_counter = 1024;
    timer_underflow = false;
    timer_interrupt_enabled = false;
}

// ============================================================================
// Timer Tick — called once per CPU clock
// ============================================================================

void pia6532_t::tick() {
    if (timer_counter > 0) {
        timer_counter--;
    }

    if (timer_counter == 0) {
        if (timer_value == 0) {
            // Timer underflow — set flag, switch to divide-by-1 countdown
            timer_underflow = true;
            timer_value = 0xFF;
            timer_divider = 1;
            timer_counter = 1;
        } else {
            timer_value--;
            timer_counter = timer_divider;
        }
    }
}

// ============================================================================
// Port Reads (with data direction masking)
// ============================================================================

uint8_t pia6532_t::read_port_a() const {
    // Output bits from output latch, input bits from external input
    return (port_a_data & port_a_ddr) | (port_a_input & ~port_a_ddr);
}

uint8_t pia6532_t::read_port_b() const {
    return (port_b_data & port_b_ddr) | (port_b_input & ~port_b_ddr);
}

// ============================================================================
// I/O Register Read
// ============================================================================
// Addresses are partially decoded:
//   A2=0: Port A regs, A2=1: Port B regs
//   A0=0: Data, A0=1: DDR
//   A0=0 with A2,A1 high bits: INTIM/INSTAT
//
// In the Atari 2600 the RIOT I/O registers are at $0280-$02FF (A9=1).
// The address bits used: A4-A0 (5 bits significant for I/O).
//
// Read addresses:
//   $0280: SWCHA (Port A data)
//   $0281: SWACNT (Port A DDR)
//   $0282: SWCHB (Port B data)
//   $0283: SWBCNT (Port B DDR)
//   $0284: INTIM (timer value)
//   $0285: INSTAT (timer interrupt status, bit 7)

uint8_t pia6532_t::read_io(uint16_t addr) {
    uint8_t a = addr & 0x07;  // Low 3 bits for I/O decoding

    // Timer reads take priority when A2 is set (addresses $04-$07)
    if (addr & 0x04) {
        if (a == 0x04) {
            // INTIM — read timer value, clear underflow flag
            timer_underflow = false;
            return timer_value;
        }
        if (a == 0x05) {
            // INSTAT — timer status (bit 7 = underflow occurred)
            uint8_t status = timer_underflow ? 0x80 : 0x00;
            return status;
        }
    }

    // Port registers (A2=0)
    switch (a & 0x03) {
        case 0x00: return read_port_a();    // SWCHA
        case 0x01: return port_a_ddr;       // SWACNT
        case 0x02: return read_port_b();    // SWCHB
        case 0x03: return port_b_ddr;       // SWBCNT
    }

    return 0xFF;
}

// ============================================================================
// I/O Register Write
// ============================================================================
// Write addresses:
//   $0280: SWCHA (Port A data)
//   $0281: SWACNT (Port A DDR)
//   $0282: SWCHB (Port B data)
//   $0283: SWBCNT (Port B DDR)
//   $0294: TIM1T (set timer, divide by 1)
//   $0295: TIM8T (set timer, divide by 8)
//   $0296: TIM64T (set timer, divide by 64)
//   $0297: TIM1024T (set timer, divide by 1024)

void pia6532_t::write_io(uint16_t addr, uint8_t data) {
    // Timer writes: A4=1, A3=x
    if (addr & 0x10) {
        // Set timer — divider from address bits A1:A0
        uint8_t divider_sel = addr & 0x03;
        static const uint16_t dividers[4] = { 1, 8, 64, 1024 };
        timer_divider = dividers[divider_sel];
        timer_value   = data;
        timer_counter = timer_divider;
        timer_underflow = false;
        timer_interrupt_enabled = (addr & 0x08) != 0;  // A3 enables interrupt
        return;
    }

    // Port registers (A4=0)
    uint8_t a = addr & 0x03;
    switch (a) {
        case 0x00: port_a_data = data; break;   // SWCHA
        case 0x01: port_a_ddr  = data; break;   // SWACNT
        case 0x02: port_b_data = data; break;   // SWCHB
        case 0x03: port_b_ddr  = data; break;   // SWBCNT
    }
}

REGISTER_CHIP_TYPE("PIA6532", pia6532_t)
