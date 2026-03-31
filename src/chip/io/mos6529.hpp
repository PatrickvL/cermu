#pragma once

// ============================================================================
// MOS 6529B — Single Port Interface (SPI), an 8-Bit Bidirectional I/O Port
// ============================================================================
//
// Also known as PIO (Programmable/Peripheral Input/Output) in Commodore
// documentation and schematics.
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
// Package: 20-pin DIP
//
// References: MOS 6529B datasheet, VICE plus4pio1.c / plus4pio2.c

#include "chip/io/io_chip_base.hpp"
#include "core/chip_debug_registry.hpp"

// ── DECL register map ───────────────────────────────────────────────────────
// The 6529 has a single 8-bit register — the output latch.
#define MOS6529_DECL(REG, FLD, CMP)                                            \
    REG(0x00, PORT, "I/O port latch")                                          \
    FLD(PORT, P0, 0:0, "Port bit 0", Flag, 0, 0)                              \
    FLD(PORT, P1, 1:1, "Port bit 1", Flag, 0, 0)                              \
    FLD(PORT, P2, 2:2, "Port bit 2", Flag, 0, 0)                              \
    FLD(PORT, P3, 3:3, "Port bit 3", Flag, 0, 0)                              \
    FLD(PORT, P4, 4:4, "Port bit 4", Flag, 0, 0)                              \
    FLD(PORT, P5, 5:5, "Port bit 5", Flag, 0, 0)                              \
    FLD(PORT, P6, 6:6, "Port bit 6", Flag, 0, 0)                              \
    FLD(PORT, P7, 7:7, "Port bit 7", Flag, 0, 0)

namespace mos6529 { namespace reg {
MOS6529_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} } // namespace mos6529::reg

DECL_EXTRACT(MOS6529, MOS6529_DECL)

struct mos6529_t : public IoChipBase {

    mos6529_t() : IoChipBase(ChipInfo{"6529B", "MOS Technology", "MOS 6529B"}) {
        init_regs(MOS6529_NUM_REGS);
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(MOS6529_REG_INFO);
        debug_registry_.set_decl_entries(MOS6529_DECL_ENTRIES.data(), MOS6529_DECL_ENTRIES.size());
        register_debug_fields();
#endif
    }

    // ── ChipBase MMIO interface ─────────────────────────────────────────

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        BUS_SET_DATA(bus, output_latch & external_pins);
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        output_latch = BUS_GET_DATA(bus);
        regs_[mos6529::reg::PORT] = output_latch;
        return bus;
    }

    // --- CS-tick: MMIO self-dispatch ---
    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            bus = BUS_GET_BIT(bus, BUS_RW_BIT)
                ? on_bus_read(bus) : on_bus_write(bus);
            mark_cs_serviced(bus);
        }
        return bus;
    }

    void reset() override {
        output_latch  = 0xFF;
        external_pins = 0xFF;
        regs_[mos6529::reg::PORT] = 0xFF;
    }

    // Layout virtuals — defined in mos6529_gui.cpp (GUI builds only)
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // ── Public state ────────────────────────────────────────────────────

    uint8_t output_latch  = 0xFF;   // Written by CPU, driven onto port pins
    uint8_t external_pins = 0xFF;   // Externally pulled pin state (ANDed with latch on read)

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using S = const mos6529_t;
        debug_registry_
            .category("6529B I/O Port")
            .value("Output Latch", +[](const ChipBase* c) -> uint32_t {
                return static_cast<S*>(c)->output_latch;
            }, 8)
            .value("External Pins", +[](const ChipBase* c) -> uint32_t {
                return static_cast<S*>(c)->external_pins;
            }, 8)
            .value("Effective (read)", +[](const ChipBase* c) -> uint32_t {
                auto* s = static_cast<S*>(c);
                return s->output_latch & s->external_pins;
            }, 8);
    }
#endif
};
