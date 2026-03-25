#pragma once

// ============================================================================
// MOS 6529B — Single 8-Bit Bidirectional I/O Port
// ============================================================================
//
// The MOS 6529B is an extremely simple 8-bit I/O port used in the Commodore
// 264 series (C16, C116, Plus/4).  It has no data direction register — all
// 8 pins are permanently bidirectional with active pull-ups.
//
// Writing stores a byte to the output latch.  Reading returns the effective
// pin state: the latch output ANDed with any external pull (active-low wired
// devices can pull individual bits low even if the latch drives them high).
//
// In the C264 series:
//   PIO1 ($FD10):  Connected to the user port (Plus/4 only) + cassette tape
//                  sense on bit 2.  C16/C116 have no physical 6529 here.
//   PIO2 ($FD30):  Keyboard row select (active-low, directly latched).
//
// References: MOS 6529B datasheet, VICE plus4pio1.c / plus4pio2.c

#include "chip/io/io_chip_base.hpp"

struct mos6529_t : public IoChipBase {

    mos6529_t() : IoChipBase(ChipInfo{"6529B", "MOS Technology", "MOS 6529B"}) {}

    // ── ChipBase MMIO interface ─────────────────────────────────────────

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        BUS_SET_DATA(bus, output_latch & external_pins);
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        output_latch = BUS_GET_DATA(bus);
        return bus;
    }

    void reset() override {
        output_latch  = 0xFF;
        external_pins = 0xFF;
    }

    // ── Public state ────────────────────────────────────────────────────

    uint8_t output_latch  = 0xFF;   // Written by CPU, driven onto port pins
    uint8_t external_pins = 0xFF;   // Externally pulled pin state (ANDed with latch on read)
};
