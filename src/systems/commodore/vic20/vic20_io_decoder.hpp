#pragma once
// ============================================================================
// vic20_io_decoder.hpp — VIC-20 I/O Address Decoder ($9000–$93FF)
// ============================================================================
//
// Models the discrete address decoding logic on the VIC-20 motherboard
// that routes the $9000–$93FF I/O block to one of three chips based on
// address bits A5:A4 within each 64-byte mirror group:
//
//   A5:A4 = 00 → VIC (MOS 6560/6561)     offsets $x00–$x0F
//   A5:A4 = 01 → VIA 1 (MOS 6522)        offsets $x10–$x1F
//   A5:A4 = 10 → VIA 2 (MOS 6522)        offsets $x20–$x2F
//   A5:A4 = 11 → VIC (mirror)            offsets $x30–$x3F
//
// The real hardware uses a 74LS138 (or equivalent gate logic) to decode
// A5:A4 and generate individual chip-select signals.  Address bits A6–A9
// are ignored by the decoder (they're don't-care for the I/O chips),
// producing the characteristic 64-byte mirroring across the 1 KB range.
//
// This chip appears as a single MMIO entry in the VIC-20 manifest,
// occupying the $9000–$93FF page.  The bus resolver sets CS to this
// decoder's chip ID; the decoder's tick() then routes to the correct
// child chip's registers_read/registers_write.
//
// System-specific — lives under src/systems/commodore/vic20/.
// ============================================================================

#include "core/chip.hpp"
#include "core/system_lines.hpp"
#include "chip/logic/ls138.hpp"
#include "chip/video/vic/vic_common.hpp"
#include "chip/io/mos6522.hpp"

class vic20_io_decoder_t : public ChipBase {
public:
    vic20_io_decoder_t()
        : ChipBase(ChipInfo{"VIC-20 I/O Decoder", "Commodore",
                            "Address decoder for $9000-$93FF I/O block"}) {
        category_ = "Logic";
    }

    // ── Wiring — called once during system initialization ───────────────

    void wire(vic_base_t* vic, mos6522_t* via1, mos6522_t* via2) {
        vic_  = vic;
        via1_ = via1;
        via2_ = via2;
    }

    // ── ChipBase MMIO interface ─────────────────────────────────────────

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        return dispatch_read(bus);
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        return dispatch_write(bus);
    }

    // ── CS-tick dispatch ────────────────────────────────────────────────

    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? dispatch_read(bus) : dispatch_write(bus);
            mark_cs_serviced(bus);
        }
        return bus;
    }

    void reset() override {
        decoder_.reset();
    }

    // ── Direct access to the 74LS138 (for debug/GUI) ────────────────────

    const LS138& decoder() const { return decoder_; }

private:
    // ── Address decode: A5:A4 selects chip, A3:A0 selects register ──────
    //
    // The 74LS138 decodes A5:A4 (only 2 bits used → 4 of 8 outputs):
    //   Y0 (A5:A4 = 00) → VIC chip select
    //   Y1 (A5:A4 = 01) → VIA 1 chip select
    //   Y2 (A5:A4 = 10) → VIA 2 chip select
    //   Y3 (A5:A4 = 11) → VIC chip select (mirror)
    //
    // A6–A9 are don't-cares — the decoder only looks at A5:A4.

    enum ChipSelect : uint8_t {
        CS_VIC  = 0,   // Y0: A5:A4 = 00
        CS_VIA1 = 1,   // Y1: A5:A4 = 01
        CS_VIA2 = 2,   // Y2: A5:A4 = 10
        CS_VIC_MIRROR = 3,  // Y3: A5:A4 = 11 (mirrors VIC)
    };

    uint8_t decode_chip(uint16_t addr) noexcept {
        uint8_t sel = (addr >> 4) & 0x03;  // A5:A4 → bits A, B
        // Pack: A=sel[0], B=sel[1], C=0, _E1=0, _E2=0, E3=1
        // decode_select returns the selected output index (0–3 here)
        return decoder_.decode_select(sel | (1 << 5));
    }

    bus_state_t dispatch_read(bus_state_t bus) noexcept {
        uint8_t sel = decode_chip(BUS_GET_ADDR(bus));
        switch (sel) {
        case CS_VIC:
        case CS_VIC_MIRROR:
            return vic_->on_bus_read(bus);
        case CS_VIA1:
            return via1_->on_bus_read(bus);
        case CS_VIA2:
            return via2_->on_bus_read(bus);
        }
        return bus;
    }

    bus_state_t dispatch_write(bus_state_t bus) noexcept {
        uint8_t sel = decode_chip(BUS_GET_ADDR(bus));
        switch (sel) {
        case CS_VIC:
        case CS_VIC_MIRROR:
            return vic_->on_bus_write(bus);
        case CS_VIA1:
            return via1_->on_bus_write(bus);
        case CS_VIA2:
            return via2_->on_bus_write(bus);
        }
        return bus;
    }

    LS138       decoder_;         // 74LS138 — address decoder
    vic_base_t* vic_  = nullptr;  // MOS 6560/6561 (non-owning)
    mos6522_t*  via1_ = nullptr;  // VIA 1 (non-owning)
    mos6522_t*  via2_ = nullptr;  // VIA 2 (non-owning)
};
