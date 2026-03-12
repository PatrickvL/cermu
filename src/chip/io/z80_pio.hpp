#pragma once
/*
 * z80_pio.h — Zilog Z80 PIO (Parallel Input/Output Controller)
 *
 * The Z80 PIO (1977) provides two independent 8-bit parallel I/O ports
 * with handshaking and interrupt capability.  It connects directly to the
 * Z80 CPU's interrupt daisy chain.
 *
 * DDR clone: U855 (VEB MME Erfurt)
 *
 * Modes per port:
 *   Mode 0: Output — all 8 bits are outputs
 *   Mode 1: Input  — all 8 bits are inputs
 *   Mode 2: Bidirectional — Port A only; Port B provides handshake
 *   Mode 3: Bit control — each bit individually input or output
 *
 * Register access (directly via IORQ + port address, active-low accent):
 *   A0=0, A1=0 → Port A data
 *   A0=0, A1=1 → Port B data
 *   A0=1, A1=0 → Port A control
 *   A0=1, A1=1 → Port B control
 *
 * Used in: KC85 series, Z9001/KC87, Z1013 (active PIO), many Z80 systems.
 *
 * 40-pin DIP package.
 */

#include "chip/io/io_chip_base.hpp"
#include "core/system_lines.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// Z80 PIO UNIFIED DECLARATION TABLE — single source of truth
// ============================================================================

// REG(offset, symbol, description)
#define Z80_PIO_DECL(REG, FLD, CMP) \
    REG(0x00, PORT_A_DATA,  "Port A data")            \
    REG(0x01, PORT_B_DATA,  "Port B data")            \
    REG(0x02, PORT_A_CTRL,  "Port A control")         \
    REG(0x03, PORT_B_CTRL,  "Port B control")         \
    REG(0x04, PORT_A_IOSEL, "Port A I/O select")      \
    REG(0x05, PORT_B_IOSEL, "Port B I/O select")      \
    REG(0x06, PORT_A_IVEC,  "Port A interrupt vec")    \
    REG(0x07, PORT_B_IVEC,  "Port B interrupt vec")    \
    REG(0x08, PORT_A_IMASK, "Port A interrupt mask")   \
    REG(0x09, PORT_B_IMASK, "Port B interrupt mask")

namespace z80_pio_regs {
    Z80_PIO_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    constexpr uint8_t REG_COUNT = 10;
} // namespace z80_pio_regs

DECL_EXTRACT_REGS_ONLY(Z80_PIO, Z80_PIO_DECL)

// ============================================================================
// Z80 PIO Port Mode
// ============================================================================

enum class PIOMode : uint8_t {
    OUTPUT        = 0,
    INPUT         = 1,
    BIDIRECTIONAL = 2,
    BIT_CONTROL   = 3,
};

// ============================================================================
// Z80 PIO
// ============================================================================

class z80_pio_t : public IoChipBase {
public:
    explicit z80_pio_t(bool is_u855 = false)
        : IoChipBase(ChipInfo(is_u855 ? "U855" : "Z80 PIO",
                             is_u855 ? "VEB MME Erfurt" : "Zilog"))
    {
        init_regs(z80_pio_regs::REG_COUNT);
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void init() {
        for (int i = 0; i < 2; ++i) {
            port_[i].mode = PIOMode::INPUT;  // Reset to input mode
            port_[i].output = 0x00;
            port_[i].input = 0xFF;
            port_[i].io_select = 0x00;       // All inputs in bit mode
            port_[i].int_control = 0x00;
            port_[i].int_mask = 0xFF;
            port_[i].int_enabled = false;
            port_[i].int_pending = false;
            port_[i].ready = false;
            port_[i].strobe = false;
        }
        update_regs();
    }

    void reset() { init(); }

    // === Register access ===

    /// Write to port data register.
    void write_data(int port_idx, uint8_t data) {
        auto& p = port_[port_idx & 1];
        p.output = data;
        p.ready = true;
        update_regs();
    }

    /// Read port data register.
    uint8_t read_data(int port_idx) const {
        const auto& p = port_[port_idx & 1];
        if (p.mode == PIOMode::BIT_CONTROL) {
            // Bit-control: input pins from external, output pins from latch
            return (p.input & p.io_select) | (p.output & ~p.io_select);
        }
        return (p.mode == PIOMode::OUTPUT) ? p.output : p.input;
    }

    /// Write to port control register.
    void write_control(int port_idx, uint8_t data) {
        // TODO: Decode control word sequences (mode set, interrupt control,
        //       I/O select mask for mode 3, interrupt mask)
        (void)port_idx;
        (void)data;
        update_regs();
    }

    /// Set external input lines (from connected device/system).
    void set_input(int port_idx, uint8_t data) {
        port_[port_idx & 1].input = data;
    }

    /// Get current output latch value.
    uint8_t get_output(int port_idx) const {
        return port_[port_idx & 1].output;
    }

    /// Strobe signal (active-low) — triggers data transfer/interrupt.
    void strobe(int port_idx, bool active) {
        port_[port_idx & 1].strobe = active;
    }

    // === Interrupt daisy chain ===

    bool interrupt_pending(int port_idx) const {
        return port_[port_idx & 1].int_pending;
    }

    /// Return interrupt vector for the given port.
    uint8_t interrupt_vector(int port_idx) const {
        return port_[port_idx & 1].int_vector;
    }

    // === ChipBase GUI virtuals ===
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    struct Port {
        PIOMode  mode = PIOMode::INPUT;
        uint8_t  output = 0x00;
        uint8_t  input = 0xFF;
        uint8_t  io_select = 0x00;      // Mode 3: 1 = input, 0 = output
        uint8_t  int_control = 0x00;
        uint8_t  int_mask = 0xFF;
        uint8_t  int_vector = 0x00;
        bool     int_enabled = false;
        bool     int_pending = false;
        bool     ready = false;
        bool     strobe = false;
    };

    Port port_[2]{};

    // Register file mirror (flattened view for debug inspection — backed by ChipBase::regs_)

    void update_regs() {
        for (int i = 0; i < 2; ++i) {
            regs_[z80_pio_regs::PORT_A_DATA  + i] = port_[i].output;
            regs_[z80_pio_regs::PORT_A_CTRL  + i] = port_[i].int_control;
            regs_[z80_pio_regs::PORT_A_IOSEL + i] = port_[i].io_select;
            regs_[z80_pio_regs::PORT_A_IVEC  + i] = port_[i].int_vector;
            regs_[z80_pio_regs::PORT_A_IMASK + i] = port_[i].int_mask;
        }
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif
};
