#pragma once
// ============================================================================
// c264_io_decoder.hpp — C264 Series I/O Address Decoder ($FD00–$FDFF)
// ============================================================================
//
// Models the board-level address decoding for the $FD00–$FDFF I/O page
// in the C264 series (C16, C116, Plus/4).
//
// In hardware, the MOS 251641-02 PLA (U19) asserts /IO for the entire
// $FD00–$FDFF page.  A 74LS139 dual 2-to-4 decoder (U20) then uses
// address bits A5:A4 to generate individual chip-selects for the low
// I/O region ($FD0x–$FD3x):
//
//   A5:A4 = 00 → /ACIA ($FD00–$FD0F)  — 6551 ACIA (Plus/4 only)
//   A5:A4 = 01 → /PIO1 ($FD10–$FD1F)  — 6529B user port / tape sense
//   A5:A4 = 10 → (unmapped)           — $FD20–$FD2F returns open bus
//   A5:A4 = 11 → /PIO2 ($FD30–$FD3F)  — 6529B keyboard row select
//
// Separately, additional board logic (AND/NAND gates) decodes the
// $FDD0–$FDDF range and uses it to clock a 74LS175 quad D-flipflop
// (U21) whose outputs drive the ROM chip-select lines:
//
//   A1:A0 → low ROM bank  (0=BASIC, 1=Function LO, 2=Cartridge LO)
//   A3:A2 → high ROM bank (0=KERNAL, 1=Function HI, 2=Cartridge HI)
//
// All other addresses in $FD00–$FDFF are open bus (no chip responds;
// DRAM is suppressed by the PLA across the whole I/O window).
//
// This chip appears as a single MMIO entry in the C264 manifest,
// occupying the $FD00–$FDFF page.  The bus resolver sets CS to this
// decoder's chip ID; the decoder's tick() routes to child chips.
//
// System-specific — lives under src/systems/commodore/c16/.
// ============================================================================

#include "core/chip.hpp"
#include "core/system_lines.hpp"
#include "chip/logic/ls139.hpp"
#include "chip/logic/ls175.hpp"
#include "chip/io/mos6529.hpp"

// Callback type for ROM bank changes (fired when LS175 Q outputs change).
using c264_rom_bank_change_fn = void (*)(void* user_data);

class c264_io_decoder_t : public ChipBase {
public:
    c264_io_decoder_t()
        : ChipBase(ChipInfo{"C264 I/O Decoder", "Commodore",
                            "Address decoder for $FD00-$FDFF I/O block"}) {
        category_ = "Logic";
    }

    // ── Wiring — called once during system initialization ───────────────

    void wire(mos6529_t* pio1, mos6529_t* pio2) {
        pio1_ = pio1;
        pio2_ = pio2;
    }

    /// Set callback for ROM bank changes (fired when $FDD0–$FDDF write
    /// actually changes the bank pair).
    void set_bank_change_callback(c264_rom_bank_change_fn fn, void* ctx) {
        on_bank_change_      = fn;
        on_bank_change_ctx_  = ctx;
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
        rom_latch_.reset();       // Q outputs → all LOW → BASIC + KERNAL selected
        low_bank_  = 0;
        high_bank_ = 0;
    }

    // ── ROM bank queries ────────────────────────────────────────────────

    /// Current low ROM bank (0=BASIC, 1=Function LO, 2=Cartridge LO).
    uint8_t low_bank()  const { return low_bank_; }
    /// Current high ROM bank (0=KERNAL, 1=Function HI, 2=Cartridge HI).
    uint8_t high_bank() const { return high_bank_; }

    // ── Direct access to the logic chips (for debug/GUI) ────────────────

    const LS139& decoder()   const { return decoder_; }
    const LS175& rom_latch() const { return rom_latch_; }

private:
    // ── Pre-decoded I/O sub-device table ────────────────────────────────
    //
    // Compile-time equivalent of the 74LS139 + gate logic output.
    // One entry per 16-byte sub-block of $FD00–$FDFF, indexed by
    // (offset >> 4) & 0x0F.  16 bytes total — fits in a fraction of
    // one cache line, co-resident with the dispatch code in rodata.
    //
    // The LS139 and LS175 chip instances exist for debug/GUI visibility
    // but are NOT evaluated on the per-tick hot path.  This table IS
    // the precalculated decode — same principle as the C64 PLA banking
    // snapshots, but static since the I/O mapping never changes.

    enum SubDevice : uint8_t {
        kOpenBus  = 0,    // No chip responds — data bus floats
        kPio1     = 1,    // 6529B PIO1 ($FD1x) — user port / tape sense
        kPio2     = 2,    // 6529B PIO2 ($FD3x) — keyboard row select
        kRomLatch = 3,    // 74LS175 ($FDDx) — write-only ROM bank latch
    };

    //                      $FD0x     $FD1x  $FD2x     $FD3x
    //                      $FD4x     $FD5x  $FD6x     $FD7x
    //                      $FD8x     $FD9x  $FDAx     $FDBx
    //                      $FDCx     $FDDx  $FDEx     $FDFx
    static constexpr uint8_t kIoMap[16] = {
        kOpenBus, kPio1,    kOpenBus, kPio2,
        kOpenBus, kOpenBus, kOpenBus, kOpenBus,
        kOpenBus, kOpenBus, kOpenBus, kOpenBus,
        kOpenBus, kRomLatch,kOpenBus, kOpenBus,
    };

    // ── Dispatch helpers ────────────────────────────────────────────────
    //
    // Hot path: one constexpr table lookup replaces all range tests and
    // LS139 evaluation.  The resulting switch on 4 values compiles to a
    // compact jump table or 2 conditional moves — no deep branch chains.

    bus_state_t dispatch_read(bus_state_t bus) noexcept {
        uint8_t dev = kIoMap[(BUS_GET_ADDR(bus) >> 4) & 0x0F];
        switch (dev) {
        case kPio1: return pio1_->on_bus_read(bus);
        case kPio2: return pio2_->on_bus_read(bus);
        default:    return bus;  // open bus (including ROM latch reads)
        }
    }

    bus_state_t dispatch_write(bus_state_t bus) noexcept {
        uint8_t dev = kIoMap[(BUS_GET_ADDR(bus) >> 4) & 0x0F];
        switch (dev) {
        case kPio1:     return pio1_->on_bus_write(bus);
        case kPio2:     return pio2_->on_bus_write(bus);
        case kRomLatch: return latch_rom_bank(bus);
        default:        return bus;  // open bus
        }
    }

    /// Clock the 74LS175 with address bits from a $FDDx write.
    bus_state_t latch_rom_bank(bus_state_t bus) noexcept {
        // In hardware, address bits A3:A0 feed the LS175 D-inputs and the
        // decoder output clocks the latch.  The data byte is irrelevant.
        uint8_t nibble = BUS_GET_ADDR(bus) & 0x0F;
        rom_latch_.clock_pulse(nibble);

        uint8_t new_low  = nibble & 0x03;          // A1:A0
        uint8_t new_high = (nibble >> 2) & 0x03;   // A3:A2

        if (new_low != low_bank_ || new_high != high_bank_) {
            low_bank_  = new_low;
            high_bank_ = new_high;
            if (on_bank_change_) on_bank_change_(on_bank_change_ctx_);
        }
        return bus;
    }

    LS139       decoder_;           // 74LS139 (U20) — I/O sub-page decode
    LS175       rom_latch_;         // 74LS175 (U21) — ROM bank select latch
    mos6529_t*  pio1_ = nullptr;    // MOS 6529B PIO1 (non-owning)
    mos6529_t*  pio2_ = nullptr;    // MOS 6529B PIO2 (non-owning)

    // ROM bank state (mirrored from LS175 Q outputs for fast access)
    uint8_t low_bank_  = 0;        // Q1:Q0 → 0=BASIC, 1=Function LO, 2=Cart LO
    uint8_t high_bank_ = 0;        // Q3:Q2 → 0=KERNAL, 1=Function HI, 2=Cart HI

    // Banking change callback
    c264_rom_bank_change_fn on_bank_change_     = nullptr;
    void*                   on_bank_change_ctx_ = nullptr;
};
