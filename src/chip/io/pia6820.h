#pragma once

#include "../../core/chip.h"
#include <cstdint>

/**
 * Motorola 6820 PIA (Peripheral Interface Adapter)
 * Used in various 6502-based and 6800-based systems
 *
 * The 6820 provides two 8-bit bidirectional data ports (A and B) with
 * handshaking capabilities via control lines CA1, CA2, CB1, CB2.
 *
 * Register Map (4 registers accessed via 2-bit address):
 *   +0: Port A Data/Direction Register (PRA/DDRA) - selected by bit 2 of CRA
 *   +1: Port A Control Register (CRA)
 *   +2: Port B Data/Direction Register (PRB/DDRB) - selected by bit 2 of CRB
 *   +3: Port B Control Register (CRB)
 *
 * Each port can be independently configured as input or output on a per-pin basis
 * using the Data Direction Register (0=input, 1=output).
 */

// ============================================================================
// PIA6820 REGISTER TABLE — single source of truth
// ============================================================================

// X(addr, symbol, description)
#define PIA_REG_TABLE(X) \
    X(0, PORTA_DATA, "Port A output latch") \
    X(1, PORTA_DDR,  "Port A direction")    \
    X(2, PORTA_CTRL, "Port A control")      \
    X(3, PORTB_DATA, "Port B output latch") \
    X(4, PORTB_DDR,  "Port B direction")    \
    X(5, PORTB_CTRL, "Port B control")

// --- Extract address constants (prefix PIA_REG_ added by macro) ---
#define PIA_X_CONST_(a, s, l) static constexpr uint8_t PIA_REG_##s = a;
PIA_REG_TABLE(PIA_X_CONST_)
#undef PIA_X_CONST_
static constexpr uint8_t PIA_NUM_REGS = 6;

// --- Extract register info array ---
#define PIA_X_INFO_(a, s, l) { #s, l },
static constexpr RegEntry PIA_REG_INFO[] = { PIA_REG_TABLE(PIA_X_INFO_) };
#undef PIA_X_INFO_

struct pia6820_t : public ChipBase {
    pia6820_t() : ChipBase(ChipInfo{"PIA6820", "Motorola"}) {
        category_ = "I/O";
#ifdef CERMU_HAS_CHIP_DEBUG
        debug_registry_.set_registers(regs_, PIA_NUM_REGS, PIA_REG_INFO);
        register_debug_fields();
#endif
    }

    // --- ChipBase GUI interface ---
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // ========================================================================
    // REGISTERS — flat array + named accessors
    // ========================================================================
    uint8_t regs_[PIA_NUM_REGS] = {};

    // Port A registers
    uint8_t& port_a_data      = regs_[PIA_REG_PORTA_DATA];   // Data register (output latch)
    uint8_t& port_a_direction  = regs_[PIA_REG_PORTA_DDR];    // Data direction register (0=input, 1=output)
    uint8_t& port_a_control   = regs_[PIA_REG_PORTA_CTRL];   // Control register

    // Port B registers
    uint8_t& port_b_data      = regs_[PIA_REG_PORTB_DATA];   // Data register (output latch)
    uint8_t& port_b_direction  = regs_[PIA_REG_PORTB_DDR];    // Data direction register
    uint8_t& port_b_control   = regs_[PIA_REG_PORTB_CTRL];   // Control register
    
    // Interrupt flags (read in bits 7-6 of control registers)
    bool irq_a1;                // CA1 interrupt flag (bit 7 of CRA)
    bool irq_a2;                // CA2 interrupt flag (bit 6 of CRA)
    bool irq_b1;                // CB1 interrupt flag (bit 7 of CRB)
    bool irq_b2;                // CB2 interrupt flag (bit 6 of CRB)
    
    // Control line states
    bool ca1_state;             // CA1 input line state
    bool ca2_state;             // CA2 input/output line state
    bool cb1_state;             // CB1 input line state
    bool cb2_state;             // CB2 input/output line state
    
    // Callbacks for I/O
    void* user_data;
    uint8_t (*on_port_a_read)(void* user_data);         // Read external port A pins
    void (*on_port_a_write)(void* user_data, uint8_t data);  // Write to port A outputs
    uint8_t (*on_port_b_read)(void* user_data);         // Read external port B pins
    void (*on_port_b_write)(void* user_data, uint8_t data);  // Write to port B outputs
    void (*on_irq_a)(void* user_data, bool asserted);   // IRQ A callback
    void (*on_irq_b)(void* user_data, bool asserted);   // IRQ B callback
    void (*on_ca2_output)(void* user_data, bool state); // CA2 output callback
    void (*on_cb2_output)(void* user_data, bool state); // CB2 output callback

    // Initialize PIA to default state
    void init();

    // Reset PIA to power-on state
    void reset();

    // Memory-mapped register access
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t data);

    // External control line inputs (for handshaking and interrupts)
    void set_ca1(bool state);
    void set_ca2_input(bool state);
    void set_cb1(bool state);
    void set_cb2_input(bool state);

    // Direct port input (bypasses callbacks, for external hardware simulation)
    void set_port_a_input(uint8_t value);
    void set_port_b_input(uint8_t value);

private:
    void update_irq();
    void update_ca2_output_state();
    void update_cb2_output_state();
    static uint8_t read_port_with_direction(uint8_t output_reg, uint8_t ddr,
                                            uint8_t (*read_cb)(void*), void* ud);

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using PI = const pia6820_t;
        debug_registry_
            .category("Port A")
            .port("Port A",
                  RegSource{PIA_REG_PORTA_DATA},
                  RegSource{PIA_REG_PORTA_DDR})
            .value("Control", PIA_REG_PORTA_CTRL)
            .flag("CA1", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->ca1_state; })
            .flag("CA2", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->ca2_state; })
            .flag("IRQ A1", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->irq_a1; })
            .flag("IRQ A2", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->irq_a2; })

            .category("Port B")
            .port("Port B",
                  RegSource{PIA_REG_PORTB_DATA},
                  RegSource{PIA_REG_PORTB_DDR})
            .value("Control", PIA_REG_PORTB_CTRL)
            .flag("CB1", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->cb1_state; })
            .flag("CB2", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->cb2_state; })
            .flag("IRQ B1", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->irq_b1; })
            .flag("IRQ B2", +[](const ChipBase* c) -> uint32_t { return static_cast<PI*>(c)->irq_b2; });
    }
#endif
};