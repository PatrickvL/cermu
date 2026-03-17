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

DECL_EXTRACT(Z80_PIO, Z80_PIO_DECL)

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
            port_[i].expect_io_select = false;
            port_[i].expect_int_mask = false;
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
    /// Decodes the Z80 PIO control word state machine:
    ///   - Interrupt vector:    bit 0 = 0
    ///   - Mode set:            lower nibble = 0x0F, bits 6-7 = mode
    ///   - Interrupt control:   lower nibble = 0x07
    ///   - I/O select mask:     follows mode 3 set
    ///   - Interrupt mask:      follows int control with mask-follows bit
    void write_control(int port_idx, uint8_t data) {
        auto& p = port_[port_idx & 1];

        // Pending state: expecting I/O direction mask or interrupt mask
        if (p.expect_io_select) {
            p.io_select = data;  // 1 = input, 0 = output
            p.expect_io_select = false;
            update_regs();
            return;
        }
        if (p.expect_int_mask) {
            p.int_mask = data;  // 1 = monitored, 0 = ignored
            p.expect_int_mask = false;
            update_regs();
            return;
        }

        // Interrupt vector: bit 0 = 0
        if ((data & 0x01) == 0) {
            p.int_vector = data & 0xFE;
            update_regs();
            return;
        }

        // Mode control word: lower nibble = 0x0F
        if ((data & 0x0F) == 0x0F) {
            p.mode = static_cast<PIOMode>((data >> 6) & 0x03);
            if (p.mode == PIOMode::BIT_CONTROL) {
                p.expect_io_select = true;  // Next byte is I/O direction mask
            }
            update_regs();
            return;
        }

        // Interrupt control word: lower nibble = 0x07
        if ((data & 0x0F) == 0x07) {
            p.int_enabled = (data & 0x80) != 0;
            p.int_control = data;
            if (data & 0x10) {
                p.expect_int_mask = true;  // Next byte is interrupt mask
            }
            update_regs();
            return;
        }

        // Interrupt enable/disable: lower nibble = 0x03
        if ((data & 0x0F) == 0x03) {
            p.int_enabled = (data & 0x80) != 0;
            update_regs();
            return;
        }

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

    /// Strobe signal — triggers data transfer/interrupt.
    /// On rising edge of strobe with interrupts enabled, sets int_pending.
    void strobe(int port_idx, bool active) {
        auto& p = port_[port_idx & 1];
        // Rising edge detection: trigger interrupt on transition to active
        if (active && !p.strobe && p.int_enabled) {
            p.int_pending = true;
        }
        p.strobe = active;
    }

    /// Acknowledge interrupt for the given port (called after CPU services it).
    void acknowledge_interrupt(int port_idx) {
        port_[port_idx & 1].int_pending = false;
    }

    /// Check if any port has a pending interrupt.
    bool any_interrupt_pending() const {
        return port_[0].int_pending || port_[1].int_pending;
    }

    /// Get interrupt vector for highest-priority pending port.
    /// Port A has higher priority than Port B.
    uint8_t highest_priority_vector() const {
        if (port_[0].int_pending) return port_[0].int_vector;
        if (port_[1].int_pending) return port_[1].int_vector;
        return 0xFF;
    }

    /// Get the port index that has the highest-priority pending interrupt.
    /// Returns -1 if none.
    int highest_priority_port() const {
        if (port_[0].int_pending) return 0;
        if (port_[1].int_pending) return 1;
        return -1;
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
        bool     expect_io_select = false;  // Next control write is I/O direction mask
        bool     expect_int_mask = false;   // Next control write is interrupt mask
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
