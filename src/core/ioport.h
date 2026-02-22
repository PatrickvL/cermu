#pragma once

/*
 * ioport.h — Generic 8-bit bidirectional I/O port with data direction control
 *
 * Template parameter Mask is the compile-time bitmask of physically connected
 * pins.  Bits outside Mask have no physical pin — their pin state is frozen
 * at the value set by reset() (HIGH after default-reset) and is never
 * modified by DDR/data writes.
 *
 * The port does NOT own storage.  It holds pointers to three externally-owned
 * uint8_t locations (DDR, data, pins) so the owning chip retains direct
 * access to the registers — e.g. CIA reg[] array, CPU io_port struct.
 * An io_port_state helper is provided for chips that want a convenient
 * locally-owned triple.
 *
 * Chip-specific extensions (banking callbacks, timer output overrides,
 * read callbacks) live in the owning chip.  This type handles only the
 * DDR / data / pins mechanics.
 *
 * Consumers:
 *   CPU port  — io_port<0x3F> (6510), io_port<0x5F> (7501), io_port<0x7F> (8502)
 *   CIA PA/PB — io_port<0xFF>
 *   VIA PA/PB — io_port<0xFF>
 *
 * All merge operations use bitmix(a, b, mask) from cermu.h:
 *   bitmix(a, b, m) = select a where m=1, b where m=0
 */

#include "cermu.h"
#include <cstdint>

// ---------------------------------------------------------------------------
// Convenience storage for chips that want to embed the three registers locally
// (CPU mixin, VIA).  CIA can point io_port at its existing reg[] entries instead.
// ---------------------------------------------------------------------------
struct io_port_state {
    uint8_t ddr  = 0x00;   // Data Direction Register (1 = output, 0 = input)
    uint8_t data = 0x00;   // Output data register
    uint8_t pins = 0xFF;   // Physical pin state (pull-ups default HIGH)
    uint8_t _pad = 0x00;   // Align to 4 bytes
};

// ---------------------------------------------------------------------------
// io_port<Mask> — non-owning view over DDR / data / pins
// ---------------------------------------------------------------------------
template <uint8_t Mask = 0xFF>
struct io_port {
    uint8_t* const ddr;     // → Data Direction Register (1 = output, 0 = input)
    uint8_t* const data;    // → Output data register
    uint8_t* const pins;    // → Physical pin state

    static constexpr uint8_t mask = Mask;

    // -- construction (from individual refs or from io_port_state) -----------

    io_port(uint8_t& ddr_, uint8_t& data_, uint8_t& pins_)
        : ddr(&ddr_), data(&data_), pins(&pins_) {}

    io_port(io_port_state& s)
        : ddr(&s.ddr), data(&s.data), pins(&s.pins) {}

    // -- lifecycle ----------------------------------------------------------

    /// Reset DDR, data, and pins to initial values.
    /// Unconnected pins (outside Mask) are forced HIGH.
    void reset(uint8_t init_ddr  = 0x00,
               uint8_t init_data = 0x00,
               uint8_t init_pins = 0xFF) {
        *ddr  = init_ddr;
        *data = init_data;
        *pins = init_pins | static_cast<uint8_t>(~Mask);
    }

    // -- bus-facing operations ----------------------------------------------

    /// Read the port ($01 / PRA / PRB).
    ///   DDR = 1 → data register  (output bits)
    ///   DDR = 0 → pin state      (input bits)
    [[nodiscard]] uint8_t read() const {
        return bitmix(*data, *pins, *ddr);
    }

    /// Write DDR register ($00 / DDRA / DDRB).
    /// - Bits changing output → input : pins pulled HIGH  (pull-up)
    /// - Bits changing input  → output: pins driven from data register
    /// Returns: bitmask of direction bits that actually changed.
    [[nodiscard]] uint8_t write_ddr(uint8_t value) {
        const uint8_t old = *ddr;
        *ddr = value;

        // Pull up pins that transition from output → input (physical pins only)
        *pins |= (old & ~value & Mask);

        // Drive data onto pins that transition from input → output
        const uint8_t new_outputs = static_cast<uint8_t>(~old & value & Mask);
        *pins = bitmix(*data, *pins, new_outputs);

        return static_cast<uint8_t>(old ^ value);
    }

    /// Write data register ($01 / PRA / PRB).
    /// Drives new value onto output pins (DDR = 1, within Mask).
    /// Returns: bitmask of output bits whose driven value changed.
    [[nodiscard]] uint8_t write_data(uint8_t value) {
        const uint8_t old = *data;
        *data = value;

        // Update pin state for output bits with physical pins
        const uint8_t out = static_cast<uint8_t>(*ddr & Mask);
        *pins = bitmix(value, *pins, out);

        return static_cast<uint8_t>((old ^ value) & out);
    }

    // -- external device interface ------------------------------------------

    /// Get effective output value visible to external hardware.
    [[nodiscard]] uint8_t output() const {
        return static_cast<uint8_t>(*data & *ddr & Mask);
    }

    /// Set external input pins (bulk replace of input-bit pin state).
    /// Only affects input bits (DDR = 0) within Mask.
    /// Output bits and unconnected bits are preserved.
    void set_input(uint8_t value) {
        const uint8_t in = static_cast<uint8_t>(~*ddr & Mask);
        *pins = bitmix(value, *pins, in);
    }

    /// Pull specific input pins LOW  (open-collector: keyboard, joystick).
    /// Only affects input bits within Mask.
    void pull_low(uint8_t bits) {
        *pins &= ~(bits & ~*ddr & Mask);
    }

    /// Release specific input pins to pull-up HIGH (device released).
    /// Only affects input bits within Mask.
    void release(uint8_t bits) {
        *pins |= (bits & ~*ddr & Mask);
    }

    // -- direct pin manipulation (for chip-specific overrides) --------------

    /// Force specific pin bits to a value regardless of DDR.
    /// Used by CIA timer output on PB6/PB7 (PBON override).
    void force_pins(uint8_t value, uint8_t force_mask) {
        *pins = bitmix(value, *pins, force_mask);
    }
};
