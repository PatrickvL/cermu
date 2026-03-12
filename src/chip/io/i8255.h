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

#include "chip/io/io_chip_base.h"
#include "core/system_lines.h"
#include <cstdint>
#include <cstring>

// ============================================================================
// i8255 REGISTER TABLE — single source of truth
// ============================================================================

// REG(offset, symbol, description)
// FLD(reg_sym, field_sym, hi:lo, description, kind, display_shift, display_scale)
#define I8255_DECL(REG, FLD, CMP) \
    REG(0x00, PORT_A,  "Port A data")                                           \
    REG(0x01, PORT_B,  "Port B data")                                           \
    REG(0x02, PORT_C,  "Port C data")                                           \
    REG(0x03, CONTROL, "Mode control word")                                     \
      FLD(CONTROL, MODE_SET,   7:7, "Mode set active",       Flag,  0, 0)       \
      FLD(CONTROL, GRP_A_MODE, 6:5, "Group A mode",          Value, 0, 0)       \
      FLD(CONTROL, PA_DIR,     4:4, "Port A dir (1=in)",     Flag,  0, 0)       \
      FLD(CONTROL, PC_HI_DIR,  3:3, "Port C hi dir (1=in)",  Flag,  0, 0)       \
      FLD(CONTROL, GRP_B_MODE, 2:2, "Group B mode",          Value, 0, 0)       \
      FLD(CONTROL, PB_DIR,     1:1, "Port B dir (1=in)",     Flag,  0, 0)       \
      FLD(CONTROL, PC_LO_DIR,  0:0, "Port C lo dir (1=in)",  Flag,  0, 0)

namespace i8255_regs {
    I8255_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    constexpr uint8_t REG_COUNT = 4;
} // namespace i8255_regs

DECL_EXTRACT_ALL(I8255, I8255_DECL)

class i8255_t : public IoChipBase {
public:
    i8255_t()
        : IoChipBase(ChipInfo("8255", "Intel"))
    {
        init_regs(i8255_regs::REG_COUNT);
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void init() {
        control_ = 0x9B;  // Mode 0, all ports input (power-on default)
        port_a_out_ = 0x00;
        port_b_out_ = 0x00;
        port_c_out_ = 0x00;
        port_a_in_ = 0xFF;
        port_b_in_ = 0xFF;
        port_c_in_ = 0xFF;
        update_regs();
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
        update_regs();
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

    // --- ChipBase bus interface (MMIO) ---
    bool has_mmio() const override { return true; }
    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        if (port_b_read_callback_)
            port_b_in_ = port_b_read_callback_(port_b_read_context_, port_a_out_);
        BUS_SET_DATA(bus, read(BUS_GET_ADDR(bus)));
        return bus;
    }
    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        write(BUS_GET_ADDR(bus), BUS_GET_DATA(bus));
        return bus;
    }

    // === External port inputs from system/devices ===

    void set_port_a_input(uint8_t data) { port_a_in_ = data; }
    void set_port_b_input(uint8_t data) { port_b_in_ = data; }
    void set_port_c_input(uint8_t data) { port_c_in_ = data; }

    uint8_t get_port_a_output() const { return port_a_out_; }
    uint8_t get_port_b_output() const { return port_b_out_; }
    uint8_t get_port_c_output() const { return port_c_out_; }

    // Port B read callback — called before every register read to refresh
    // external input (e.g. keyboard matrix column data driven by Port A rows).
    // Signature: uint8_t callback(void* context, uint8_t port_a_output)
    void set_port_b_read_callback(uint8_t (*callback)(void*, uint8_t), void* context) {
        port_b_read_callback_ = callback;
        port_b_read_context_ = context;
    }

    // === Direction queries ===
    bool port_a_input() const { return (control_ & 0x10) != 0; }
    bool port_b_input() const { return (control_ & 0x02) != 0; }
    bool port_c_upper_input() const { return (control_ & 0x08) != 0; }
    bool port_c_lower_input() const { return (control_ & 0x01) != 0; }

    // === ChipBase GUI virtuals ===
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

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

    uint8_t (*port_b_read_callback_)(void*, uint8_t) = nullptr;
    void* port_b_read_context_ = nullptr;

    // Register file mirror (for debug inspection — backed by ChipBase::regs_)

    void update_regs() {
        regs_[i8255_regs::PORT_A]  = port_a_out_;
        regs_[i8255_regs::PORT_B]  = port_b_out_;
        regs_[i8255_regs::PORT_C]  = port_c_out_;
        regs_[i8255_regs::CONTROL] = control_;
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif
};
